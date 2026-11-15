# Array Timing (Exercise 4) walkthrough

**Big question: if a computer does exactly the same additions, why can the order you visit the elements make it 20× slower, and what happens when you add threads?**

The sheet asks for this:

1. **Serial:** how many loop orderings are there for `(i, j, k)`, and what is the formula for the linear index `I(i, j, k)`? Time `C = A + B` for every ordering, averaged over M runs, with N and M given on the command line. Which ordering is fastest, how do they rank, does Hamilton agree with your Mac, and what else affects the timing?
2. **Challenge:** do it again with one flat index, `double A[N*N*N]`. Is it faster? Is it convenient? Is it necessary?
3. **Parallel:** which OpenMP pragma should you use? How many threads can N = 64 keep busy? Try `parallel for`, then `collapse(3)`, and say how you know how many threads did work. Should you keep using `clock()`?
4. **Challenge:** compute the standard deviation alongside the mean with Welford's method.

Running analogy: **a library with one very long shelf.** Every book (number) sits in one long row. The librarian (CPU) never fetches a single book. They always grab a whole armful of 8 neighbouring books (a 64-byte *cache line*) and keep the last few armfuls on a trolley (the *cache*). Reading in shelf order is cheap. Jumping around the shelf means fetching a fresh armful for every book and throwing away 7 of them.

Files (for the full step-by-step explanation of the theory and every line of code, read `EXPLAINED.md`):

| File | What it is |
|---|---|
| `array_timing.c` | serial: all 6 orderings, the 2D pair, the 1D challenge, mean ± std |
| `array_timing_omp.c` | parallel: serial vs `parallel for` vs `collapse(3)` vs `collapse(2)`, with a "who did the work" count |
| `timing.slurm` | runs the serial program at -O0, -O2 and -O3, then at bigger N |
| `timing_omp.slurm` | whole node, 1 to 128 threads, N = 64 and 256 |

---

## Part 0: the starter `timing.c` has bugs

You only need to know these so you don't get confused if you run the original file.

- `time_ji` has its loop headers mixed up: `for(j=0; i<N; i++)` and `for (i=0; j<N; j++)`. It never loops properly.
- `main` calls `time_ij(M)`. It should be `time_ij(N)`.
- The "ji" timing calls `time_ij` again, so it prints the same thing twice.
- The arrays are never filled in, and C is never used afterwards. With `-O2` the compiler can see that the work is pointless and delete it, so you'd time nothing.

My versions fix all four. They also put the arrays on the heap with `malloc`, so you don't need `ulimit -s unlimited`. The arrays are still indexed as `A[i][j][k]`, using this trick:

```c
double (*A)[N][N] = (double (*)[N][N]) a;   // view a flat block as A[N][N][N]
```

---

## Part 1: how a 3D array sits in memory

**Number of orderings:** three loops can be nested in **3! = 6** orders: ijk, ikj, jik, jki, kij, kji.

**The index formula.** C is **row-major**, which means the **last** index changes fastest as you walk along memory:

```
I(i, j, k) = i·N2·N3 + j·N3 + k        ( = (i·N2 + j)·N3 + k )
```

Worked example, with N = 64: `A[1][2][3]` sits at 1·4096 + 2·64 + 3 = **4227** doubles from the start.

So a step of 1 in each index jumps this far:

| step in | jump in memory (N = 64) | library version |
|---|---|---|
| k | 1 double (8 bytes) | the next book along |
| j | N = 64 doubles (512 bytes) | 64 books along, a new armful every time |
| i | N² = 4096 doubles (32 KB) | a different bookcase |

(side note: the sheet has the labels swapped. It calls `iN2 + j` "column-major, like C" and the footnote calls `i + jN1` row-major. It's the other way round. C is row-major (`iN2 + j`). Fortran and Julia are column-major (`i + jN1`). That's why the Julia file `timing.jl` has the opposite fastest order.)

**Why it matters:** the rule falls straight out of the table. Put the index whose step is 1 (k in C) in the **innermost** loop.

---

## Part 2: what the serial program does

Each ordering is a small function that runs the triple loop and returns how long it took:

