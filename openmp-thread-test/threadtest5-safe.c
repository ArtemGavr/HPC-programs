#include <omp.h>
#include <stdio.h>

/* Same as threadtest5.c, but warns you if the team is too small
   for a thread #5 to exist (it needs at least 6 threads: 0..5). */
int main() {

#pragma omp parallel
  {
    int id = omp_get_thread_num();
    int n  = omp_get_num_threads();

    if (id == 5) {
      printf("Thread #%d reports: This program uses %d threads.\n", id, n);
    }

    /* single = exactly one thread (whichever gets there first) runs this */
#pragma omp single
    if (n < 6) {
      printf("Only %d threads, so there is no thread #5 to report.\n", n);
    }

    printf("Hello world from thread #%d\n", id);
  }

  return 0;
}
