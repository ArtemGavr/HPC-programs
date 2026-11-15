#include <omp.h>
#include <stdio.h>

int main() {

  /* Out here only one thread exists (the "master", thread 0),
     so omp_get_num_threads() would always say 1. */

#pragma omp parallel
  {
    int id = omp_get_thread_num();

    /* Inside the parallel region the whole team exists,
       so this call now gives the real team size. */
    if (id == 5) {
      printf("Thread #%d reports: This program uses %d threads.\n",
             id, omp_get_num_threads());
    }

    printf("Hello world from thread #%d\n", id);
  }

  return 0;
}
