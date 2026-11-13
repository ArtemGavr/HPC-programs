/*
 * OpenMP Array Timing (Exercise 4), parallel part.
 *
 *   C[i][j][k] = A[i][j][k] + B[i][j][k]   in the fast ijk order, three ways:
 *     serial      : no pragma
 *     pfor        : #pragma omp parallel for            (splits only the i loop)
 *     collapse    : #pragma omp parallel for collapse(3) (splits all N^3 iterations)
 *     collapse2   : #pragma omp parallel for collapse(2) (splits the N^2 (i,j) pairs,
 *                   keeps the k loop whole so it can still be vectorised)
 *
 * For each, it prints the wall-clock time (omp_get_wtime) AND the CPU time
 * (clock) so you can see why clock() stops being the right timer, and a
 * "census" of how many threads actually got any work.
 *
 * Usage:   OMP_NUM_THREADS=8 ./array_timing_omp N M
 * Compile: gcc -O2 -fopenmp -o array_timing_omp array_timing_omp.c -lm
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <omp.h>

typedef struct { long n; double mean, S; } stats;          /* Welford */
static void stats_add(stats *s, double t) {
    s->n += 1; double d = t - s->mean; s->mean += d / s->n; s->S += d * (t - s->mean);
}
static double stats_std(stats s) { return s.n > 1 ? sqrt(s.S / (s.n - 1)) : 0.0; }

#define ARR3(p) ((double (*)[N][N]) (p))

/* ---- the three kernels ---- */
static void add_serial(int N, double *a, double *b, double *c) {
    double (*A)[N][N] = ARR3(a), (*B)[N][N] = ARR3(b), (*C)[N][N] = ARR3(c);
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            for (int k = 0; k < N; k++)
                C[i][j][k] = A[i][j][k] + B[i][j][k];
}
static void add_pfor(int N, double *a, double *b, double *c) {
    double (*A)[N][N] = ARR3(a), (*B)[N][N] = ARR3(b), (*C)[N][N] = ARR3(c);
    #pragma omp parallel for            /* N chunks of work: one per value of i */
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            for (int k = 0; k < N; k++)
                C[i][j][k] = A[i][j][k] + B[i][j][k];
}
static void add_collapse(int N, double *a, double *b, double *c) {
    double (*A)[N][N] = ARR3(a), (*B)[N][N] = ARR3(b), (*C)[N][N] = ARR3(c);
    #pragma omp parallel for collapse(3) /* N*N*N chunks: one per element */
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            for (int k = 0; k < N; k++)
                C[i][j][k] = A[i][j][k] + B[i][j][k];
}

static void add_collapse2(int N, double *a, double *b, double *c) {
    double (*A)[N][N] = ARR3(a), (*B)[N][N] = ARR3(b), (*C)[N][N] = ARR3(c);
    #pragma omp parallel for collapse(2) /* N*N chunks: one per (i,j) row */
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            for (int k = 0; k < N; k++)
                C[i][j][k] = A[i][j][k] + B[i][j][k];
}

/* ---- census: same loop shapes, but each thread counts what it did ---- */
static void census(const char *name, int N, int mode) {
    int T = omp_get_max_threads();
    long *done = calloc(T, sizeof *done);
    int team = 1;
    if (mode == 0) {                                   /* serial */
        done[0] = (long)N * N * N;
    } else if (mode == 1) {
        #pragma omp parallel for
        for (int i = 0; i < N; i++) {
            if (i == 0) team = omp_get_num_threads();
            for (int j = 0; j < N; j++)
                for (int k = 0; k < N; k++)
                    done[omp_get_thread_num()]++;      /* each thread owns its own slot */
        }
    } else if (mode == 3) {
        #pragma omp parallel for collapse(2)
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++) {
                if (i == 0 && j == 0) team = omp_get_num_threads();
                for (int k = 0; k < N; k++)
                    done[omp_get_thread_num()]++;
            }
    } else {
        #pragma omp parallel for collapse(3)
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++)
                for (int k = 0; k < N; k++) {
                    if (i == 0 && j == 0 && k == 0) team = omp_get_num_threads();
                    done[omp_get_thread_num()]++;
                }
    }
    int busy = 0; long lo = -1, hi = 0;
    for (int t = 0; t < T; t++) if (done[t] > 0) {
        busy++; if (lo < 0 || done[t] < lo) lo = done[t]; if (done[t] > hi) hi = done[t];
    }
    printf("  %-9s team of %3d threads, %3d got work, %ld..%ld elements each\n",
           name, team, busy, lo, hi);
    free(done);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s N M\n", argv[0]); return 1; }
    int N = atoi(argv[1]), M = atoi(argv[2]);
    long NNN = (long)N * N * N;
    double *a = malloc(NNN * sizeof *a), *b = malloc(NNN * sizeof *b), *c = malloc(NNN * sizeof *c);
    if (!a || !b || !c) { fprintf(stderr, "out of memory\n"); return 1; }

    /* first touch in parallel, same split as the kernels, so each thread's
       pages sit in memory close to the core that will use them (NUMA) */
    #pragma omp parallel for
    for (long I = 0; I < NNN; I++) { a[I] = (double)I / NNN; b[I] = 1.0 - a[I]; c[I] = 0.0; }

    printf("N = %d, M = %d, OMP_NUM_THREADS = %d\n", N, M, omp_get_max_threads());
    printf("Who did the work:\n");
    census("serial",   N, 0);
    census("pfor",     N, 1);
    census("collapse", N, 2);
    census("collapse2", N, 3);

    struct { const char *name; void (*f)(int, double *, double *, double *); } k[] = {
        {"serial", add_serial}, {"pfor", add_pfor}, {"collapse", add_collapse},
        {"collapse2", add_collapse2} };

    printf("Timing (mean +- std over M calls):\n");
    printf("  %-9s %14s %10s %14s %10s\n", "version", "wall [us]", "std", "clock() [us]", "clock/wall");
    double base = 0.0, checksum = 0.0;
    for (int q = 0; q < 4; q++) {
        stats w = {0}, cpu = {0};
        k[q].f(N, a, b, c);                                    /* warm-up */
        for (int m = 0; m < M; m++) {
            double  t0 = omp_get_wtime();  clock_t c0 = clock();
            k[q].f(N, a, b, c);
            double  t1 = omp_get_wtime();  clock_t c1 = clock();
            stats_add(&w,   t1 - t0);
            stats_add(&cpu, (double)(c1 - c0) / CLOCKS_PER_SEC);
        }
        checksum += c[NNN / 2];
        if (q == 0) base = w.mean;
        printf("  %-9s %14.2f %10.2f %14.2f %10.2f   speedup %.2fx\n", k[q].name,
               1e6 * w.mean, 1e6 * stats_std(w), 1e6 * cpu.mean, cpu.mean / w.mean, base / w.mean);
    }
    printf("  (checksum %.3f)\n", checksum);
    free(a); free(b); free(c);
    return 0;
}
