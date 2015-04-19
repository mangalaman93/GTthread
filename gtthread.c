#include "gtthread.h"

/************************** GLOBAL DATA **************************************/
/* maintains a list of runnable threads */
Node *queue;
Node *tail;
/* maintains a list of dead threads */
Node *dead_queue;
/* last allocated thread id in order to avoid thread identifier conflicts */
long last_allocated_thread_id;
/* min time slice allocated to each thread */
long interval;
/* mutex linked list */
MutexNode *mutex_queue;


/************************ INTERNAL FUNCTIONS *********************************/
void set_preempt_timer(long);

/* disable SIGVTALRM signal */
void disable_alarm() {
  sigset_t sigset;
  assert(!sigemptyset(&sigset));
  assert(!sigaddset(&sigset, SIGVTALRM));
  assert(!sigprocmask(SIG_BLOCK, &sigset, NULL));
  return;
}

/* enable SIGVTALRM signal */
void enable_alarm() {
  sigset_t sigset;
  assert(!sigemptyset(&sigset));
  assert(!sigaddset(&sigset, SIGVTALRM));
  assert(!sigprocmask(SIG_UNBLOCK, &sigset, NULL));
  return;
}

/* preemptions of currently running thread */
void timer_handler(int signum) {
  /* check whether context switch is inside the user code */
  #if __x86_64__ || __ppc64__
    printf("IP: %lx\n", queue->thread->context.uc_mcontext.gregs[REG_RIP]);
  #else
    printf("IP: %lx\n", queue->thread->context.uc_mcontext.gregs[REG_EIP]);
  #endif

 /* if() {
    set_preempt_timer(interval);
    return;
  } */

  /* if there is only one thread */
  if(!queue->next) {
    assert(tail == queue);

    /* start the timer again */
    set_preempt_timer(interval);
    return;
  }

  /* invariant check */
  assert(tail != queue);

  /* put the currently running threads at the end of the queue */
  tail->next = queue;
  queue = queue->next;
  tail = tail->next;
  tail->next = NULL;

  /* start the timer again */
  set_preempt_timer(interval);

  /* swap context */
  swapcontext(&tail->thread->context, &queue->thread->context);
  return;
}

/* sets the preemption timer */
void set_preempt_timer(long tinterval) {
  struct sigaction sa;
  struct itimerval timer;

  /* Install timer_handler as the signal handler for SIGVTALRM */
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = &timer_handler;
  sigaction(SIGVTALRM, &sa, NULL);

  if(tinterval >= 1000000) {
    timer.it_value.tv_sec = tinterval/1000000;
    timer.it_value.tv_usec = tinterval%1000000;
  } else {
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = tinterval;
  }

  timer.it_interval.tv_usec = 0;
  timer.it_interval.tv_sec = 0;
  setitimer(ITIMER_VIRTUAL, &timer, NULL);

  return;
}

/* linear search through the queue to find the thread with the given id */
Node* search_thread(gtthread_t id) {
  MutexNode *mutex_cur;
  Node *cur_waiting;
  Node *cur;

  /* search in current runnable threads */
  cur = queue;
  while(cur) {
    if(cur->thread->id == id) {
      return cur;
    }

    /* search in waiting threads */
    cur_waiting = cur->thread->waiting_threads;
    while(cur_waiting) {
      if(cur_waiting->thread->id == id) {
        return cur_waiting;
      }

      cur_waiting = cur_waiting->next;
    }

    cur = cur->next;
  }

  /* search in dead threads */
  cur = dead_queue;
  while(cur) {
    if(cur->thread->id == id) {
      return cur;
    }

    cur = cur->next;
  }

  /* search in mutex blocked thread */
  mutex_cur = mutex_queue;
  while(mutex_cur) {
    cur_waiting = mutex_cur->mutex->waiting_threads;
    while(cur_waiting) {
      if(cur_waiting->thread->id == id) {
        return cur_waiting;
      }

      cur_waiting = cur_waiting->next;
    }

    mutex_cur = mutex_cur->next;
  }

  printf("SHOULD NOT REACH HERE @line %d in file %s\n", __LINE__, __FILE__);
  assert(0);
}

