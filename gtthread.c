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


/************************ INTERNAL FUNCTIONS *********************************/
void set_preempt_timer();

/* disable SIGVTALRM signal */
void disable_alarm(sigset_t* orig_mask) {
  sigset_t sigset;

  assert(orig_mask);
  assert(!sigemptyset(&sigset));
  assert(!sigaddset(&sigset, SIGVTALRM));
  assert(!sigprocmask(SIG_BLOCK, &sigset, orig_mask));
  return;
}

/* enable SIGVTALRM signal */
void enable_alarm(sigset_t *orig_mask) {
  assert(orig_mask);
  assert(!sigprocmask(SIG_SETMASK, orig_mask, NULL));
  return;
}

/* preemptions of currently running thread */
void timer_handler(int signum) {
  /* if there is only one thread */
  if(!queue->next) {
    assert(tail == queue);

    /* start the timer again */
    set_preempt_timer();
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
  set_preempt_timer();

  /* swap context */
  swapcontext(&tail->thread->context, &queue->thread->context);
  return;
}

/* sets the preemption timer */
void set_preempt_timer() {
  struct sigaction sa;
  struct itimerval timer;

  /* Install timer_handler as the signal handler for SIGVTALRM */
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = &timer_handler;
  sigaction(SIGVTALRM, &sa, NULL);

  if(interval >= 1000000) {
    timer.it_value.tv_sec = interval/1000000;
    timer.it_value.tv_usec = interval%1000000;
  } else {
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = interval;
  }

  timer.it_interval.tv_usec = 0;
  timer.it_interval.tv_sec = 0;
  setitimer(ITIMER_VIRTUAL, &timer, NULL);
  return;
}

/* linear search through the queue to find the thread with the given id */
Node* search_thread(gtthread_t id) {
  Node* cur_waiting;
  Node *cur = queue;

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
  }

  printf("SHOULD NOT REACH HERE @line %d in file %s\n", __LINE__, __FILE__);
  assert(0);
}


/************************** GTthread API *************************************/
/* initialize data structures */
void gtthread_init(long period) {
  last_allocated_thread_id = 0;
  interval = period;

  /* record the details of main thread */
  queue = malloc(sizeof(Node));
  tail = queue;
  queue->next = NULL;
  queue->thread = malloc(sizeof(gtthreadint_t));
  queue->thread->id = 0; /* main thread has id 0 */
  queue->thread->return_value = NULL;
  queue->thread->waiting_threads = NULL;
  queue->thread->alive = 1;
  /* not really required but just to initialize with something */
  assert(getcontext(&(queue->thread->context)) == 0);

  /* initialize the timer interrupt for preemption */
  set_preempt_timer();
}

/* see man pthread_create(3); the attr parameter is omitted, and this should
 * behave as if attr was NULL (i.e., default attributes) */
int gtthread_create(gtthread_t *thread, void *(*start_routine)(void *),
    void *arg) {
  sigset_t mask;
  Node* temp_tail;

  /* checking inputs */
  assert(thread);
  assert(start_routine);
  assert(queue);
  assert(tail);

  /* disabling preemption interrupt */
  disable_alarm(&mask);

  /* generating a random thread id */
  srand(time(NULL));
  *thread = last_allocated_thread_id + (rand()%100);

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
    tail = temp_tail;
    enable_alarm(&mask);
    return -1;
  }

  /* stack allocation */
  /* TODO: setup a return context */
  tail->thread->context.uc_link = NULL;
  tail->thread->context.uc_stack.ss_sp = malloc(SIGSTKSZ);
  tail->thread->context.uc_stack.ss_size = SIGSTKSZ;

  /* As arugment to the function should be only int type if we want to use
   * makecontext, but we are asked to implement void*. We pass it anyway
   * in makecontext function but make sure that size of a pointer on this
   * architecture is no bigger than size of int type */
  /* TODO: assert(sizeof(int) >= sizeof(void*)); */

  /* we have to change following things in the context we obtained using getcontext
   * to create new context for a new thread-
   *  ->stack pointer (allocation)
   *  ->program counter (initialize with new function) */
  makecontext(&tail->thread->context, (void (*)(void))start_routine, 1, arg);
  last_allocated_thread_id = *thread;

  /* restoring the timer signal again */
  enable_alarm(&mask);
  return 0;
}

