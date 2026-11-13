/*
 * OpenMP Array Timing (Exercise 4), serial part + both challenges.
 *
 *   C[i][j][k] = A[i][j][k] + B[i][j][k]   for all 6 loop orderings
 *   plus the 2D (ij, ji) and 1D "single index" versions.
 *
 * Usage:   ./array_timing N M
 *          N = array side length, M = number of timed repeats to average over
 *
 * Arrays live on the heap (malloc), so there is no stack limit and no need
 * for "ulimit -s unlimited", but we still index them as A[i][j][k].
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

/* ---------- timer: CPU time from clock(), fine for ONE thread ---------- */
static double now(void) { return (double)clock() / CLOCKS_PER_SEC; }

/* ---------- Welford's running mean / variance (challenge) ---------- */
typedef struct { long n; double mean, S; } stats;

static void stats_add(stats *s, double t) {
    s->n += 1;
    double d = t - s->mean;          /* distance from the OLD mean   */
    s->mean += d / s->n;             /* nudge the mean towards t     */
    s->S += d * (t - s->mean);       /* old distance x new distance  */
}
static double stats_std(stats s) { return s.n > 1 ? sqrt(s.S / (s.n - 1)) : 0.0; }

/* ---------- 3D kernels: one per loop ordering ----------
 * In C the LAST index is the one that sits next door in memory
 * (row-major): address of A[i][j][k] = i*N*N + j*N + k.
 * So the ordering whose innermost loop is k walks memory in stride 1.  */

#define ARR3(p) ((double (*)[N][N]) (p))   /* view a flat block as [N][N][N] */

static double time_ijk(int N, double *a, double *b, double *c) {
    double (*A)[N][N] = ARR3(a), (*B)[N][N] = ARR3(b), (*C)[N][N] = ARR3(c);
    double t0 = now();
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            for (int k = 0; k < N; k++)
                C[i][j][k] = A[i][j][k] + B[i][j][k];
    return now() - t0;
}
static double time_ikj(int N, double *a, double *b, double *c) {
    double (*A)[N][N] = ARR3(a), (*B)[N][N] = ARR3(b), (*C)[N][N] = ARR3(c);
    double t0 = now();
    for (int i = 0; i < N; i++)
        for (int k = 0; k < N; k++)
            for (int j = 0; j < N; j++)
                C[i][j][k] = A[i][j][k] + B[i][j][k];
    return now() - t0;
}
static double time_jik(int N, double *a, double *b, double *c) {
    double (*A)[N][N] = ARR3(a), (*B)[N][N] = ARR3(b), (*C)[N][N] = ARR3(c);
    double t0 = now();
    for (int j = 0; j < N; j++)
        for (int i = 0; i < N; i++)
            for (int k = 0; k < N; k++)
                C[i][j][k] = A[i][j][k] + B[i][j][k];
    return now() - t0;
}
static double time_jki(int N, double *a, double *b, double *c) {
    double (*A)[N][N] = ARR3(a), (*B)[N][N] = ARR3(b), (*C)[N][N] = ARR3(c);
    double t0 = now();
    for (int j = 0; j < N; j++)
        for (int k = 0; k < N; k++)
            for (int i = 0; i < N; i++)
                C[i][j][k] = A[i][j][k] + B[i][j][k];
    return now() - t0;
}
static double time_kij(int N, double *a, double *b, double *c) {
    double (*A)[N][N] = ARR3(a), (*B)[N][N] = ARR3(b), (*C)[N][N] = ARR3(c);
    double t0 = now();
    for (int k = 0; k < N; k++)
        for (int i = 0; i < N; i++)
            for (int j = 0; j < N; j++)
                C[i][j][k] = A[i][j][k] + B[i][j][k];
    return now() - t0;
}
static double time_kji(int N, double *a, double *b, double *c) {
    double (*A)[N][N] = ARR3(a), (*B)[N][N] = ARR3(b), (*C)[N][N] = ARR3(c);
    double t0 = now();
    for (int k = 0; k < N; k++)
        for (int j = 0; j < N; j++)
            for (int i = 0; i < N; i++)
                C[i][j][k] = A[i][j][k] + B[i][j][k];
    return now() - t0;
}

