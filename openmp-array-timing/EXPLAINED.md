# Array Timing, explained step by step

This is the long version. `WALKTHROUGH.md` gives the answers, and this file explains *why* each answer is true and what every line of the code does. Read it in order, because each part builds on the one before.

**The whole exercise in one sentence:** adding two arrays takes the same number of additions whichever order you loop in, but the order decides how hard the computer has to work to *fetch* the numbers, and fetching is the slow part.

Contents

1. [How memory works (the theory you need first)](#1-how-memory-works)
2. [Arrays in memory and the index formula](#2-arrays-in-memory-and-the-index-formula)
3. [Loop orderings: what they are and why there are 6](#3-loop-orderings)
4. [Why the order changes the speed: caches](#4-why-the-order-changes-the-speed-caches)
5. [How to time code properly](#5-how-to-time-code-properly)
6. [The serial program: whole code, big picture, then line by line](#6-the-serial-program-whole-code-big-picture-then-line-by-line)
7. [Reading the serial output and answering the questions](#7-reading-the-serial-output)
8. [Parallel theory: threads and OpenMP](#8-parallel-theory)
9. [The parallel program: whole code, big picture, then line by line](#9-the-parallel-program-whole-code-big-picture-then-line-by-line)
10. [clock() against omp_get_wtime()](#10-clock-against-omp_get_wtime)
11. [Welford's method, step by step](#11-welfords-method)
12. [The SLURM scripts, line by line](#12-the-slurm-scripts-line-by-line)
13. [The commands to run, line by line](#13-the-commands-to-run)
14. [Glossary](#14-glossary)

---

## 1. How memory works

### 1.1 Memory is one long street of numbered boxes

Your computer's memory (RAM) is one long row of boxes. Each box holds **1 byte** and has a number, called its **address**: 0, 1, 2, 3 and so on, up to billions.

Memory has no rows, columns or grids. It is one dimension only. Every "2D" or "3D" array is really laid out along this single street. That fact is the whole exercise.

### 1.2 A `double` takes 8 boxes

A `double` is a decimal number like `3.14`. It uses **8 bytes**, so it fills 8 boxes in a row.

If an array of doubles starts at address 1000, then:

| element | starts at address |
|---|---|
| `a[0]` | 1000 |
| `a[1]` | 1008 |
| `a[2]` | 1016 |
| `a[n]` | 1000 + 8·n |

So "the next element" always means "8 bytes further along the street".

### 1.3 Worked size example

The exercise uses three arrays, each N × N × N doubles.

- N = 64 gives 64 × 64 × 64 = 262,144 doubles. Multiply by 8 bytes and you get 2,097,152 bytes, which is **2 MiB per array** and **6 MiB for all three**.
- N = 256 gives 16,777,216 doubles, which is **128 MiB per array** and **384 MiB for all three**.

These two sizes behave very differently, as you'll see in Part 4.

---

## 2. Arrays in memory and the index formula

### 2.1 A 2D array

Take `double A[3][4]`, which has 3 rows and 4 columns. C stores it **row by row**: first all of row 0, then all of row 1, then row 2.

```
position on the street:   0     1     2     3     4     5     6     7     8     9     10    11
element:               A[0][0] A[0][1] A[0][2] A[0][3] A[1][0] A[1][1] A[1][2] A[1][3] A[2][0] ...
                       |------- row 0 -------|  |------- row 1 -------|  |---- row 2 ...
```

To find where `A[i][j]` lives, you:

1. skip `i` whole rows, and each row has 4 elements, so that's `i·4` elements,
2. then step `j` more along the row.

That gives **position = i·4 + j**. For a general `A[N1][N2]` it's **I(i, j) = i·N2 + j**.

Check: `A[2][1]` is at 2·4 + 1 = **9**. Count along the diagram above and you land on position 9. ✓

This "rows one after another" layout is called **row-major**, and C, C++ and Python's NumPy (by default) all use it.

(side note: Fortran and Julia do it the other way round, column by column. That's **column-major**, with the formula `I(i, j) = i + j·N1`. The exercise sheet has these two labels swapped. It says "column-major, like C", but C is row-major. The formula `i·N2 + j` the sheet gives is correct for C, only the name is wrong.)

### 2.2 A 3D array

`double A[N1][N2][N3]` is N1 "slabs". Each slab is an N2 × N3 2D array, and the slabs are stored one after another.

To find `A[i][j][k]` you:

1. skip `i` whole slabs. Each slab holds N2·N3 elements, so that's `i·N2·N3`.
2. Inside that slab, skip `j` whole rows of N3 elements each, which is `j·N3`.
3. Step `k` more along the row.

**I(i, j, k) = i·N2·N3 + j·N3 + k**

You can write the same thing as `(i·N2 + j)·N3 + k`. That's identical, it just multiplies in a different order.

Worked example, N1 = N2 = N3 = 64: `A[1][2][3]` sits at 1·64·64 + 2·64 + 3 = 4096 + 128 + 3 = **4227**. In bytes, that's 4227 × 8 = 33,816 bytes from the start.

**That's the answer to the sheet's question "What is I(i, j, k)?".**

### 2.3 What a step of 1 in each index costs

From the formula, if you add 1 to:

- **k**: I goes up by **1**, so the very next double.
- **j**: I goes up by **N3** (64 doubles = 512 bytes away when N = 64).
- **i**: I goes up by **N2·N3** (4096 doubles = 32 KB away when N = 64).

The size of this jump is called the **stride**. **k has stride 1**, which is the cheapest move you can make in memory. Keep this table in mind, because everything else follows from it.

---

## 3. Loop orderings

### 3.1 What "loop ordering" means

To visit every element you need three nested loops, one for each index. You choose which index goes in the **outer** loop, which in the **middle** and which in the **inner** loop. The inner loop changes fastest.

Here's "ijk" (i outer, j middle, k inner):

```c
for (int i = 0; i < N; i++)          // outer: changes slowest
    for (int j = 0; j < N; j++)      // middle
        for (int k = 0; k < N; k++)  // inner: changes every single step
            C[i][j][k] = A[i][j][k] + B[i][j][k];
```

And here's "kji" (k outer, j middle, i inner). The loop body is exactly the same line:

```c
for (int k = 0; k < N; k++)
    for (int j = 0; j < N; j++)
        for (int i = 0; i < N; i++)
            C[i][j][k] = A[i][j][k] + B[i][j][k];
```

Both versions do the same N³ additions and get the same answer. Only the **order of visits** is different.

### 3.2 How many orderings?

You have 3 choices for the outer index, then 2 left for the middle, then 1 for the inner. That's 3 × 2 × 1 = **3! = 6**:

**ijk, ikj, jik, jki, kij, kji**

That's the sheet's first answer.

### 3.3 Watch the order of visits with a tiny array

Take N = 2, which gives 8 elements at street positions 0 to 7. I ran a script that lists the positions each ordering visits:

```
ijk  visits  0, 1, 2, 3, 4, 5, 6, 7     ← walks straight down the street
ikj  visits  0, 2, 1, 3, 4, 6, 5, 7     ← small hops back and forth
kji  visits  0, 4, 2, 6, 1, 5, 3, 7     ← big jumps all over the place
```

ijk reads memory **in order**. kji jumps halfway along the street, then back, then forward again. With N = 2 that doesn't matter, but with N = 256 those jumps are 512 KB long.

---

## 4. Why the order changes the speed: caches

### 4.1 The CPU is fast, memory is slow

A modern core can do an addition in well under a nanosecond. Fetching a number from RAM takes **around 100 nanoseconds**. If the CPU had to go to RAM for every number, it would spend more than 99% of its time waiting.

The fix is a **cache**: a small, very fast memory right next to the core that keeps copies of recently used data. There are several levels, from small and fast to big and slower. Here are the rough figures for Hamilton's AMD EPYC 7702 chip:

| level | size (EPYC 7702) | rough time to fetch | library version |
|---|---|---|---|
| L1 | 32 KB per core | ~1 ns | the book in your hand |
| L2 | 512 KB per core | ~4 ns | the trolley next to you |
| L3 | 16 MB shared by 4 cores | ~10–15 ns | the shelf behind you |
| RAM | 256 GB per node | ~100 ns | the warehouse across town |

(the latencies are typical ballpark values, not measured on Hamilton)

### 4.2 Memory is fetched in cache lines of 64 bytes

This is the key idea. The CPU **never fetches one number**. It always fetches a whole **cache line** of **64 bytes**, which is 8 doubles sitting next to each other.

Library analogy: the librarian can't carry one book. They always pick up an armful of the 8 books next to each other on the shelf.

- If your next 7 reads are the neighbours, they're **already in the armful**, so they're nearly free. Ask for 1 book, get 7 more free.
- If your next read is far away, you need a **new armful**, and 7 of the 8 books you carried are wasted.

### 4.3 Worked count: how many fetches?

Take one array at N = 256. It has 16,777,216 doubles, which is 16,777,216 / 8 = **2,097,152 cache lines**.

- **ijk** (stride 1): every armful gets fully used before you move on, so you fetch each line **once**. That's about 2.1 million fetches.
- **kji** (stride N² in the inner loop): each read lands on a different line. By the time you come back for a neighbour, that line has been pushed out of the cache, so you fetch it again. Worst case, every one of the 16.8 million reads is a fresh fetch, which is **8× more fetches**.

Do that for A, B and C and the waste adds up quickly.

The measured slowdown was about 21×, which is more than 8×. Two more effects pile on top:

- **The prefetcher.** The CPU watches your reads, and when it sees you walking along in order it starts fetching the next lines *before you ask*. That only works for simple, regular patterns, and stride 1 is the easiest. With huge jumps it helps much less.
- **Vectorisation (SIMD from lecture 1).** With stride 1 the compiler can use one instruction to add 2 or 4 neighbouring doubles at once. With big strides the neighbours aren't next to each other, so it can't.

(side note: there's also the **TLB**, a small cache of where memory pages are. Huge jumps keep landing on different 4 KB pages, which adds more misses. You don't need it for the answer, but it's part of why kji is so bad.)

### 4.4 Why the ranking comes in pairs

Look at where the stride-1 index **k** sits:

| ordering | inner loop stride | middle loop stride | where k is | expected speed |
|---|---|---|---|---|
| ijk | 1 | N | inner | fast |
| jik | 1 | N² | inner | fast |
| ikj | N | 1 | middle | medium |
| jki | N² | 1 | middle | medium-slow |
| kij | N | N² | outer | slow |
| kji | N² | N | outer | slowest |

- **k inner:** every armful is used completely. Fast.
- **k in the middle:** the inner loop jumps around and touches N different lines. Then the middle loop moves k on by one, which is the neighbour in each of **those same lines**. If those N lines are still on the trolley (in cache), they're reused. So it's half good, which gives medium.
- **k outer:** you touch the entire array before k moves on by one. By then the early lines are long gone from the cache, so nothing is reused. Slow.

### 4.5 "Bandwidth-bound": the arithmetic is not the problem

For each element, the computer does **1 addition**, but it moves this much data:

- reads `A[i][j][k]`, 8 bytes
- reads `B[i][j][k]`, 8 bytes
- writes `C[i][j][k]`, 8 bytes (and usually reads C's line first before writing it, another 8 bytes)

That's **24 to 32 bytes of traffic for 1 addition**. Here's a worked example with the measured ijk speed at N = 256 (2.1 ns per element): 32 bytes / 2.1 ns ≈ **15 GB/s**. That's about the most one core can pull from RAM, while the adder could go many times faster.

A program whose speed is set by how fast memory delivers data, not by how fast the maths runs, is called **memory-bandwidth-bound**. This exercise is the classic example.

---

## 5. How to time code properly

### 5.1 The basic pattern

```c
start = read the clock;
do the work;
end   = read the clock;
time  = end - start;
```

Only the work you want to measure goes between the two clock reads. That's why the sheet says "do not time the allocations": `malloc` and filling the arrays happen **before** the clock starts.

### 5.2 Why repeat M times and average

One measurement is noisy. The operating system might interrupt, another job might be using memory, or the CPU might change its clock speed. If you repeat M times and take the **mean**, the noise averages out. The **standard deviation** (std) tells you how much the runs disagreed. A small std means you can trust the mean.

### 5.3 Why a warm-up call

The first call runs on a "cold" machine: the data isn't in cache yet, and the first time you touch some memory the OS has to set up the page. So the first call is slower and unrepresentative. My code runs every function **once without timing it**, then times the next M calls.

### 5.4 Two traps that give fake results

- **Untouched memory.** If you `malloc` an array and never write to it, Linux doesn't really give you memory yet. Every page points at one shared page full of zeros, so reading it is impossibly fast (it's always the same tiny bit of memory). The fix is to fill the arrays with real values before timing.
- **Dead code elimination.** With `-O2`, the compiler deletes work whose result nobody uses. If C is never read afterwards, the compiler can remove the whole loop and you'd time nothing. The fix is to use C afterwards. My code adds one element of C to a "checksum" and prints it.

---

## 6. The serial program: whole code, big picture, then line by line

The file is `array_timing.c`. This part comes in three steps:

1. **6.1** shows the whole program in one go, so you can see its shape before any detail.
2. **6.2** explains what it does, in plain words: what the pieces are, what order they run in, and why.
3. **6.3 onwards** goes through it function by function, line by line.

### 6.1 The whole program

Don't try to understand every line yet. Just notice the shape: some small helpers at the top, nine timing functions that mostly look the same, a function called `run`, and `main` at the bottom.

```c
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
```

### 6.2 The big picture: what the program does, and how

**What we're doing.** We measure how long `C = A + B` takes for each of the 6 loop orders, plus the 1D version and the two 2D versions. We average each one over M runs and print them **fastest first**. That one table answers "which is fastest" and "what is the ranking".

**The pieces of the program, grouped by job.** The file has 4 kinds of pieces. It helps to picture a timing contest: a stopwatch and a notebook, the contestants, a referee, and the organiser.

| kind | names in the code | what it does | in the contest |
|---|---|---|---|
| **tools** | `now()`, `stats` / `stats_add` / `stats_std`, `ARR3` / `ARR2` | read the clock, keep a running mean and std, let us write `A[i][j][k]` on a flat block | the stopwatch, the notebook, and a map of the shelves |
| **workers** | `time_ijk`, `time_ikj`, `time_jik`, `time_jki`, `time_kij`, `time_kji`, `time_1d`, `time_ij`, `time_ji` | each does the whole addition **once**, in its own loop order, and returns how long that took | the contestants. Each runs the same course a different way |
| **manager** | `run()` | for each worker: a warm-up, then M timed calls fed into the notebook. Then it sorts and prints the table | the referee |
| **boss** | `main()` | reads N and M, gets the memory, fills it, hands the list of workers to the manager, cleans up | the organiser |

**What happens, in order, when you type `./array_timing 64 200`:**

1. `main` reads **N = 64** and **M = 200** from the command line.
2. `main` asks for three blocks of memory with `malloc`, each holding 64³ = 262,144 doubles (2 MiB). **Not timed.**
3. `main` fills every element of a, b and c with real numbers. **Not timed.**
4. `main` prints a header line.
5. `main` makes a list of the 7 3D workers and gives it to `run`. For **each** worker, `run`:
   - calls it once and ignores the time (the warm-up),
   - calls it 200 more times, and puts each returned time into that worker's notebook,
   - reads one value of C into a checksum, so the compiler can't skip the work.
   Then `run` sorts the 7 workers by mean time and prints the table.
6. `main` does the same with the list of 2 workers for 2D.
7. `main` gives the memory back with `free` and ends.

Here's the same thing as a picture of who calls whom:

```
main
 ├─ read N, M from the command line
 ├─ malloc a, b, c                       (not timed)
 ├─ fill a, b, c with numbers            (not timed)
 ├─ run("3D", [ijk, ikj, jik, jki, kij, kji, 1D])
 │    ├─ for each worker:
 │    │    ├─ worker(N, a, b, c)         warm-up, time thrown away
 │    │    ├─ repeat M times:
 │    │    │    t = worker(N, a, b, c)   inside: t0 = now(); loops; return now() - t0
 │    │    │    stats_add(notebook, t)
 │    │    └─ checksum += c[middle]
 │    └─ sort fastest first, print the table
 ├─ run("2D", [ij, ji])                  same again
 └─ free a, b, c
```

**How the contest is kept fair:**

- Every worker gets **the same arrays and the same N**.
- Every worker does **exactly the same additions**. Only the order of the three `for` lines differs.
- The stopwatch runs **only around the loops**, so memory setup is never counted.
- Each worker gets the same warm-up and the same M repeats.

So any difference in the times comes from the loop order and nothing else.

**How much work that is:** 7 workers × (1 warm-up + 200 timed calls) × 262,144 elements ≈ **369 million additions** for the 3D part alone. That takes a few seconds.

Now for the line-by-line breakdown. It follows the order of the file: tools first, then the workers, then the manager, then the boss.

### 6.3 The includes

```c
#include <stdio.h>   // printf, fprintf
#include <stdlib.h>  // malloc, free, atoi, qsort
#include <time.h>    // clock, CLOCKS_PER_SEC
#include <math.h>    // sqrt
```

`#include` pastes in the declarations of library functions so the compiler knows they exist. Because of `math.h`, you add `-lm` when compiling, which links in the maths library.

### 6.4 The timer

```c
static double now(void) { return (double)clock() / CLOCKS_PER_SEC; }
```

- `clock()` returns how much **CPU time** the program has used so far, counted in "ticks".
- `CLOCKS_PER_SEC` is how many ticks make one second (1,000,000 on Linux).
- Dividing gives seconds. `(double)` turns the whole number into a decimal first, so the division doesn't round down to 0.
- `static` just means "this function is only used inside this file". You can ignore it.

So `now()` gives the CPU time in seconds as a decimal number. For **one thread**, CPU time and real time are almost the same, so it's fine here. Part 10 explains why it stops being fine with threads.

### 6.5 The statistics (Welford): the box that remembers mean and std

```c
typedef struct { long n; double mean, S; } stats;
```

A `struct` bundles several variables into one thing. This one holds:

- `n`, how many times we've seen so far,
- `mean`, the running average,
- `S`, a running total that lets us get the variance later.

`typedef ... stats;` lets us write `stats s;` instead of `struct {...} s;`.

```c
static void stats_add(stats *s, double t) {
    s->n += 1;
    double d = t - s->mean;
    s->mean += d / s->n;
    s->S += d * (t - s->mean);
}
```

This adds one new time `t`. `stats *s` is a **pointer** to the box, meaning its address, so the function can change the original box. `s->n` means "the `n` inside the box `s` points to". What each line does is explained in Part 11.

```c
static double stats_std(stats s) { return s.n > 1 ? sqrt(s.S / (s.n - 1)) : 0.0; }
```

This gives the standard deviation: √(S / (n − 1)). The `? :` is a short if/else: if we have more than 1 value, return the formula, otherwise return 0 (you can't measure spread from one value).

### 6.6 The view trick: treating a flat block as a 3D array

```c
#define ARR3(p) ((double (*)[N][N]) (p))
```

This is the hardest line in the file, so I'll go slowly.

- `malloc` (in `main`) gives back a **flat** block of N³ doubles. Its type is `double *`, which means "pointer to a double". You can only index it with one number, `a[I]`.
- We want to write `A[i][j][k]`. For that the compiler needs to know the row length (N) and slab size (N×N), so it can do the `i·N·N + j·N + k` maths for us.
- `double (*)[N][N]` is the type "pointer to an N×N block of doubles". If a pointer points to slabs of N×N, then `A[i]` means slab number i, `A[i][j]` means row j of that slab, and `A[i][j][k]` means element k of that row. The compiler works out exactly the formula from Part 2.2.
- `(type) p` is a **cast**: "take the address p and treat it as this type". The memory doesn't move or change. We only change how we label it.
- `#define ARR3(p) ...` is a **macro**: a find-and-replace the compiler does before it compiles. Everywhere you write `ARR3(a)`, it pastes `((double (*)[N][N]) (a))`. That saves typing the ugly type six times.

Analogy: the street of houses stays the same, but you hand the postman a map that says "blocks of N×N houses, rows of N houses". Now they can find "block 1, row 2, house 3" without you working out the house number.

**Why not just write `double A[N][N][N];` like the starter code?** That puts the arrays on the **stack**, a small area of memory (usually 8 MB) for local variables. At N = 64 the three arrays need 6 MiB, which only just fits. At N = 128 they need 48 MiB and the program crashes with a "segmentation fault". That's why the sheet tells you to run `ulimit -s unlimited`. `malloc` uses the **heap** instead, which can be as big as the machine's RAM, so no `ulimit` is needed.

### 6.7 One timed function per ordering

```c
static double time_ijk(int N, double *a, double *b, double *c) {
    double (*A)[N][N] = ARR3(a), (*B)[N][N] = ARR3(b), (*C)[N][N] = ARR3(c);
    double t0 = now();
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            for (int k = 0; k < N; k++)
                C[i][j][k] = A[i][j][k] + B[i][j][k];
    return now() - t0;
}
```

Line by line:

1. `static double time_ijk(int N, double *a, double *b, double *c)`: the function takes the size N and the addresses of the three flat arrays, and it returns a `double` (the time taken). That's exactly what the sheet asks for: "a small function which performs the addition and returns a double time".
2. `double (*A)[N][N] = ARR3(a), ...`: makes A, B and C as 3D views of a, b and c (Part 6.6). This costs no time, because nothing is copied.
3. `double t0 = now();`: reads the clock. **Start.**
4. The three loops in i, j, k order with the addition inside: the work.
5. `return now() - t0;`: reads the clock again and returns the difference. **Stop.**

The other five functions, `time_ikj`, `time_jik`, `time_jki`, `time_kij` and `time_kji`, are identical except that **the three `for` lines are in a different order**. The body line `C[i][j][k] = A[i][j][k] + B[i][j][k];` never changes, so all six compute exactly the same result.

### 6.8 The 1D challenge function

```c
static double time_1d(int N, double *a, double *b, double *c) {
    long NNN = (long)N * N * N;
    double t0 = now();
    for (long I = 0; I < NNN; I++)
        c[I] = a[I] + b[I];
    return now() - t0;
}
```

- This uses no 3D view at all. It's one loop over the flat arrays from 0 to N³ − 1.
- `long` is a bigger whole-number type than `int`. An `int` tops out around 2.1 billion. N³ goes past that once N gets above 1290, so `long` is the safe choice. `(long)N * N * N` converts **before** multiplying, so the multiplication itself doesn't overflow.
- It visits memory in exactly the same order as ijk (0, 1, 2, 3, ...), because ijk with k innermost **is** street order.

### 6.9 The 2D functions (the starter code, fixed)

```c
#define ARR2(p) ((double (*)[N]) (p))
static double time_ij(...)  { ... for i { for j { C[i][j] = A[i][j] + B[i][j]; } } ... }
static double time_ji(...)  { ... for j { for i { C[i][j] = A[i][j] + B[i][j]; } } ... }
```

These are the same idea in 2D. `double (*)[N]` means "pointer to rows of N doubles". `ij` has j innermost (stride 1, fast). `ji` has i innermost (stride N, slow).

They fix the starter file's bugs:

- the broken `time_ji` loop headers (`for(j=0; i<N; i++)`: it starts j, tests i and increments i, so it never runs properly),
- `main` passing M instead of N,
- `main` timing `time_ij` twice,
- arrays that were never filled in and never used.

### 6.10 The driver: a list of tests

```c
typedef double (*kernel)(int, double *, double *, double *);
typedef struct { const char *name; kernel f; stats s; } test;
```

- `kernel` is the type "**pointer to a function** that takes (int, double*, double*, double*) and returns a double". All the `time_xxx` functions have exactly that shape, so you can store any of them in a variable of type `kernel` and call it later. It's like writing a function's name on a card and filing the card.
- `test` bundles a label (like `"ijk"`), which function to call (`f`) and its statistics box (`s`).

```c
static int by_mean(const void *x, const void *y) {
    double a = ((const test *)x)->s.mean, b = ((const test *)y)->s.mean;
    return (a > b) - (a < b);
}
```

This is a **comparison function** for `qsort` (C's built-in sort). `qsort` hands it two items and asks "which comes first?". It returns a negative number if x should come first, positive if y should, and 0 if they're equal.

- `(a > b) - (a < b)` gives 1 − 0 = 1 if a is bigger, 0 − 1 = −1 if a is smaller, and 0 if they're equal.
- So the tests get sorted from smallest mean time to largest: **fastest first**.

### 6.11 The driver: running and printing

```c
static void run(const char *title, test *t, int n, int N, int M,
                double *a, double *b, double *c, long elems) {
    double checksum = 0.0;
    for (int q = 0; q < n; q++) {
        t[q].f(N, a, b, c);                       /* warm-up call, not counted */
        for (int m = 0; m < M; m++)
            stats_add(&t[q].s, t[q].f(N, a, b, c));
        checksum += c[elems / 2];
    }
    ...
```

For each test `q` in the list:

1. `t[q].f(N, a, b, c);` calls that test's function once and **ignores** the time. That's the warm-up.
2. The `m` loop calls it M more times. Each call returns a time, and `stats_add(&t[q].s, ...)` feeds that time into the test's statistics box. `&` means "the address of", so `stats_add` can update the box. **This is the sheet's "wrap each timing call in a loop so you can get an average across M calls".**
3. `checksum += c[elems / 2];` reads one value of C (from the middle) and adds it to a total that gets printed. That's purely so the compiler sees C being used and can't delete the loops (Part 5.4).

```c
    qsort(t, n, sizeof *t, by_mean);
    printf("%s  (fastest first)\n", title);
    printf("  %-6s %14s %12s %10s %9s\n", "order", "mean [us]", "std [us]", "ns/elem", "x slower");
    for (int q = 0; q < n; q++)
        printf("  %-6s %14.2f %12.2f %10.3f %9.2f\n", t[q].name,
               1e6 * t[q].s.mean, 1e6 * stats_std(t[q].s),
               1e9 * t[q].s.mean / elems, t[q].s.mean / t[0].s.mean);
    printf("  (checksum %.3f)\n\n", checksum);
}
```

- `qsort(t, n, sizeof *t, by_mean)` sorts the `n` tests in place, fastest first. That gives the **ranking** the sheet asks for.
- The `printf` codes are format instructions. `%-6s` means "a piece of text, padded to 6 characters, left-aligned". `%14.2f` means "a decimal number, 14 characters wide, 2 digits after the point". They only make the columns line up.
- The printed columns are:
  - **mean [us]**: `1e6 * mean`. The mean is in seconds, and × 1,000,000 turns it into microseconds (µs).
  - **std [us]**: the standard deviation, also in µs.
  - **ns/elem**: `1e9 * mean / elems`, the time per element in nanoseconds. It lets you compare different N fairly.
  - **x slower**: this test's mean divided by the fastest test's mean (`t[0]`, since the list is sorted). The fastest shows 1.00.

### 6.12 `main`: where it all starts

```c
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s N M\n", argv[0]); return 1; }
    int N = atoi(argv[1]);
    int M = atoi(argv[2]);
    long NNN = (long)N * N * N;
```

- When you type `./array_timing 64 200`, the shell passes the words to `main`. `argc` is the number of words (3) and `argv` is the list of words: `argv[0]` = `"./array_timing"`, `argv[1]` = `"64"`, `argv[2]` = `"200"`.
- If you forget a number, `argc < 3`, so it prints how to use the program and quits. `return 1` means "finished with an error".
- `atoi` ("ASCII to integer") turns the text `"64"` into the number 64. **This is the sheet's "make N and M command line arguments".**

```c
    double *a = malloc(NNN * sizeof *a), *b = malloc(NNN * sizeof *b), *c = malloc(NNN * sizeof *c);
    if (!a || !b || !c) { fprintf(stderr, "out of memory\n"); return 1; }
```

- `malloc(bytes)` asks the operating system for that many bytes on the heap and returns the address of the start.
- `sizeof *a` is the size of one element of `a`, which is 8 for a double. So we ask for N³ × 8 bytes per array.
- If the machine can't supply that much memory, `malloc` returns `NULL` (zero), and `!a` is then true. We check this so the program stops with a clear message rather than crashing.
- This happens **once**, outside every timer. That's the sheet's "do not time the allocations".

```c
    for (long I = 0; I < NNN; I++) { a[I] = (double)I / NNN; b[I] = 1.0 - a[I]; c[I] = 0.0; }
```

This fills every element with a real number. `a[I]` goes from 0 up to nearly 1, and `b[I] = 1 − a[I]`, so every `a + b` comes out to exactly 1. That makes it easy to check: each run's middle value of C is 1, which is why the 3D checksum comes out as 7.000 (7 tests × 1) and the 2D one as 2.000. Filling also forces Linux to really give us the memory (Part 5.4).

```c
    printf("N = %d, M = %d, %ld elements, %.1f MiB per array\n\n",
           N, M, NNN, NNN * 8.0 / (1 << 20));
```

This prints a header. `1 << 20` is 2²⁰ = 1,048,576, the number of bytes in a MiB.

```c
    test t3[] = { {"ijk", time_ijk}, {"ikj", time_ikj}, {"jik", time_jik},
                  {"jki", time_jki}, {"kij", time_kij}, {"kji", time_kji},
                  {"1D",  time_1d} };
    run("3D version", t3, 7, N, M, a, b, c, NNN);
```

This makes a list of 7 tests, each a label plus the function to call. Their stats boxes start at zero automatically. Then `run` times them all and prints the sorted table.

```c
    test t2[] = { {"ij", time_ij}, {"ji", time_ji} };
    run("2D version (same N, N*N elements)", t2, 2, N, M, a, b, c, (long)N * N);
```

This does the same for the 2D pair. It reuses the start of the same arrays, using only the first N² elements.

```c
    free(a); free(b); free(c);
    return 0;
}
```

`free` hands the memory back. `return 0` means "finished OK".

---

## 7. Reading the serial output

### 7.1 An example output, annotated

Here's my N = 256 run (cloud Intel Xeon, `gcc -O2`, **not Hamilton**):

```
N = 256, M = 5, 16777216 elements, 128.0 MiB per array

3D version  (fastest first)
  order       mean [us]     std [us]    ns/elem  x slower
  1D           33858.00       408.18      2.018      1.00
  ijk          34915.00      1505.95      2.081      1.03
  jik          39728.60       526.50      2.368      1.17
  ikj         136319.00      1519.96      8.125      4.03
  jki         280041.60      3948.77     16.692      8.27
  kij         447632.80      6212.32     26.681     13.22
  kji         719485.80      8248.71     42.885     21.25
```

How to read the ijk row:

- **mean 34915 µs** = 0.035 seconds to add 16.8 million pairs.
- **ns/elem 2.081**: check it yourself, 34,915 µs × 1000 / 16,777,216 = 2.08 ns per element. ✓
- **std 1506 µs**: the 5 runs differed by about 4% of the mean. That's decent.
- **x slower 1.03**: 34,915 / 33,858 = 1.03, so it's 3% slower than the fastest (1D). That's within the noise, so call them equal.

The kji row: 719,485 / 33,858 = **21× slower** for the identical sum.

### 7.2 Answering each serial question

**"How many loop orderings?"** 3! = 6 (Part 3.2).

**"What is I(i, j, k)?"** `i·N2·N3 + j·N3 + k` for C (Part 2.2).

**"Which loop ordering is fastest?"** **ijk**, because k (stride 1) is innermost, so memory is read in street order.

**"What is the ranking?"** From the output: ijk ≈ jik, then ikj, then jki, then kij, then kji. The three groups come from where k sits (Part 4.4). The order *inside* a group can swap between runs and machines, because those pairs are closer together. My N = 64 run had kij and kji swapped.

**"Is the ranking different on Hamilton than on your machine?"** You find this out by running it on both, so you report what you see. What to expect: **the three groups should be the same** on both machines, because they come from C's memory layout, not from the hardware. **The gaps and the order inside a group may differ**, because an Apple M-series chip and an AMD EPYC have different cache sizes, different memory speeds and different prefetchers. Your answer should say both things, with your numbers.

**"What other factors influence the timing?"**

- **Compiler optimisation level.** At N = 64, ijk took 1302 µs at `-O0` (no optimisation), 254 µs at `-O2` and 232 µs at `-O3`. At `-O0` the compiler keeps every variable in memory and doesn't vectorise, so even the good ordering is 5× slower. `timing.slurm` runs all three levels so you can show this.
- **N compared with the cache size.** ijk was 0.97 ns per element at N = 64 (6 MiB in total, which fits in cache) and 2.1 ns per element at N = 256 (384 MiB, which has to come from RAM). Bigger arrays are slower *per element*.
- **Vectorisation**, which only works well with stride 1 (Part 4.3).
- **Warm-up and first touch** (Part 5.3).
- **Other jobs and the OS**, which is why we use M repeats and a std.
- **Timer resolution.** A single N = 16 call takes about 2 µs, close to the smallest time `clock()` can measure, so tiny N needs a big M.
- **How the arrays are filled** (untouched memory is fake-fast, Part 5.4).

### 7.3 The 1D challenge answers

**"Is it faster?"** About the same: 1D was 1.00 and ijk was 1.03 at N = 256, which is inside the noise. Both read memory in exactly the same order, 0, 1, 2, 3, ... The compiler already turns `A[i][j][k]` into the `i·N·N + j·N + k` sum, and it's clever about doing that cheaply inside loops. One loop instead of three saves a tiny bit of loop bookkeeping, and that's all.

**"Is it convenient?"** For "do the same thing to every element", yes, because it's one simple loop. But as soon as you need "the element to my right" (`A[i][j][k+1]`) or "the one below" (`A[i][j+1][k]`), you have to work out `I + 1` or `I + N` yourself, and it's easy to get wrong. The 3D form does that maths for you.

**"Is it necessary?"** No. `A[i][j][k]` and `a[I]` are **the same memory**, as Part 2 showed. The multi-index form is just a more readable label. **The order you visit memory in matters for speed. The number of indices you write doesn't.**

---

## 8. Parallel theory

### 8.1 Threads and shared memory

A **thread** is a worker that runs instructions. Normally your program has one. OpenMP lets you start several threads, ideally one per core, which all see **the same memory**. That's "shared memory parallelism", the BSP-style model from lecture 1.

Kitchen analogy (from Thread Test): the node is a kitchen, a core is a cooking station, a thread is a cook. All the cooks share one fridge (memory).

### 8.2 Why this problem is easy to parallelise

`C[i][j][k] = A[i][j][k] + B[i][j][k]` depends only on the **same** (i, j, k) in A and B. No element needs any other element's result. So the N³ additions can be done in **any order, by anyone, at the same time**. This is called "embarrassingly parallel".

Compare a running total like `x[i] = x[i-1] + a[i]`: there, element i needs element i−1 first, so you can't just hand them out.

### 8.3 Fork and join

```c
#pragma omp parallel for
for (int i = 0; i < N; i++) { ... }
```

- `#pragma omp ...` is an instruction to the compiler. It only works if you compile with **`-fopenmp`**. Without that flag, the line is ignored and the program runs on one thread.
- At the pragma, the main thread **forks**: it wakes up a team of threads (as many as `OMP_NUM_THREADS` says).
- The `for` loop's iterations are **split between the team**.
- At the end of the loop, all threads **join**: everyone waits until the last thread finishes, then the main thread carries on alone.

Fork and join cost time, a few microseconds each time. That matters a lot when the work is tiny (Part 8.7).

### 8.4 How iterations are split: the static schedule

By default (the "static" schedule), the iterations are cut into equal, contiguous blocks, one per thread.

Worked example: N = 64, so the `i` loop has 64 iterations, with 4 threads:

| thread | gets i = | slices | elements |
|---|---|---|---|
| 0 | 0 – 15 | 16 | 16 × 64 × 64 = 65,536 |
| 1 | 16 – 31 | 16 | 65,536 |
| 2 | 32 – 47 | 16 | 65,536 |
| 3 | 48 – 63 | 16 | 65,536 |

My census output with 4 threads said exactly this: `4 got work, 65536..65536 elements each`. ✓

Each thread gets a solid chunk of memory, so each one still reads its part in street order. That's good.

### 8.5 Which variables are private and which are shared

- **Shared:** A, B, C and N. Every thread uses the same arrays. That's safe because each thread writes **different** elements of C.
- **Private:** the loop counters i, j and k. Each thread must have its own, or they'd overwrite each other's counter.
  - The loop variable directly under `parallel for` (here `i`) is made private automatically.
  - `j` and `k` are declared **inside** the loops (`for (int j = ...)`), and anything declared inside the parallel region is automatically private. If you'd declared `int i, j, k;` at the top of the function (as the starter code does), `j` and `k` would be **shared**. That's a **race condition**: two threads changing the same counter, giving wrong answers. Then you'd need `#pragma omp parallel for private(j, k)`.

### 8.6 The limit: you can't use more threads than there are iterations

`parallel for` only splits the loop **directly beneath it**, here the `i` loop, which has **N iterations**. With N = 64:

- 64 threads: each thread gets 1 slice of 64 × 64 = **4096 elements**.
- 128 threads (a full Hamilton node): 64 threads get one slice each and **64 threads get nothing**. They're forked, sit idle, and join.

**So the answer to "how many threads could we use effectively with N = 64?" is 64 for a plain `parallel for`, each doing 4096 elements.**

### 8.7 collapse(n): making more pieces

```c
#pragma omp parallel for collapse(3)
for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++)
        for (int k = 0; k < N; k++)
            ...
```

`collapse(3)` tells OpenMP to treat the 3 nested loops as **one big loop** of N × N × N iterations and split that.

- N = 64 gives 262,144 iterations. That's far more than 128 threads, so **all 128 can work**, each getting 262,144 / 128 = **2048 elements**.

How it works: picture one counter I running from 0 to N³ − 1. Each I stands for one (i, j, k), with `i = I / (N·N)`, `j = (I / N) % N` and `k = I % N` (where `/` is whole-number division and `%` is the remainder). Check with I = 4227 and N = 64: i = 4227 / 4096 = 1, j = (4227 / 64) % 64 = 66 % 64 = 2, k = 4227 % 64 = 3. That gives **(1, 2, 3)**, matching Part 2.2. ✓

**The catch:** the compiler now runs one long loop and has to keep track of i, j and k itself. On every element it checks "has k reached N? If so, reset k and add 1 to j. Has j reached N? ...". That extra bookkeeping makes the loop harder to vectorise. In my test, `collapse(3)` was **about 3× slower than plain `parallel for` on the same number of threads**:

```
1 thread,  N=64:   pfor 226 µs    collapse(3) 652 µs    collapse(2) 218 µs
4 threads, N=64:   pfor  79 µs    collapse(3) 202 µs    collapse(2)  44 µs
```

**`collapse(2)`** is the middle ground. It joins only the i and j loops into N² = 4096 pieces (plenty for 128 threads) and leaves the k loop alone, so the inner loop is still a plain stride-1 loop the compiler can vectorise. That gives the best of both. The sheet only asks for `collapse(3)`, so I added `collapse(2)` as an extra worth a sentence in your write-up.

### 8.8 How much speedup to expect

From lecture 1: **speedup S = t₁ / tₚ** (time on 1 thread ÷ time on p threads), and **efficiency E = S / p**.

Worked example from my 4-thread test: serial 262 µs, collapse(2) 44 µs, so S = 262 / 44 ≈ 6. That's more than 4. Partly that's because each thread's quarter of the arrays fits better in its own core's cache, and partly it's noise (the serial time in that run had a large std). Don't expect it on every run.

What to expect on Hamilton (a **prediction**, not a measurement):

- **N = 64:** with 128 threads each thread has only about 2048 elements, roughly 0.5 µs of work, while fork plus join costs a few µs. The overhead is bigger than the work, so the speedup will **stop improving and may even drop** at high thread counts. In Amdahl's terms, the fork/join overhead acts like the serial fraction f.
- **N = 256:** each thread has 64× more work, so the overhead matters much less and it should keep speeding up much further. Eventually it hits the **memory bandwidth** of the node: all the cores share the same memory channels, so beyond some point extra cores just queue for memory.

### 8.9 NUMA and first touch

From the OpenMP lecture: Hamilton's node is **NUMA** (Non-Uniform Memory Access). Memory is attached to different chips, and a core reads its "own" memory faster than memory attached to the other socket.

Linux puts each page of memory next to **the core that first writes to it** ("first touch"). So the parallel program fills the arrays **with the same `parallel for` split as the timed loops**. That way, thread 5's part of A, B and C sits next to thread 5's core.

### 8.10 Pinning threads: OMP_PLACES and OMP_PROC_BIND

If the OS moves threads between cores in the middle of a run, they lose their cache and their nearby memory. `timing_omp.slurm` sets:

- `OMP_PLACES=cores`: one thread per physical core.
- `OMP_PROC_BIND=close`: keep threads on those cores (don't move them) and put thread 0, 1, 2, ... next to each other.

This makes the timings steadier.

---

## 9. The parallel program: whole code, big picture, then line by line

The file is `array_timing_omp.c`. Same three steps as Part 6: the whole code (9.1), what it does and how (9.2), then line by line (9.3 onwards). The Welford code and the `ARR3` macro are the same as in the serial file, so they're explained in Parts 6.5 and 6.6.

### 9.1 The whole program

Notice the shape again: the helpers at the top, four worker functions that differ only in their `#pragma` line, a `census` function, and `main`.

```c
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
```

### 9.2 The big picture: what the program does, and how

**What we're doing.** We take the fastest loop from the serial part (ijk) and run it **4 ways**: serial, `parallel for`, `parallel for collapse(3)` and `parallel for collapse(2)`. For each way, the program answers two questions:

1. **Who did the work?** How many threads were started, and how many actually got elements to add. That's the "census". It answers "how many threads are utilised, and how do you know?".
2. **How long did it take?** It times the work with **two** clocks, `omp_get_wtime()` and `clock()`, and prints the speedup against serial. That answers "should you keep using `clock()`?".

**The pieces of the program, grouped by job.** Back to the kitchen from Thread Test: a node is a kitchen, a core is a cooking station, and a thread is a cook.

| kind | names in the code | what it does | in the kitchen |
|---|---|---|---|
| **tools** | `stats` / `stats_add` / `stats_std`, `ARR3`, and from `omp.h`: `omp_get_wtime`, `omp_get_thread_num`, `omp_get_num_threads`, `omp_get_max_threads` | running mean and std, the 3D view, and OpenMP's "what time is it / who am I / how big is my team" questions | the clock on the wall, the notebook, the cooks' name badges |
| **workers** | `add_serial`, `add_pfor`, `add_collapse`, `add_collapse2` | each does the whole addition once. **The loops are identical. Only the `#pragma` line differs.** They don't time themselves | four ways of sharing out the same chopping job: one cook alone, or hand out work by slab, by element, or by row |
| **inspector** | `census()` | runs the same loop shape, but every thread ticks off each element it handles on its own tally. Then it counts who has a non-zero tally | a supervisor with a clipboard, checking which cooks actually did anything |
| **boss** | `main()` | reads N and M, gets memory, fills it in parallel, runs the 4 census checks, then times each worker with both clocks and prints the table | the head chef |

**One difference from the serial program:** there, each worker timed itself. Here, **`main` does the timing** around each worker call. That's because we want **both** clocks wrapped around **exactly the same** call.

**What happens, in order, when you type `OMP_NUM_THREADS=4 ./array_timing_omp 64 200`:**

1. `main` reads **N = 64** and **M = 200**. OpenMP reads `OMP_NUM_THREADS=4` by itself, so every parallel region will start a team of 4.
2. `main` asks for a, b and c with `malloc`.
3. `main` fills them **with a `parallel for`**, so each of the 4 threads writes the part of memory it will use later (first touch, Part 8.9).
4. `main` runs `census` four times (serial, pfor, collapse, collapse2) and prints one "who did the work" line for each.
5. For each of the 4 workers, `main`:
   - calls it once as a warm-up,
   - 200 times: reads both clocks, calls the worker, reads both clocks again, and puts the two differences into two notebooks (`w` for wall time, `cpu` for `clock()` time),
   - prints the mean wall time, its std, the mean `clock()` time, the ratio of the two, and the speedup against serial.
6. `main` frees the memory and ends.

As a picture of who calls whom:

```
main
 ├─ read N, M                       (OMP_NUM_THREADS is read by OpenMP itself)
 ├─ malloc a, b, c
 ├─ parallel fill of a, b, c        (each thread touches its own part first)
 ├─ census("serial")    ─┐
 ├─ census("pfor")       │  same loop shapes as the workers, but each thread
 ├─ census("collapse")   │  counts its elements in done[my_thread_number]
 ├─ census("collapse2") ─┘  → prints "team of T threads, B got work"
 ├─ for each worker in [serial, pfor, collapse, collapse2]:
 │    ├─ worker(N, a, b, c)                         warm-up
 │    ├─ repeat M times:
 │    │    t0 = omp_get_wtime();  c0 = clock();
 │    │    worker(N, a, b, c)                       ← fork → threads share the loop → join
 │    │    t1 = omp_get_wtime();  c1 = clock();
 │    │    wall notebook += t1 - t0;   cpu notebook += c1 - c0
 │    └─ print: wall mean, std, clock() mean, clock/wall, speedup
 └─ free a, b, c
```

**How the comparison is kept fair:**

- All 4 workers have **the same loop body and the same ijk order**. The only change is the pragma, so any time difference comes from how the work is shared out.
- The census uses the **same pragmas** as the workers, so it splits the work in exactly the same way, and its counts describe what the timed workers really do.
- Both clocks wrap **the same call**, so the `clock/wall` ratio is a direct comparison.

Now for the line-by-line breakdown, in file order: the include, the workers, the census, then `main`.

### 9.3 The extra include

```c
#include <omp.h>
```

This gives you `omp_get_wtime()`, `omp_get_thread_num()`, `omp_get_num_threads()` and `omp_get_max_threads()`.

### 9.4 The four kernels

These don't time themselves. `main` does the timing around them, so it can use two clocks at once.

```c
static void add_serial(int N, double *a, double *b, double *c) {
    double (*A)[N][N] = ARR3(a), (*B)[N][N] = ARR3(b), (*C)[N][N] = ARR3(c);
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            for (int k = 0; k < N; k++)
                C[i][j][k] = A[i][j][k] + B[i][j][k];
}
```

This is plain ijk with no pragma, the baseline. `void` means it returns nothing.

```c
static void add_pfor(...) {
    ...
    #pragma omp parallel for            /* N chunks of work: one per value of i */
    for (int i = 0; i < N; i++)
        ...same loops...
}
```

This is the same as `add_serial` plus one line. The pragma splits the `i` loop between threads (Parts 8.3 to 8.6).

```c
static void add_collapse(...)  { ... #pragma omp parallel for collapse(3) ... }
static void add_collapse2(...) { ... #pragma omp parallel for collapse(2) ... }
```

These are the same again, but the pragma joins 3 loops or 2 loops before splitting (Part 8.7).

**The only difference between the four functions is the pragma line.** That makes it a fair test.

### 9.5 The census: proving how many threads did work

The sheet asks "how many threads are utilised, and how do you know?". The census answers that by **counting**.

```c
static void census(const char *name, int N, int mode) {
    int T = omp_get_max_threads();
    long *done = calloc(T, sizeof *done);
    int team = 1;
```

- `omp_get_max_threads()` is how many threads a parallel region *would* start, which is the value of `OMP_NUM_THREADS`.
- `calloc(T, 8)` is like `malloc`, but it also sets everything to 0. So `done` is a tally sheet with one counter per thread: `done[0]` for thread 0, `done[1]` for thread 1, and so on.
- `team` will hold the team size, measured from inside the parallel region.

```c
    if (mode == 0) {                                   /* serial */
        done[0] = (long)N * N * N;
```

Serial: thread 0 does everything. No need to loop.

```c
    } else if (mode == 1) {
        #pragma omp parallel for
        for (int i = 0; i < N; i++) {
            if (i == 0) team = omp_get_num_threads();
            for (int j = 0; j < N; j++)
                for (int k = 0; k < N; k++)
                    done[omp_get_thread_num()]++;
        }
```

- This is the same loop shape as `add_pfor`, so the iterations are split in the same way.
- `if (i == 0) team = omp_get_num_threads();`: whichever thread gets iteration i = 0 records the team size. It has to happen **inside** the parallel region, because outside it the answer is always 1 (that was the Thread Test lesson).
- `done[omp_get_thread_num()]++`: `omp_get_thread_num()` is "which thread am I?" (0, 1, 2, ...). Each thread adds 1 to **its own** counter for every element it handles. No two threads touch the same counter, so there's no race.

Modes 3 and 2 do the same with `collapse(2)` and `collapse(3)` loop shapes. The "record the team" line goes where the very first iteration runs (`i == 0 && j == 0`, or `i == 0 && j == 0 && k == 0`).

```c
    int busy = 0; long lo = -1, hi = 0;
    for (int t = 0; t < T; t++) if (done[t] > 0) {
        busy++; if (lo < 0 || done[t] < lo) lo = done[t]; if (done[t] > hi) hi = done[t];
    }
    printf("  %-9s team of %3d threads, %3d got work, %ld..%ld elements each\n",
           name, team, busy, lo, hi);
    free(done);
}
```

This goes through the tally sheet:

- `busy` counts the threads whose counter isn't 0. That's "how many actually worked".
- `lo` and `hi` track the smallest and largest non-zero counts, which shows how evenly the work was split.
- Then it prints one line, for example: `pfor team of 8 threads, 4 got work, 16..16 elements each`.

**How to read the census for the sheet's questions:**

- **team** is how many threads were *started*.
- **got work** is how many were *utilised*.
- If got work < team, the extra threads were wasted. That's what happens with plain `parallel for` when there are more threads than values of i.

### 9.6 `main`: setup

```c
    int N = atoi(argv[1]), M = atoi(argv[2]);
    long NNN = (long)N * N * N;
    double *a = malloc(...), *b = malloc(...), *c = malloc(...);
```

This is the same as the serial version (Part 6.12).

```c
    #pragma omp parallel for
    for (long I = 0; I < NNN; I++) { a[I] = (double)I / NNN; b[I] = 1.0 - a[I]; c[I] = 0.0; }
```

This fills the arrays **in parallel**, for first touch (Part 8.9). Because I goes in the same order as ijk, thread t fills the same part of memory it will later work on.

```c
    printf("N = %d, M = %d, OMP_NUM_THREADS = %d\n", N, M, omp_get_max_threads());
    census("serial",   N, 0);
    census("pfor",     N, 1);
    census("collapse", N, 2);
    census("collapse2", N, 3);
```

This prints the header and the four census lines.

### 9.7 `main`: timing with two clocks

```c
    struct { const char *name; void (*f)(int, double *, double *, double *); } k[] = {
        {"serial", add_serial}, {"pfor", add_pfor}, {"collapse", add_collapse},
        {"collapse2", add_collapse2} };
```

This is a list of 4 entries, each a name plus a function pointer (Part 6.10). Here the functions return `void`, because they don't time themselves.

```c
    double base = 0.0, checksum = 0.0;
    for (int q = 0; q < 4; q++) {
        stats w = {0}, cpu = {0};
        k[q].f(N, a, b, c);                                    /* warm-up */
```

- `w` will collect wall-clock times and `cpu` will collect `clock()` times. `= {0}` starts every field at zero.
- There's one warm-up call before the timed ones.

```c
        for (int m = 0; m < M; m++) {
            double  t0 = omp_get_wtime();  clock_t c0 = clock();
            k[q].f(N, a, b, c);
            double  t1 = omp_get_wtime();  clock_t c1 = clock();
            stats_add(&w,   t1 - t0);
            stats_add(&cpu, (double)(c1 - c0) / CLOCKS_PER_SEC);
        }
```

M times over:

1. read **both** clocks,
2. run the kernel,
3. read both clocks again,
4. put the wall-time difference into `w` and the CPU-time difference (converted to seconds) into `cpu`.

Running both side by side is what lets you **show** the difference in Part 10, rather than just claim it.

```c
        checksum += c[NNN / 2];
        if (q == 0) base = w.mean;
        printf("  %-9s %14.2f %10.2f %14.2f %10.2f   speedup %.2fx\n", k[q].name,
               1e6 * w.mean, 1e6 * stats_std(w), 1e6 * cpu.mean, cpu.mean / w.mean, base / w.mean);
    }
```

- The checksum stops the compiler deleting the work (Part 5.4).
- `base` remembers the serial wall time from the first entry (q = 0).
- The printed columns are:
  - **wall [us]**: mean real time, in microseconds.
  - **std**: its standard deviation.
  - **clock() [us]**: mean CPU time.
  - **clock/wall**: CPU time ÷ real time. That's roughly **how many cores were busy**.
  - **speedup**: serial wall time ÷ this version's wall time. That's S = t₁ / tₚ from lecture 1.

---

## 10. clock() against omp_get_wtime()

**What each one measures:**

- `clock()`: **CPU time**, the total time that *all* threads of your program spent on a CPU, **added together**.
- `omp_get_wtime()`: **wall-clock time**, the real time that passed, as if you'd timed it with a stopwatch on the wall. It returns seconds as a `double`.

**Analogy:** 4 cooks each work 1 hour at the same time. The wall clock says 1 hour has passed. The payroll (`clock()`) says 4 hours of work were paid for.

**Real numbers from my test** (4 threads, N = 64):

```
version        wall [us]   clock() [us]   clock/wall
collapse2          43.87         201.19         4.59
```

The job really took **44 µs**. `clock()` says **201 µs**, about 4.6 × 44: 4 threads busy at once, plus a bit of time threads spent spinning while waiting at the join.

**Why that's a problem:** in the same run, `pfor` and serial looked like this:

```
version        wall [us]   clock() [us]
serial            262.04         296.27
pfor               78.63         297.64
```

By `clock()`, `pfor` looks **no faster than serial** (298 µs against 296 µs). By the wall clock it was **3.3× faster**. Speedup is about how long *you* wait, so you need wall time.

(side note: serial's `clock()` is a bit above its wall time too. That's because the idle OpenMP threads left over from the census keep spinning for a moment, waiting for more work, and `clock()` counts that.)

**The answer to "Should you continue to use clock()?": no. Use `omp_get_wtime()` for anything with threads.**

(side note: the lecture slides call it `omp_get_wall_time()`, but that function doesn't exist. The real name is `omp_get_wtime()`.)

---

## 11. Welford's method

### 11.1 What we want

From M times t₁ ... t_M we want:

- the **mean** μ = (t₁ + ... + t_M) / M,
- the **variance**, the average squared distance from the mean, and its square root, the **standard deviation**.

We want both **in one pass**, updating as each new time arrives, without storing all M times.

### 11.2 Why the obvious one-pass way is dangerous

A well-known formula says variance = (Σt² − (Σt)²/n) / (n − 1). You keep two running totals, Σt and Σt². It's mathematically correct, but on a computer it subtracts **two huge, nearly equal numbers**. Computers keep only about 16 significant digits, so the digits that matter can get rounded away. That's called **catastrophic cancellation**.

**I tested it.** Take four values 10⁹ + 4, 10⁹ + 7, 10⁹ + 13 and 10⁹ + 16. They're just 4, 7, 13 and 16 shifted up, so the true sample variance is **30**:

| method | answer |
|---|---|
| Σt² formula | **−170.7** (a negative variance, which is impossible) |
| Welford | **30.0** ✓ |

Timings in seconds are tiny numbers, so this exact case won't bite you here. But it's why the sheet asks for Welford, and it's good numerical practice.

### 11.3 Welford's update, one line at a time

The code:

```c
s->n += 1;                     // (1) one more value
double d = t - s->mean;        // (2) how far is t from the OLD mean?
s->mean += d / s->n;           // (3) move the mean 1/n of the way towards t
s->S += d * (t - s->mean);     // (4) add (distance from old mean) × (distance from new mean)
// variance = S / (n - 1)
```

What each line means:

1. Count the new value.
2. `d` is how much the new time differs from the average so far.
3. **Updating the mean.** The new average equals the old average plus a share of the surprise. With n values, the new one gets weight 1/n. For example, if the old mean of 2 values is 3 and the new value is 6, the new mean is 3 + (6 − 3)/3 = 4, which is the average of 2, 4 and 6. ✓ This is the same as the sheet's μ_{k+1} = (k/(k+1))·μ_k + (1/(k+1))·t_{k+1}, just rearranged.
4. **Updating S.** S is the running "sum of squared distances from the mean". When the mean moves, all the old distances shift a little. Multiplying the distance from the *old* mean by the distance from the *new* mean turns out to account for that exactly, with no big numbers being subtracted.

### 11.4 Worked example: times 2, 4, 6

| new t | n | d = t − old mean | new mean | S |
|---|---|---|---|---|
| 2 | 1 | 2 − 0 = 2 | 0 + 2/1 = **2** | 0 + 2 × (2 − 2) = **0** |
| 4 | 2 | 4 − 2 = 2 | 2 + 2/2 = **3** | 0 + 2 × (4 − 3) = **2** |
| 6 | 3 | 6 − 3 = 3 | 3 + 3/3 = **4** | 2 + 3 × (6 − 4) = **8** |

- mean = **4** ✓ ((2 + 4 + 6)/3 = 4)
- variance = S / (n − 1) = 8 / 2 = **4**, so std = √4 = **2**

Check it the slow way: the distances from 4 are −2, 0 and +2. Their squares are 4, 0 and 4, which add up to 8. Divided by 2 that's 4. ✓

(side note: the sheet divides S by k instead of k − 1, which gives the "population" variance, 8/3 here. Dividing by n − 1 gives the "sample" variance, which is the usual choice when your values are a sample of possible runs. With M = 200 the two differ by 0.5%. Say which one you used in your write-up.)

---

## 12. The SLURM scripts, line by line

### 12.1 `timing.slurm` (serial)

```bash
#!/bin/bash
```

"Run this file with bash." It always goes on the first line.

```bash
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
```

Lines starting with `#SBATCH` are instructions to SLURM (bash ignores them as comments). This asks for **1 node, 1 task (one program), 1 CPU core**, which is right for a serial program.

```bash
#SBATCH --mem=2G
```

Asks for 2 GB of memory. N = 256 needs 384 MiB, so this is plenty.

```bash
#SBATCH --job-name="array-timing"
#SBATCH -o timing.%J.out
#SBATCH -e timing.%J.err
```

The job's name in `squeue`, and where normal output (`-o`) and error messages (`-e`) go. `%J` becomes the job number, so each run gets its own files.

```bash
#SBATCH -t 00:10:00
#SBATCH -p test
```

A maximum of 10 minutes, on the `test` partition (short jobs, 1 node, 15 min maximum).

```bash
module load gcc
```

Makes the GCC compiler available inside the job.

```bash
echo "Node: $SLURMD_NODENAME"
lscpu | grep -E "^(Model name|L1d|L2|L3)"
```

Records which node you ran on and its CPU model and cache sizes. That's useful for the "Hamilton against my machine" answer.

```bash
for OPT in O0 O2 O3; do
    gcc -$OPT -o array_timing_$OPT array_timing.c -lm
done
```

Compiles the program three times: with no optimisation (`-O0`), normal (`-O2`) and aggressive (`-O3`). That produces `array_timing_O0`, `array_timing_O2` and `array_timing_O3`. `-o name` sets the output file name, and `-lm` links the maths library for `sqrt`.

```bash
for OPT in O0 O2 O3; do
    echo "################ gcc -$OPT, N=64 ################"
    ./array_timing_$OPT 64 200
done
```

Runs each version with N = 64 and M = 200. That gives the "compiler flags" evidence.

```bash
./array_timing_O2 128 20
./array_timing_O2 256 5
```

Bigger arrays, with a smaller M because each call takes longer. That gives the "N compared with cache size" evidence. The whole job takes about 2 minutes.

### 12.2 `timing_omp.slurm` (parallel)

```bash
#SBATCH --cpus-per-task=128
#SBATCH --mem=8G
```

Asks for **all 128 cores** of one node, so we can test up to 128 threads.

```bash
gcc -O2 -fopenmp -o array_timing_omp array_timing_omp.c -lm
```

**`-fopenmp` is essential.** Without it the pragmas are ignored and everything runs on one thread.

```bash
export OMP_PLACES=cores
export OMP_PROC_BIND=close
```

Pins threads to cores (Part 8.10). `export` makes these settings visible to the program.

```bash
OMP_NUM_THREADS=8 ./array_timing_omp 4 1000
```

The tiny demo: N = 4 has only 4 values of i, so with 8 threads the census should show `pfor ... 4 got work` and `collapse ... 8 got work`. Writing `VAR=value command` sets the variable for that one command only.

```bash
for N in 64 256; do
    if [ $N -eq 64 ]; then M=200; else M=10; fi
    for T in 1 2 4 8 16 32 64 128; do
        echo "######## N=$N, $T threads ########"
        OMP_NUM_THREADS=$T ./array_timing_omp $N $M
        echo
    done
done
```

Two nested loops in bash:

- outer: N = 64 (with M = 200) and N = 256 (with M = 10, because each call is slower),
- inner: thread counts 1, 2, 4, ..., 128.

Each run prints its census and timing table. Together they give your **speedup against thread count** data for both sizes.

---

## 13. The commands to run

### 13.1 On your Mac

```bash
cd /Users/artem/Documents/Durham/HPC/programs/openmp-array-timing
```

Go into the exercise folder. The new files are already there.

```bash
clang -O2 -o array_timing array_timing.c
```

Compile the serial program with Apple's compiler. (Apple's `clang` doesn't support `-fopenmp` out of the box, so only run the parallel program on Hamilton.)

```bash
./array_timing 64 200
./array_timing 256 5
```

Run it. **Save this output.** It's your "my machine" answer.

```bash
git add .
git commit -m "Array Timing: serial + OpenMP timing code, SLURM scripts, notes"
git push
```

- `git add .` stages all new and changed files in this folder.
- `git commit -m "..."` saves a snapshot with a message.
- `git push` uploads it to GitHub.

(side note: `git add .` would also add the compiled `array_timing` program. Delete it first with `rm array_timing`, or add `array_timing` to `.gitignore`.)

### 13.2 On Hamilton

```bash
ssh hamilton
```

Log in. It uses your `~/.ssh/config` alias and asks for your password and MFA code.

```bash
cd HPC-programs && git pull && cd openmp-array-timing
```

Go into the repo, download what you just pushed, then go into the exercise folder. `&&` means "only do the next step if this one worked".

```bash
sbatch timing.slurm
sbatch timing_omp.slurm
```

Submit both jobs to the queue. Each prints `Submitted batch job 1234567`. This is the right way to run things: the work happens on a compute node, not the login node.

```bash
squeue -u $USER
```

Shows your jobs. `PD` means waiting, `R` means running. When they've disappeared from the list, they're finished.

```bash
cat timing.*.out
cat timing_omp.*.out
```

Prints the results. Also check that the `.err` files are empty (`cat timing*.err`). If they're not, read them first.

### 13.3 What to copy from the output into your notes

1. The `-O2` ranking at N = 64 and N = 256, next to your Mac's ranking. That's the "fastest, ranking, Hamilton against my machine" answer.
2. The `-O0`, `-O2` and `-O3` ijk times. That's the "other factors" answer.
3. The census lines for N = 64 with 128 threads. Expect `pfor ... 64 got work` and `collapse ... 128 got work`. That's the "how many threads, how do you know" answer.
4. The speedup column against thread count for N = 64 and for N = 256, and the point where each one stops improving.
5. The `clock/wall` column. That's the evidence for "don't use clock()".

---

## 14. Glossary

- **Address:** the number of a byte in memory.
- **Bandwidth-bound:** speed is limited by how fast data moves from memory, not by the maths.
- **Cache:** small fast memory near the core that keeps copies of recently used data.
- **Cache line:** the 64-byte (8-double) piece memory is always fetched in.
- **Cast:** telling the compiler to treat a value as a different type, e.g. `(double)x`.
- **Catastrophic cancellation:** losing accuracy by subtracting two big, nearly equal numbers.
- **collapse(n):** OpenMP clause that joins n nested loops into one before splitting the work.
- **CPU time:** time spent on CPUs, added up over all threads (`clock()`).
- **Fork/join:** starting a team of threads, then waiting for all of them to finish.
- **Heap:** the big memory area `malloc` uses.
- **Stack:** the small memory area (usually 8 MB) for local variables. Big local arrays overflow it.
- **Macro:** a `#define` find-and-replace done before compiling.
- **NUMA:** memory attached to different chips, so some of it is closer (faster) than the rest.
- **First touch:** Linux places a memory page near the core that first writes to it.
- **Pointer:** a variable that holds an address.
- **Prefetcher:** hardware that guesses which memory you'll need next and fetches it early.
- **Race condition:** two threads changing the same variable at once, giving unpredictable results.
- **Row-major:** rows stored one after another, with the last index changing fastest (C).
- **Column-major:** columns stored one after another, with the first index changing fastest (Fortran, Julia).
- **Speedup:** S = t₁ / tₚ. **Efficiency:** E = S / p.
- **Stride:** how far apart in memory two consecutive accesses are.
- **Vectorisation (SIMD):** one instruction working on several neighbouring numbers at once.
- **Wall-clock time:** real elapsed time (`omp_get_wtime()`).