/* see man pthread_join(3) */
int gtthread_join(gtthread_t thread, void **status) {
  sigset_t mask;
  Node *temp, *join_node;

  /* disable ALARM signal */
  disable_alarm(&mask);

  /* find thread with id=thread */
  join_node = search_thread(thread);

  /* if the thread has exit already */
  if(join_node->thread->alive == 0) {
    *status = join_node->thread->return_value;
    return 0;
  }

  /* invariant check
   *  -there has to be at least one more thread in the queue
   *  -status cannot be NULL */
  assert(queue->next);
  assert(*status);

  /* remove current thread from list of runnable threads and
   * put it at the front of the list of waiting threads for
   * the thread */
  temp = queue;
  queue = queue->next;
  temp->next = join_node->thread->waiting_threads;
  join_node->thread->waiting_threads = temp;

  /* enable ALARM signal */
  enable_alarm(&mask);

  /* switch context to next thread */
  swapcontext(&tail->thread->context, &queue->thread->context);

  /* set value of status before return */
  *status = join_node->thread->return_value;
  assert(join_node->thread->alive == 0);
  return 0;
}

/* see man pthread_exit(3) */
void gtthread_exit(void *retval) {
  Node *cur;
  sigset_t mask;

  /* disable ALARM signal */
  disable_alarm(&mask);

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

  /* keep it in the list of dead threads */
  cur = queue;
  queue = queue->next;
  cur->next = dead_queue;
  dead_queue = cur;

  /* enable ALARM signal */
  enable_alarm(&mask);

  /* swap context */
  swapcontext(NULL, &queue->thread->context);
}

/* see man sched_yield(2) */
int gtthread_yield(void) {
  sigset_t mask;

  if(!queue->next) {
    return 0;
  }

  /* disable ALARM signal */
  disable_alarm(&mask);

  /* put the thread at the end of the queue */
  tail->next = queue;
  tail = queue;
  tail->next = NULL;
  queue = queue->next;

  /* enable ALARM signal */
  enable_alarm(&mask);

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
  sigset_t mask;

  /* invariant check */
  assert(queue);

  /* disable ALARM signal */
  disable_alarm(&mask);

  /* search for the thread.
     Possible cases:
   *  -first element of the linked list
   *  -middle element of the linked list
   *  -last element of the linked list
   */
  prev = NULL;
  cur = queue;
  while(cur) {
    if(cur->thread->id == thread) {
      goto DELETEDATA;
    }

    /* search in waiting threads */
    prev_waiting = NULL;
    cur_waiting = cur->thread->waiting_threads;
    while(cur_waiting) {
      if(cur_waiting->thread->id == thread) {
        prev = prev_waiting;
        cur = cur_waiting;
        goto DELETEDATA;
      }

      prev_waiting = cur_waiting;
      cur_waiting = cur_waiting->next;
    }

    prev = cur;
    cur = cur->next;
  }

  /* delete the data structure */
  DELETEDATA: prev->next = cur->next;

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

  /* setting return value */
  cur->thread->return_value = NULL;
  cur->thread->alive = 0;

  /* inserting the thread in the dead_queue */
  cur->next = dead_queue;
  dead_queue = cur;

  /* enable ALARM signal */
  enable_alarm(&mask);

  return 0;
}

/* see man pthread_self(3) */
gtthread_t gtthread_self(void) {
  return queue->thread->id;
}


/************************ GTthread MUTEX API *********************************/
int  gtthread_mutex_init(gtthread_mutex_t *mutex);
int  gtthread_mutex_lock(gtthread_mutex_t *mutex);
int  gtthread_mutex_unlock(gtthread_mutex_t *mutex);