/* wrapper around the original thread routine so that
   -return value can be captured
   -argument type can be along the lines of the specification
     in the assignment description
*/
void start_routine_wrapper(int arg) {
  enable_alarm();

  void* return_value;
  routine_t *r;

  /* casting back to routine_t type */
  r = (routine_t*)arg;

  /* call original function */
  return_value = (*r->routine)(r->args);

  /* call gtthread_exit explicitly
     will come here only if gtthread_exit
     is not already called in the routine */
  gtthread_exit(return_value);
}


/************************** GTthread API *************************************/
/* initialize data structures */
void gtthread_init(long period) {
  last_allocated_thread_id = 0;
  interval = period;

  /* mutex init */
  mutex_queue = NULL;

  /* record the details of main thread */
  queue = malloc(sizeof(Node));
  tail = queue;
  queue->next = NULL;
  queue->thread = malloc(sizeof(gtthreadint_t));
  queue->thread->id = 0; /* main thread has id 0 */
  queue->thread->return_value = NULL;
  queue->thread->waiting_threads = NULL;
  queue->thread->alive = 1;

  /* dead queue init */
  dead_queue = NULL;

  /* initialize the timer interrupt for preemption */
  set_preempt_timer(interval);
}

/* see man pthread_create(3); the attr parameter is omitted, and this should
 * behave as if attr was NULL (i.e., default attributes) */
int gtthread_create(gtthread_t *thread, void *(*start_routine)(void *), void *arg) {
  Node* temp_tail;
  routine_t *r;

  /* checking inputs */
  assert(thread);
  assert(start_routine);
  assert(queue);
  assert(tail);

  /* disabling preemption interrupt */
  disable_alarm();

  /* generating a random thread id */
  srand(time(NULL));
  *thread = last_allocated_thread_id + 1 + (rand()%100);

  /* adding thread to end of list of runnable threads */
  tail->next = malloc(sizeof(Node));
  temp_tail = tail;
  tail = tail->next;
  tail->next = NULL;
  tail->thread = malloc(sizeof(gtthreadint_t));
  tail->thread->id = *thread;
  tail->thread->return_value = NULL;
  tail->thread->waiting_threads = NULL;
  tail->thread->alive = 1;
  if(getcontext(&tail->thread->context) != 0) {
    /* reverting back */
    *thread = -1;
    free(tail->thread);
    free(tail);
    tail = NULL;
    tail = temp_tail;
    enable_alarm();
    return -1;
  }

  /* stack allocation */
  tail->thread->context.uc_link = NULL;
  tail->thread->context.uc_stack.ss_sp = malloc(SIGSTKSZ);
  tail->thread->context.uc_stack.ss_size = SIGSTKSZ;

  /* we have to change following things in the context we obtained using getcontext
   * to create new context for a new thread-
   *  ->stack pointer (allocation)
   *  ->program counter (initialize with new function) */
  r = (routine_t*) malloc(sizeof(routine_t));
  r->routine = start_routine;
  r->args = arg;

  /* As arugment to the function should be only int type if we want to use
   * makecontext, but we are asked to implement void*. We pass it anyway
   * in makecontext function but make sure that value of a pointer on this
   * architecture is no bigger than after cast to int type */
  assert(r == (int)r);

  /* creating context */
  makecontext(&tail->thread->context, (void (*)(void))start_routine_wrapper, 1, (int)r);
  last_allocated_thread_id = *thread;

  /* restoring the timer signal again */
  enable_alarm();
  return 0;
}

