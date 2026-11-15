#include <stdio.h>      // printf
#include <string.h>     // strlen
#include <omp.h>        // omp_get_thread_num, omp_get_num_threads, omp_get_max_threads

int main(void)
{
    char message[] = "Zpv!uijol!zpvs!qbjo!boe!zpvs!ifbsucsfbl!bsf!voqsfdfefoufe"
                     "!jo!uif!ijtupsz!pg!uif!xpsme-!cvu!uifo!zpv!sfbe/";
    int n = strlen(message);

    // Each character can be decoded on its own, so the loop is split between
    // the threads. Every thread gets its own copy of i, before and after.
    #pragma omp parallel for
    for (int i = 0; i < n; i++) {
        char before = message[i];
        char after  = before - 1;
        message[i]  = after;
        printf("thread %2d of %2d decoded message[%3d]: '%c' -> '%c'\n",
               omp_get_thread_num(), omp_get_num_threads(), i, before, after);
    }
    // The parallel region has ended here, so all threads have finished.

    printf("\nDecoded with up to %d threads:\n%s\n", omp_get_max_threads(), message);
    return 0;
}
