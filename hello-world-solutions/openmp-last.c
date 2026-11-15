#include <stdio.h>
#include <omp.h>

/* Exercise: use more threads, but only the thread with the highest index says hello. */
int main(void)
{
  int nthread = omp_get_max_threads();
  int thread;
#pragma omp parallel private(thread) shared(nthread)
  {
    thread = omp_get_thread_num();
    if (thread == nthread - 1) {
      printf("Hello, World! I am thread %d of %d (the last one)\n", thread, nthread);
    }
  }
  return 0;
}