/* ---------- 1D challenge: one flat index, I = i*N*N + j*N + k ---------- */
static double time_1d(int N, double *a, double *b, double *c) {
    long NNN = (long)N * N * N;
    double t0 = now();
    for (long I = 0; I < NNN; I++)
        c[I] = a[I] + b[I];
    return now() - t0;
}

/* ---------- 2D versions (the starter code's, with its bugs fixed) ---------- */
#define ARR2(p) ((double (*)[N]) (p))
static double time_ij(int N, double *a, double *b, double *c) {
    double (*A)[N] = ARR2(a), (*B)[N] = ARR2(b), (*C)[N] = ARR2(c);
    double t0 = now();
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            C[i][j] = A[i][j] + B[i][j];
    return now() - t0;
}
static double time_ji(int N, double *a, double *b, double *c) {
    double (*A)[N] = ARR2(a), (*B)[N] = ARR2(b), (*C)[N] = ARR2(c);
    double t0 = now();
    for (int j = 0; j < N; j++)
        for (int i = 0; i < N; i++)
            C[i][j] = A[i][j] + B[i][j];
    return now() - t0;
}

/* ---------- driver ---------- */
typedef double (*kernel)(int, double *, double *, double *);
typedef struct { const char *name; kernel f; stats s; } test;

static int by_mean(const void *x, const void *y) {
    double a = ((const test *)x)->s.mean, b = ((const test *)y)->s.mean;
    return (a > b) - (a < b);
}

static void run(const char *title, test *t, int n, int N, int M,
                double *a, double *b, double *c, long elems) {
    double checksum = 0.0;
    for (int q = 0; q < n; q++) {
        t[q].f(N, a, b, c);                       /* warm-up call, not counted */
        for (int m = 0; m < M; m++)
            stats_add(&t[q].s, t[q].f(N, a, b, c));
        checksum += c[elems / 2];                 /* use C so the compiler can't skip the work */
    }
    qsort(t, n, sizeof *t, by_mean);
    printf("%s  (fastest first)\n", title);
    printf("  %-6s %14s %12s %10s %9s\n", "order", "mean [us]", "std [us]", "ns/elem", "x slower");
    for (int q = 0; q < n; q++)
        printf("  %-6s %14.2f %12.2f %10.3f %9.2f\n", t[q].name,
               1e6 * t[q].s.mean, 1e6 * stats_std(t[q].s),
               1e9 * t[q].s.mean / elems, t[q].s.mean / t[0].s.mean);
    printf("  (checksum %.3f)\n\n", checksum);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s N M\n", argv[0]); return 1; }
    int N = atoi(argv[1]);
    int M = atoi(argv[2]);
    long NNN = (long)N * N * N;

    /* allocate ONCE, outside every timer ("do not time the allocations") */
    double *a = malloc(NNN * sizeof *a), *b = malloc(NNN * sizeof *b), *c = malloc(NNN * sizeof *c);
    if (!a || !b || !c) { fprintf(stderr, "out of memory\n"); return 1; }
    /* fill with real numbers: untouched malloc memory is all one shared
       zero page in Linux, which would make reads look unrealistically fast */
    for (long I = 0; I < NNN; I++) { a[I] = (double)I / NNN; b[I] = 1.0 - a[I]; c[I] = 0.0; }

    printf("N = %d, M = %d, %ld elements, %.1f MiB per array\n\n",
           N, M, NNN, NNN * 8.0 / (1 << 20));

    test t3[] = { {"ijk", time_ijk}, {"ikj", time_ikj}, {"jik", time_jik},
                  {"jki", time_jki}, {"kij", time_kij}, {"kji", time_kji},
                  {"1D",  time_1d} };
    run("3D version", t3, 7, N, M, a, b, c, NNN);

    test t2[] = { {"ij", time_ij}, {"ji", time_ji} };
    run("2D version (same N, N*N elements)", t2, 2, N, M, a, b, c, (long)N * N);

    free(a); free(b); free(c);
    return 0;
}
