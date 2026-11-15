# Thread Test (Exercise 3) walkthrough

**Big question: how many workers does the machine really have, and when can a program actually see them?**

The sheet has two small tasks:

1. **Hardware:** how many physical cores are on Hamilton's compute nodes and login nodes, and how would a batch script find out?
2. **Threads:** run `threadtest.c`, explain what it prints, then change it so that only thread 5 prints the correct number of threads.

Running analogy: **a restaurant kitchen.** The node is the kitchen, a core is a cooking station, and a thread is a cook. `#pragma omp parallel` is the moment the head chef shouts "everyone in!" and the team turns up.

---

## Part 1: counting the cores

**What lscpu shows you**

- `Socket(s)`: how many CPU chips are on the motherboard (the number of kitchens in the building).
- `Core(s) per socket`: the number of cooking stations in each kitchen.
- `Thread(s) per core`: the number of cooks that can share one station (hyperthreading, or SMT). If it says 2, the `CPU(s)` count is double the number of real cores.
- **Physical cores = sockets × cores per socket.** Don't take `CPU(s)` as the answer without checking threads per core first.

**Hamilton8's numbers** (from Durham ARC's [Systems page](https://www.dur.ac.uk/research/institutes-and-centres/advanced-research-computing/hamilton-supercomputer/systems/))

- **Compute node:** 2 × AMD EPYC 7702, so 2 sockets × 64 cores = **128 physical cores**, with 256 GB RAM (246 GB usable).
- **Login node:** ARC doesn't publish its specs. The `lstopo` picture in OpenMP lecture 1 showed a Hamilton login node with 2 packages and 128 cores. **Run `lscpu` on the login node yourself and write down what it says.** That's cheap to do and it's allowed.
- (side note: ARC doesn't say whether hyperthreading is switched on. If `Thread(s) per core` says 1, then `CPU(s)` = 128 = physical cores.)

Worked example: if lscpu said `Socket(s): 2`, `Core(s) per socket: 64` and `Thread(s) per core: 2`, you would see `CPU(s): 256`, but the node would still have only **128** physical cores.

**How a batch script finds out (the third question)**

The catch is that `lscpu` and `nproc` answer different questions inside a job:

| Command | What it tells you | Kitchen version |
|---|---|---|
| `lscpu` | the whole node's hardware | how many stations the kitchen has |
| `nproc` | the CPUs **your job** is allowed to use | how many stations you booked |
| `$SLURM_CPUS_PER_TASK` | what you asked for with `--cpus-per-task` | what's written on your booking |
| `scontrol show node $SLURMD_NODENAME` | SLURM's own record (`CPUTot`, `Sockets`, `CoresPerSocket`) | the building's floor plan |

So a script that detects the node's physical cores does this:

```bash
SOCKETS=$(lscpu | awk -F: '/^Socket\(s\)/ {gsub(/ /,"",$2); print $2}')
CPS=$(lscpu | awk -F: '/^Core\(s\) per socket/ {gsub(/ /,"",$2); print $2}')
echo "Physical cores = $((SOCKETS * CPS))"
export OMP_NUM_THREADS=$SLURM_CPUS_PER_TASK   # but only use what you booked
```

`cores.slurm` does exactly this on a whole node (`--exclusive`), so `lscpu`, `nproc` and `scontrol` should all agree on 128.

**Why it matters:** if you start more threads than you have cores, the cooks queue for stations and your speedup numbers in the scaling report go bad. Setting `OMP_NUM_THREADS=$SLURM_CPUS_PER_TASK` stops that.

---

## Part 2: what threadtest.c prints and why

**The code has two halves:**

```c
printf("... uses %d threads.\n", omp_get_thread_num(), omp_get_num_threads());  // OUTSIDE

#pragma omp parallel
  printf("Hello world from thread #%d\n", omp_get_thread_num());                 // INSIDE
```

**What I got here with `OMP_NUM_THREADS=8`:**

```
Thread #0 reports: This program uses 1 threads.
Hello world from thread #1
Hello world from thread #2
Hello world from thread #5
Hello world from thread #7
Hello world from thread #6
Hello world from thread #0
Hello world from thread #4
Hello world from thread #3
```

**The answer to "how many threads are reported?"**

- The first line always says **1 thread**. That isn't a bug in the counter: `omp_get_num_threads()` gives the size of the **current team**, and outside a parallel region the team is just the master thread (thread 0). The head chef is alone in the kitchen before shouting "everyone in!"
- Then there are **N "Hello" lines**, one per thread, where N = `OMP_NUM_THREADS`. If you don't set it, N defaults to the number of CPUs the program can see.
- The order changes every run, because the threads race to `printf`. That's the fork-join picture from lecture: fork, everyone works in any order, then join.

So the number of threads the program "says" it uses (1) and the number it actually uses (8) disagree.

---

## Part 3: making only thread 5 report the right number

**The fix:** move the report **inside** the parallel region, where the team exists, and let only thread 5 run it. See `threadtest5.c`:

```c
#pragma omp parallel
{
  int id = omp_get_thread_num();
  if (id == 5) {
    printf("Thread #%d reports: This program uses %d threads.\n",
           id, omp_get_num_threads());
  }
  printf("Hello world from thread #%d\n", id);
}
```

Output with 8 threads (tested here):

```
Hello world from thread #1
...
Thread #5 reports: This program uses 8 threads.
Hello world from thread #5
...
```

Two things to notice:

- The `{ }` braces matter. Without them, `#pragma omp parallel` covers **only the next single statement**. In the original code, that's why only one `printf` ran in parallel.
- **The trap:** thread IDs go 0 to N−1, so thread 5 exists only when N ≥ 6. With `OMP_NUM_THREADS=4`, nobody prints the report (I tested this: there are just 4 hellos). `threadtest5-safe.c` adds a warning for that case using `#pragma omp single` ("exactly one cook does this job"):

```
Only 4 threads, so there is no thread #5 to report.
```

**Why it matters:** it's the same idea as `openmp-last.c` in Hello World, where you pick a thread by its ID. In Array Timing you'll need the real thread count inside the region again.

---

## Running it on Hamilton

**1. On your Mac:** copy the files into the repo folder and push.

```bash
cd /Users/artem/Documents/Durham/HPC/programs/openmp-thread-test
# put threadtest5.c, threadtest5-safe.c, threadtest.slurm, cores.slurm, WALKTHROUGH.md here
git add .
git commit -m "Thread Test: thread-5 fix, SLURM scripts, notes"
git push
```

**2. On Hamilton:**

```bash
ssh hamilton
cd HPC-programs && git pull && cd openmp-thread-test

lscpu | grep -E "^(Model name|Socket|Core|Thread|CPU\(s\))"   # login node answer (light command, fine here)

sbatch threadtest.slurm    # compile + all 4 experiments on 8 cores
sbatch cores.slurm         # whole compute node, for the core count
squeue -u $USER            # wait until they're gone from the list
cat threadtest.*.out cores.*.out
```

- Both scripts use `-p test`, which ARC lists as a short-test queue (1 node, 15 minutes max). If it's busy, change it to `-p shared`. `cores.slurm` asks for a whole node, so it may wait a bit longer.
- Don't run `./threadtest` on the login node with lots of threads. Compile and run in the job, which is what the scripts do.

---

## Must-remembers

1. **Physical cores = sockets × cores per socket.** On a Hamilton8 compute node that's 2 × 64 = 128.
2. **`lscpu` shows the node, while `nproc` and `$SLURM_CPUS_PER_TASK` show your share.** Set `OMP_NUM_THREADS` from your share.
3. **`omp_get_num_threads()` gives the size of the current team:** 1 outside a parallel region, N inside one.
4. **`#pragma omp parallel` covers only the next statement** unless you give it a `{ }` block.
5. **Thread IDs run from 0 to N−1,** so "thread 5" needs at least 6 threads.