```c
double t0 = now();
for (int i = 0; i < N; i++)
  for (int j = 0; j < N; j++)
    for (int k = 0; k < N; k++)
      C[i][j][k] = A[i][j][k] + B[i][j][k];
return now() - t0;
```

`main` reads N and M from the command line (`./array_timing 64 200`), allocates and fills the arrays **once**, outside every timer, and then for each ordering does this:

- one warm-up call that isn't counted (the first call pays for loading things into cache),
- M timed calls, each fed into the running mean and standard deviation,
- a sort, so the table prints fastest first, with a "× slower" column.

Two details that would otherwise spoil the numbers:

- **Fill the arrays with real values.** Fresh `malloc` memory on Linux points at one shared page of zeros, so reading it looks impossibly fast.
- **Use C afterwards** (the program prints a checksum), so the compiler can't throw the loop away.

---

## Part 3: the results and the ranking

I ran it here on a cloud Intel Xeon, with `gcc -O2`. **These are not Hamilton numbers.** Your job output will give you those.

**N = 256** (128 MiB per array, far bigger than any cache):

| order | inner loop jumps | middle loop jumps | ns per element | × slower |
|---|---|---|---|---|
| ijk | 1 | N | 2.1 | 1.0 |
| jik | 1 | N² | 2.4 | 1.2 |
| ikj | N | 1 | 8.1 | 4.0 |
| jki | N² | 1 | 16.7 | 8.3 |
| kij | N | N² | 26.7 | 13 |
| kji | N² | N | 42.9 | 21 |
| 1D | 1 | (none) | 2.0 | 1.0 |

**Fastest: ijk** (with jik close behind). **Slowest: kji**, about 20× worse for exactly the same additions.

**The ranking comes in three pairs. Ask "where is k?":**

1. **k innermost** (ijk, jik): you read the shelf in order, so every book in each armful gets used.
2. **k in the middle** (ikj, jki): each inner sweep wastes most of each armful, but the next sweep uses the neighbouring books from the same armfuls, which are still on the trolley. So it's medium.
3. **k outermost** (kij, kji): you use 1 book per armful and the armful has gone by the time you come back for its neighbour. That's the worst case.

Inside each pair, a smaller jump in the inner loop usually wins. My N = 64 run at -O2 had kji and kij swap places. They're close and noisy.

**Is the ranking different on Hamilton than on your Mac?** The three groups should be the same on both, because that comes from C's memory layout and not from the machine. The exact order inside a group and the size of the gaps will differ. An M-series Mac and an EPYC 7702 have different cache sizes, memory bandwidth and prefetchers. Write down what your two runs actually show. That's the answer they want.

**Other things that change the timing** (this is the "what other factors" question):

- **Compiler flags.** At N = 64 here, ijk took 1302 µs at -O0, 254 µs at -O2 and 232 µs at -O3. At -O0 even the "good" order is 5× slower, and the ranking gets muddier. `timing.slurm` runs all three levels so you can compare.
- **N compared with the cache size.** ijk cost 0.97 ns per element at N = 64 (the 6 MiB of arrays fits in cache) and 2.1 ns at N = 256 (it has to come from RAM). An adder this simple is limited by how fast memory can deliver numbers, not by the arithmetic. That's what "bandwidth-bound" means.
- **Vectorisation.** With stride 1, the compiler can add 2 to 4 doubles per instruction. With big strides it can't.
- **Warm-up and first touch.** The first call is slower because pages and cache are still cold. That's why there's an untimed warm-up call.
- **Noise.** Other jobs on a shared node, CPU clock boost, the OS interrupting you. That's why you average over M and report the standard deviation.
- **Timer resolution.** A single N = 16 run takes about 2 µs, which is close to what `clock()` can resolve. So small N needs a big M.

### Challenge: one flat index

`time_1d` uses one loop, `for (long I = 0; I < N*N*N; I++) c[I] = a[I] + b[I];`.

