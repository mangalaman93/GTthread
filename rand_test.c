#include <stdio.h>
#include <stdlib.h>
#include "gtthread.h"

#define SIZE 2
#define MAX_LOOP 10000000

void lower() {}

void *worker(void *in) {
  int j, i=0;
  for(i=0; i<MAX_LOOP; i++) {
    j += rand()%2;
  }
}

int main() {
  int i;
  gtthread_t t[SIZE];
  gtthread_init(5000L);

  for(i=0; i<SIZE; i++) {
    gtthread_create(&t[i], worker, (void*)i);
    printf("created worker thread with id %d\n", (int)t[i]);
  }

  for(i=0; i<SIZE; i++) {
    gtthread_join(t[i], NULL);
  }

  return 0;
}