/* see man pthread_join(3) */
int gtthread_join(gtthread_t thread, void **status) {
  Node *temp, *join_node;

  /* if trying to join to itself */
  if(queue->thread->id == thread) {
    return -1;
  }

  /* disable ALARM signal */
  disable_alarm();

  /* find thread with id=thread */
  join_node = search_thread(thread);

  /* if the thread has exit already */
  if(join_node->thread->alive == 0) {
    if(status) {
      *status = join_node->thread->return_value;
    }

    enable_alarm();
    return 0;
  }

  /* invariant check
   *  -there has to be at least one more thread in the queue */
  if(!queue->next){
    printf("Something is wrong! exiting!\n");
    exit(0);
  }

  /* remove current thread from list of runnable threads and put
   * it at the front of the list of waiting threads for the thread */
  temp = queue;
  queue = queue->next;
  temp->next = join_node->thread->waiting_threads;
  join_node->thread->waiting_threads = temp;

  /* restart the timer */
  set_preempt_timer(interval);
  enable_alarm();

  /* switch context to next thread */
  if(swapcontext(&join_node->thread->waiting_threads->thread->context,
      &queue->thread->context) == -1) {
    return -1;
  }

  /* set value of status before return */
  if(status) {
    *status = join_node->thread->return_value;
  }

  assert(join_node->thread->alive == 0);
  return 0;
}

/* see man pthread_exit(3) */
void gtthread_exit(void *retval) {
  Node *cur;

  /* disable ALARM signal */
  set_preempt_timer(0);

  /* set the return value */
  queue->thread->return_value = retval;
  queue->thread->alive = 0;

  /* free data structures (memory) */
  if(queue->thread->id != 0) {
    free(queue->thread->context.uc_stack.ss_sp);
    queue->thread->context.uc_stack.ss_sp = NULL;
  }

  /* put the waiting threads in the runnable queue */
  cur = queue->thread->waiting_threads;
  if(cur) {
    tail->next = cur;

    do {
      tail = cur;
      cur = cur->next;
    } while(cur);
  }
  queue->thread->waiting_threads = NULL;

  /* keep it in the list of dead threads */
  cur = queue;
  queue = queue->next;
  cur->next = dead_queue;
  dead_queue = cur;

  /* enable ALARM signal */
  set_preempt_timer(interval);

  /* Check if there are no more threads */
  if(queue) {
    /* set a new context */
    setcontext(&queue->thread->context);
  } else {
    exit(0);
  }
}

/* see man sched_yield(2) */
int gtthread_yield(void) {
  if(!queue->next) {
    return 0;
  }

  /* disable ALARM signal */
  set_preempt_timer(0);

  /* put the thread at the end of the queue */
  tail->next = queue;
  queue = queue->next;
  tail = tail->next;
  tail->next = NULL;

  /* enable ALARM signal */
  set_preempt_timer(interval);

  /* swap context */
  if(swapcontext(&tail->thread->context, &queue->thread->context) == -1) {
    return -1;
  }

  return 0;
}

/* see man pthread_equal(3) */
int gtthread_equal(gtthread_t t1, gtthread_t t2) {
  return t1==t2;
}

/* see man pthread_cancel(3); but deferred cancelation does not need to be
 * implemented; all threads are canceled immediately */
