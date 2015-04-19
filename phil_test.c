#include <stdio.h>
#include <stdlib.h>
#include "gtthread.h"

#define NUM_PHIL 10
#define LOOP_SIZE 100
#define MAX_SLEEP_TIME 2000000

void lower() {}
gtthread_mutex_t pfork[NUM_PHIL];

void mysleep(long unsigned num) {
  long unsigned int i=0;
  while(i<num*10) {
    i++;
  }
}

void *philosopher(void *in) {
  int i, index=(int)(in);

  for(i=0; i<LOOP_SIZE; i++) {
    /* Think */
    printf("Philosopher #%d is thinking\n", index);
    mysleep(rand()%MAX_SLEEP_TIME);

    /* acuire chopsticks */
    if(index%2 == 0) {
      printf("Philosopher #%d tries to acquire right chopstick\n", index);
      gtthread_mutex_lock(&pfork[index]);

      printf("Philosopher #%d tries to acquire left chopstick\n", index);
      gtthread_mutex_lock(&pfork[(index+1)%NUM_PHIL]);
    } else {
      printf("Philosopher #%d tries to acquire left chopstick\n", index);
      gtthread_mutex_lock(&pfork[(index+1)%NUM_PHIL]);

      printf("Philosopher #%d tries to acquire right chopstick\n", index);
      gtthread_mutex_lock(&pfork[index]);
    }

    /* Eat */
    printf("philosopher #%d is eating\n", index);
    mysleep(rand()%MAX_SLEEP_TIME);

    /* release chopsticks */
    printf("Philosopher #%d releases right chopstick\n", index);
    gtthread_mutex_unlock(&pfork[index]);
    printf("Philosopher #%d releases left chopstick\n", index);
    gtthread_mutex_unlock(&pfork[(index+1)%NUM_PHIL]);
  }

  return 0;
}

int main() {
  int i;
  gtthread_t t[NUM_PHIL];
  gtthread_init(50000L);

  for(i=0; i<NUM_PHIL; i++) {
    gtthread_mutex_init(&pfork[i]);
  }

  for(i=0; i<NUM_PHIL; i++) {
    gtthread_create(&t[i], philosopher, (void*)i);
    printf("created thread with id %d\n", (int)t[i]);
  }

  for(i=0; i<NUM_PHIL; i++) {
    gtthread_join(t[i], NULL);
  }

  return 0;
}
