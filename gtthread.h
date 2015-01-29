#ifndef __GTTHREAD_H
#define __GTTHREAD_H

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

/************************** ASSUMPTIONS AND INVARIANT *************************
-> One GTthread can only wait for one thread at a time
-> Child GTthread keeps running even when parent thread finishes its execution
-> The pointer to the thread would be present in either the `queue` or in one
    of the list of waiting_threads for a thread or in the list of dead threads.
    We perform a linear search operation to find a thread whenever referenced
    using int identifier
-> The first node of the linked list (queue) is the thread running currently
-> No signals in GTthread code
-> Not too small context switch interval
******************************************************************************/

/************************** DATA STRUCTURES **********************************/
typedef unsigned long gtthread_t;

/* linked list node */
typedef struct Node {
  struct Node *next;
  struct gtthreadint_t *thread;
} Node;

/* internal GTthread data structure */
typedef struct gtthreadint_t {
	gtthread_t id;
  void* return_value;
  struct Node *waiting_threads;
  ucontext_t context;
  int alive;
} gtthreadint_t;

typedef struct MutexNode {
  struct MutexNode *next;
  struct gtthread_mutex_t *mutex;
} MutexNode;

typedef struct gtthread_mutex_t {
  struct Node *cur_thread;
  struct Node *waiting_threads;
} gtthread_mutex_t;

typedef struct routine_t {
  void* (*routine)(void *);
  void* args;
} routine_t;


/****************************** GTthread Interface ***************************/
/* Must be called before any of the below functions. Failure to do so may
 * result in undefined behavior. 'period' is the scheduling quantum (interval)
 * in microseconds (i.e., 1/1000000 sec.). */
void gtthread_init(long period);

/* see man pthread_create(3); the attr parameter is omitted, and this should
 * behave as if attr was NULL (i.e., default attributes) */
int gtthread_create(gtthread_t *thread,
  void *(*start_routine)(void *), void *arg);

/* see man pthread_join(3) */
int gtthread_join(gtthread_t thread, void **status);

/* gtthread_detach() does not need to be implemented; all threads should be
 * joinable */

/* see man pthread_exit(3) */
void gtthread_exit(void *retval);

/* see man sched_yield(2) */
int gtthread_yield(void);

/* see man pthread_equal(3) */
int  gtthread_equal(gtthread_t t1, gtthread_t t2);

/* see man pthread_cancel(3); but deferred cancelation does not need to be
 * implemented; all threads are canceled immediately */
int gtthread_cancel(gtthread_t thread);

/* see man pthread_self(3) */
gtthread_t gtthread_self(void);


/************************** GTthread Mutex Interface *************************/
/* see man pthread_mutex(3); except init does not have the mutexattr parameter,
 * and should behave as if mutexattr is NULL (i.e., default attributes); also,
 * static initializers do not need to be implemented */
int gtthread_mutex_init(gtthread_mutex_t *mutex);
int gtthread_mutex_lock(gtthread_mutex_t *mutex);
int gtthread_mutex_unlock(gtthread_mutex_t *mutex);

/* gtthread_mutex_destroy() and gtthread_mutex_trylock() do not need to be
 * implemented */

#endif