int gtthread_cancel(gtthread_t thread) {
  Node *cur, *prev, *cur_waiting, *prev_waiting;
  MutexNode *mutex_cur;

  /* invariant check */
  assert(queue);

  /* if canceling itself */
  if(queue->thread->id == thread) {
    gtthread_exit(((void *)(size_t) -1));
  }

  /* disable ALARM signal */
  disable_alarm();

  /* search for the thread */
  prev = NULL;
  cur = queue;
  while(cur) {
    if(cur->thread->id == thread) {
      prev->next = cur->next;
      goto SKIPDELETEDATA;
    }

    /* search in waiting threads */
    prev_waiting = NULL;
    cur_waiting = cur->thread->waiting_threads;
    while(cur_waiting) {
      if(cur_waiting->thread->id == thread) {

        /* if prev is NULL */
        if(!prev) {
          cur->thread->waiting_threads = cur_waiting->next;
        } else {
          prev_waiting->next = cur_waiting->next;
        }

        cur = cur_waiting;
        goto SKIPDELETEDATA;
      }

      prev_waiting = cur_waiting;
      cur_waiting = cur_waiting->next;
    }

    prev = cur;
    cur = cur->next;
  }

  /* search in mutex blocked thread */
  mutex_cur = mutex_queue;
  while(mutex_cur) {
    prev = NULL;
    cur = mutex_cur->mutex->waiting_threads;
    while(cur) {
      if(cur->thread->id == thread) {
        if(!prev) {
          mutex_cur->mutex->waiting_threads = cur->next;
        } else {
          prev->next = cur->next;
        }

        goto SKIPDELETEDATA;
      }

      prev = cur;
      cur = cur->next;
    }

    mutex_cur = mutex_cur->next;
  }

  /* If came here => the thread id doesn't exist */
  enable_alarm();
  return -1;

  SKIPDELETEDATA:
  /* free data structures (memory) */
  if(cur->thread->id != 0) {
    free(cur->thread->context.uc_stack.ss_sp);
    cur->thread->context.uc_stack.ss_sp = NULL;
  }

  /* put the waiting threads in the runnable queue */
  cur_waiting = cur->thread->waiting_threads;
  if(cur_waiting) {
    tail->next = cur_waiting;

    do {
      tail = cur_waiting;
      cur_waiting = cur_waiting->next;
    } while(cur_waiting);
  }
  cur->thread->waiting_threads = NULL;

  /* setting return value */
  cur->thread->return_value = ((void *)(size_t) -1);
  cur->thread->alive = 0;

  /* inserting the thread in the dead_queue */
  cur->next = dead_queue;
  dead_queue = cur;

  /* enable ALARM signal */
  enable_alarm();

  return 0;
}

/* see man pthread_self(3) */
gtthread_t gtthread_self(void) {
  return queue->thread->id;
}


/************************ GTthread MUTEX API *********************************/
int gtthread_mutex_init(gtthread_mutex_t *mutex) {
  MutexNode *mn = malloc(sizeof(MutexNode));

  /* disable ALARM signal */
  disable_alarm();

  /* putting the mutex in mutex queue */
  mn->next = mutex_queue;
  mn->mutex = mutex;
  mutex_queue = mn;

  /* enable ALARM signal */
  enable_alarm();

  /* init mutex */
  mn->mutex->cur_thread = NULL;
  mn->mutex->waiting_threads = NULL;

  return 0;
}

int gtthread_mutex_lock(gtthread_mutex_t *mutex) {
  Node *node;

  /* disable ALARM signal */
  disable_alarm();

  if(mutex->cur_thread == NULL) {
    mutex->cur_thread = queue;

    /* enable ALARM signal */
    enable_alarm();
    return 0;
  } else {
    /* enable ALARM signal */
    enable_alarm();

    /* disable ALARM signal again */
    set_preempt_timer(0);

    /* adding current thread to the list of waiting
       threads for this mutex */
    node = queue->next;
    queue->next = mutex->waiting_threads;
    mutex->waiting_threads = queue;

    /* removing the current thread from the list of
       runnable threads, blocking this thread */
    queue = node;
    if(!queue) {
      printf("I don't know what to do!!! exiting!\n");
      exit(0);
    }

    /* enable ALARM signal */
    set_preempt_timer(interval);

    /* swapcontext to another thread */
    swapcontext(&mutex->waiting_threads->thread->context,
                &queue->thread->context);
    return 0;
  }
}

int gtthread_mutex_unlock(gtthread_mutex_t *mutex) {
  Node *node;

  /* disable ALARM signal */
  disable_alarm();

  /* Invariant check */
  assert(queue->thread->id == mutex->cur_thread->thread->id);

  /* put first waiting thread in the list of runnable threads */
  node = mutex->waiting_threads;
  if(node) {
    mutex->waiting_threads = node->next;
    tail->next = node;
    node->next = NULL;
    tail = tail->next;

    /* re-assign the mutex */
    mutex->cur_thread = tail;
  } else {
    /* unlock the mutex */
    mutex->cur_thread = NULL;
  }

  /* enable ALARM signal */
  enable_alarm();

  return 0;
}