- **Is it faster?** Barely. It came in at 1.00 against ijk's 1.03 at N = 256, which is inside the noise. ijk already walks memory in exactly this order. The compiler works out `i·N² + j·N + k` for you either way.
- **Is it convenient?** For a plain "do every element" loop, yes. As soon as you need neighbours (for example `A[i][j][k+1]`) you have to work out the offsets by hand, and that's where bugs creep in.
- **Is it necessary?** No. `A[i][j][k]` and `a[I]` are the same memory. The multi-index form is just a nicer label. What matters is the **order** you visit memory in, not how many indices you write.

---

## Part 4: going parallel

**Which pragma?** No `(i, j, k)` result depends on any other, so this is a plain `parallel for`, the worksharing loop from OpenMP lecture 1. Put it on the outer loop:

```c
#pragma omp parallel for            // splits the i loop between threads
for (int i = 0; i < N; i++)
  for (int j = 0; j < N; j++)
    for (int k = 0; k < N; k++)
      C[i][j][k] = A[i][j][k] + B[i][j][k];
```

(side note: `j` and `k` are declared inside the loops, so each thread automatically gets its own copy. If they were declared at the top of the function they'd be shared, which would be a race, and you'd need `private(j, k)`.)

**How many threads can N = 64 use?** Plain `parallel for` only splits the **i** loop. That's **64 chunks**, one per value of i.

- With 64 threads, each thread gets one slice `C[i][*][*]`, which is 64 × 64 = **4096 elements**.
- With 128 threads (a full Hamilton node), 64 threads get one slice each and **64 threads get nothing**.

**collapse(3)** turns the three loops into one loop of 64³ = **262,144 iterations** before splitting it. That's enough for every one of the 128 cores. Each thread gets 262,144 / 128 = **2048 elements**.

**How do you know?** Don't guess. Count. `array_timing_omp.c` has a "census" that runs the same loop shapes and has each thread tally how many elements it did. Here it is with the small demo from `timing_omp.slurm` (N = 4, 8 threads, run here):

```
pfor      team of   8 threads,   4 got work, 16..16 elements each
collapse  team of   8 threads,   8 got work, 8..8 elements each
```

The **team** is how many threads were started (`omp_get_num_threads()`, as in Thread Test). **Got work** is how many actually did anything. With N = 4 there are only 4 values of i, so half the team sits idle. On Hamilton with N = 64 and 128 threads, you should see `pfor ... 64 got work` and `collapse ... 128 got work`.

**The catch I found when testing:** `collapse(3)` was about **3× slower** than `parallel for` when using the same threads:

```
1 thread, N=64:   pfor 226 µs   collapse(3) 652 µs   collapse(2) 218 µs
4 threads, N=64:  pfor  79 µs   collapse(3) 202 µs   collapse(2)  44 µs
```

To collapse the loops, the compiler runs one long loop and has to keep track of i, j and k itself on every element ("has k reached N? then reset it and add 1 to j"). That bookkeeping makes the loop hard to vectorise. So I added **`collapse(2)`** as an extra. It splits the 64 × 64 = 4096 `(i, j)` rows, which is plenty for 128 threads, and leaves the k loop whole and fast. That's worth a sentence in your write-up: *more chunks is good, but not if each chunk gets more expensive*.

**What to expect on Hamilton (a prediction, not a result):** N = 64 is tiny. At 128 threads each thread has about 2048 elements, which is well under a microsecond of work, and starting and joining a team of threads costs a few microseconds. So the speedup should flatten out and may even drop at high thread counts. N = 256 has 64× more work per thread and should scale much further. This is Amdahl from lecture 1: fork/join overhead plays the part of the serial fraction.

**Should you keep using `clock()`?** **No.** `clock()` measures **CPU time added up over all threads**, not time on the wall. The program prints both, so you can see this (4 threads, N = 64, run here):

```
version        wall [us]   clock() [us]   clock/wall
collapse2          43.87         201.19         4.59
```

The job took 44 µs, but `clock()` says 201 µs, because 4 threads were busy at once (plus some spinning while they wait). It would make parallel code look *slower* than serial. Use **`omp_get_wtime()`** (wall-clock time in seconds, as a double). The slides call it `omp_get_wall_time`, which doesn't exist.

