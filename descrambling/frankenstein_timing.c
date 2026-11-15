// Time the +1 encoder on a whole book with 1, 2, 4, ... threads.
// Usage:  ./frankenstein_timing ASCII-Frankenstein.txt [copies] [reps] > results.csv
//   copies: glue the book to itself this many times (default 1) to make the job bigger
//   reps:   how many timed repetitions per thread count (default 200)
// The table goes to stdout as CSV; notes go to stderr.
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include "read_file.h"

// The encoder: every byte goes up by one. in and out are separate buffers,
// so every repetition does exactly the same work on the same input.
static void encode(const unsigned char *in, unsigned char *out, long n)
{
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++)
        out[i] = in[i] + 1;          // unsigned char: 255 + 1 wraps to 0, still reversible
}

// Time one thread count: one warm-up call, then reps timed calls.
static void time_threads(int T, const unsigned char *in, unsigned char *out, long n,
                         int reps, double *best, double *mean)
{
    omp_set_num_threads(T);
    encode(in, out, n);              // warm-up: wakes the threads, warms the caches

    *best = 1e30;
    double sum = 0.0;
    for (int r = 0; r < reps; r++) {
        double t0 = omp_get_wtime();
        encode(in, out, n);
        double t = omp_get_wtime() - t0;
        sum += t;
        if (t < *best) *best = t;
    }
    *mean = sum / reps;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s book.txt [copies] [reps]\n", argv[0]);
        return 1;
    }
    int copies = (argc > 2) ? atoi(argv[2]) : 1;
    int reps   = (argc > 3) ? atoi(argv[3]) : 200;

    long len;
    char *book = read_file(argv[1], &len);
    long n = len * copies;

    unsigned char *in  = malloc(n);
    unsigned char *out = malloc(n);
    if (in == NULL || out == NULL) { fprintf(stderr, "out of memory\n"); return 1; }

    // Fill the buffers in parallel with the same static split the encoder uses,
    // so each thread first touches (and so owns, on a NUMA machine) its own part.
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++) {
        in[i]  = (unsigned char) book[i % len];
        out[i] = 0;
    }

    int maxT = omp_get_max_threads();
    fprintf(stderr, "%s: %ld bytes x %d copies = %ld bytes, %d reps, up to %d threads\n",
            argv[1], len, copies, n, reps, maxT);

    printf("threads,best_s,mean_s,speedup,efficiency,GB_per_s\n");
    double t1 = 0.0;
    int T = 1;                                  // 1, 2, 4, 8, ... and finally maxT
    while (1) {
        double best, mean;
        time_threads(T, in, out, n, reps, &best, &mean);
        if (T == 1) t1 = best;
        double speedup = t1 / best;
        double gbps    = 2.0 * n / best / 1e9;  // one byte read + one byte written each
        printf("%d,%.4e,%.4e,%.2f,%.3f,%.2f\n", T, best, mean, speedup, speedup / T, gbps);
        if (T == maxT) break;
        T = 2 * T;
        if (T > maxT) T = maxT;                 // e.g. maxT = 6: 1, 2, 4, then 6
    }

    // Check the answer: undo the shift and compare with the original book.
    long wrong = 0;
    for (long i = 0; i < n; i++)
        if ((unsigned char) (out[i] - 1) != in[i]) wrong++;
    fprintf(stderr, "check: %ld wrong bytes (should be 0)\n", wrong);

    free(in); free(out); free(book);
    return wrong != 0;
}
