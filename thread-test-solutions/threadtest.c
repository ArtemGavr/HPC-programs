#include <omp.h>
#include <stdio.h>

int main() {

  printf("Thread #%d reports: This program uses %d threads.\n",
         omp_get_thread_num(), omp_get_num_threads());

#pragma omp parallel
  printf("Hello world from thread #%d\n", omp_get_thread_num());

  return 0;
}