```c
double t0 = omp_get_wtime();
add_pfor(N, a, b, c);
double t = omp_get_wtime() - t0;
```

(side note: the program also fills the arrays in parallel with the same split as the work. On a NUMA machine like Hamilton, a page lives near the core that first writes to it, so this keeps each thread's data close. That's "first touch" from the UMA vs NUMA slides.)

### Challenge: mean and standard deviation in one pass (Welford)

The naive way is to keep Σt and Σt² and use "mean of squares minus square of mean". When the times are almost identical, that subtracts two huge, nearly equal numbers and the answer is mostly rounding error (the "catastrophic cancellation" the sheet mentions). Welford's method updates the mean and a running sum of squared distances S, one time at a time:

```c
n += 1;
d  = t - mean;          // how far the new time is from the OLD mean
mean += d / n;          // move the mean a 1/n step towards t
S  += d * (t - mean);   // old distance × new distance
// variance = S / (n - 1),  std = sqrt(variance)
```

Worked example, with times 2, 4, 6:

| t | n | d | new mean | S |
|---|---|---|---|---|
| 2 | 1 | 2 | 2 | 0 |
| 4 | 2 | 2 | 3 | 0 + 2·1 = 2 |
| 6 | 3 | 3 | 4 | 2 + 3·2 = 8 |

So the mean is 4 and the variance is 8 / 2 = 4, giving a std of **2**. That's correct. Both programs print this std next to each mean.

(side note: the sheet divides S by k, which gives the "population" variance, 8/3 here. I used n − 1, the usual "sample" version. With M = 200 the difference is 0.5%. Use whichever you like, but say which one.)

---

## Running it

**1. On your Mac:** the files are already in `programs/openmp-array-timing` (or unzip `array-timing.zip` there). Have a quick local run (this is your "my machine" result), then push.

```bash
cd /Users/artem/Documents/Durham/HPC/programs/openmp-array-timing
clang -O2 -o array_timing array_timing.c
./array_timing 64 200
./array_timing 256 5
git add .
git commit -m "Array Timing: serial + OpenMP timing code, SLURM scripts, notes"
git push
```

(side note: Apple's `clang` doesn't do `-fopenmp` out of the box, so run the parallel program on Hamilton only. The serial one is fine on the Mac.)

**2. On Hamilton:**

```bash
ssh hamilton
cd HPC-programs && git pull && cd openmp-array-timing
sbatch timing.slurm        # serial, 1 core, about 2 minutes
sbatch timing_omp.slurm    # whole node, 1 to 128 threads
squeue -u $USER            # wait until both have gone
cat timing.*.out
cat timing_omp.*.out
```

- Both scripts compile inside the job, so there's no heavy work on the login node.
- Both use `-p test` (1 node, 15 minutes max). `timing_omp.slurm` asks for all 128 cores, so it may wait a little. If it sits in the queue for ages, change it to `-p shared`.
- If any `.err` file isn't empty, have a look at it first.

**What to pull out of the output for your notes:**

1. The serial ranking at -O2 for N = 64 and N = 256, next to your Mac's ranking.
2. The census lines at N = 64 with 128 threads (pfor 64 busy, collapse 128 busy).
3. Wall-time speedup against threads for N = 64 and N = 256, and where each one stops improving.
4. The `clock/wall` column, as evidence for the "don't use clock()" answer.

---

## Must-remembers

1. **C is row-major: the last index is the stride-1 one.** `I = i·N² + j·N + k`, so loop k innermost. Fortran and Julia are the other way round.
2. **The order of loops, not the number of indices, decides speed.** It can be 20× here for identical maths, because the adder is bandwidth-bound.
3. **`parallel for` splits only the loop directly under it.** N = 64 means at most 64 busy threads. `collapse(n)` makes more chunks but can cost speed per element, and `collapse(2)` is often the sweet spot.
4. **Count, don't guess:** compare `omp_get_num_threads()` (the team) with which threads actually got iterations.
5. **Time parallel code with `omp_get_wtime()`, never `clock()`.** `clock()` adds up the CPU time of every thread.
