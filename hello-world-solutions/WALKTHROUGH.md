# Hello World exercise: walkthrough

The sheet has 4 parts. The `.c` files you already had are fine (they match the sheet).
What was missing was the little exercise in each part, plus the whole scaling part.

## 0. Get the new files onto Hamilton

On your Mac, put these files into `~/Documents/Durham/HPC/programs/hello-world/`, then:

```bash
cd ~/Documents/Durham/HPC/programs
git add hello-world
git commit -m "Hello World exercise: slurm fixes, openmp-last, scaling fits"
git push
```

On Hamilton:

```bash
ssh hamilton
cd ~/HPC-programs          # or wherever you cloned it
git pull
cd hello-world
```

## 1. Serial

Quick test on the login node is allowed for something this tiny:

```bash
module load gcc
gcc serial.c -o hello-serial
./hello-serial            # -> Hello, World!
```

Then the real way, as a job:

```bash
sbatch serial.slurm
squeue -u $USER           # wait until it disappears from the list
ls hello-serial.*.{err,out}
cat hello-serial.*.out    # Hello, World!
cat hello-serial.*.err    # should be empty
```

**Exercise answer:** `serial.slurm` now compiles inside the job (`gcc serial.c -o hello-serial`
before running). Yes, it still works. Doing it this way means the compiler and libraries
used to build are the same ones used to run.

If SLURM says the partition `test` doesn't exist, run `sfree` and use the test queue name it
shows (your Week 0 notes call it `test.q`).

## 2. OpenMP

```bash
sbatch openmp.slurm
cat hello-openmp.*.out
```

Two changes from the sheet's script:

- `--cores=2` became `--cpus-per-task=8`. `--cores` isn't a real sbatch option; SLURM quietly
  reads it as `--cores-per-socket`, which picks *which* nodes are allowed but doesn't give you
  2 CPUs. `--cpus-per-task` is the right way to ask for cores for threads.
- `OMP_NUM_THREADS=$SLURM_CPUS_PER_TASK`, so the thread count always matches what you asked for.
  Change the `8` in one place to try other counts.

**Exercise answers:**

- *What do you notice when you change the thread count?* The lines come out in a **random
  order**, and the order changes from run to run. All threads run at the same time and race to
  print. Thread numbers always go 0 to N-1.
- *Only the highest thread says hello:* see `openmp-last.c`. Every thread still runs, but
  only the one where `thread == nthread - 1` prints. With 8 threads you get one line:
  `Hello, World! I am thread 7 of 8 (the last one)`.

## 3. MPI

```bash
sbatch mpi.slurm              # 1 node,  2 processes
sbatch mpi-2nodes.slurm       # 2 nodes, 2 per node = 4 processes
sbatch mpi-2nodes-1each.slurm # 2 nodes, 1 per node = 2 processes
cat hello-mpi*.out
```

I removed `-np 2` from `mpirun`. On Hamilton mpirun gets the count from SLURM, and a hard-coded
`2` would have hidden what happens when you go to 2 nodes.

**Exercise answers:**

- *2 nodes: how many processes?* nodes x tasks-per-node = 2 x 2 = **4**.
- *Node names?* Two different names show up (e.g. `cn1234` and `cn1235`), two ranks on each.
  Rank order in the output is random, like the OpenMP threads.
- *How many nodes while keeping 2 processes total?* At most **2** (1 process per node,
  that's `mpi-2nodes-1each.slurm`). A node with zero processes is pointless.

Try `module purge; module load openmpi` to see the hint: it fails or complains because
OpenMPI needs a compiler module loaded first. Then `module load gcc openmpi` works.

## 4. Parallel scaling

Run on your **Mac**, not Hamilton (it's just plotting):

```bash
pip3 install numpy scipy matplotlib
cd ~/Documents/Durham/HPC/programs/hello-world
python3 scaling.py
```

It prints tables and saves `strong_scaling.png`, `weak_scaling.png`, `usl_fits.png`.

### Strong scaling (Amdahl, `strong.dat`)

Amdahl in words: total time = the serial bit (never speeds up) + the parallel bit divided by p.

- **Serial fraction f = 0.015** (about 1.5% of the work is serial). Leaving out p=256 gives 0.0145, so basically the same.
- **Max speedup with infinite threads = 1/f = about 66x.**
- **Good fit?** Good up to about 16 threads. After that the real data is a bit *better* than
  the curve at 64 and 128, then much *worse* at 256: time goes **up** from 1.02s to 1.44s.
  Amdahl can never predict time going up, because it only has "serial part" and "perfectly
  parallel part". The rise means there's an overhead that grows with p (thread start-up,
  synchronisation, memory traffic, or 256 threads on a 128-core node fighting for cores).
  Efficiency drops from 95% at 2 threads to 18% at 256.

### Weak scaling (Gustafson, `weak.dat`)

Gustafson in words: give each process its own chunk; the speedup is how much bigger a problem
you finished in the same-ish time. Scaled speedup S(p) = f + (1 - f) x p.

- **Can you fit (p, t(p)) on its own?** **No.** Each p is a *different-size* problem, and
  Gustafson needs t(1) for that bigger problem, i.e. how long the p-sized problem would take on
  one process. We never measured that, so the times alone don't contain f.
- **With serial times ts(p) from Eq. (5)** (`scaling.py` generates them with a fixed random
  seed): at each p, serial fraction = ts(p) / t(p). These range from 0.23 to 0.99, so I take the
  median as the single f: **f ≈ 0.39**. (Using the third column that was already in `weak.dat`
  gives f ≈ 0.44, the same picture.)
- **Good fit?** Okay up to about 16 processes, then poor. The "serial fraction" isn't constant:
  ts grows with p (the xi x p term) and t(p) grows too (2.4s at 256 vs 0.58s at 1), so one f
  can't describe it. The random noise in xi also throws single points around (e.g. p=128).
- **Max speedup with infinite processes?** Gustafson says **no limit**: S keeps growing in a
  straight line with slope (1 - f), about 0.6 extra per process. That's the key difference from
  Amdahl's hard cap of 1/f.

### Challenge: Universal Scaling Law

USL adds two penalties to the ideal "N times faster":
**alpha = contention** (queueing for something shared, like a lock or memory bus) and
**beta = coherency** (every worker having to sync with every other one, which costs ~N²).
Because of beta, USL *can* bend over and go down, which Amdahl can't.

- **Strong:** alpha ≈ 0.003, beta ≈ 5e-5, peak around N ≈ 137. Fits better than Amdahl
  (RMS error 3.6 vs 7.4) because it captures the drop at 256. Diagnosis: small contention,
  but the coherency/crosstalk cost dominates at high thread counts.
- **Weak:** alpha ≈ 0.009, beta ≈ 1e-5, peak around N ≈ 310. Contention is the bigger effect,
  so throughput flattens out; the 256 point suggests it's close to the peak already.

## One-line summary

Compile with `gcc` / `gcc -fopenmp` / `mpicc`, always run through `sbatch`, ask SLURM for the
right resources (`--cpus-per-task` for threads, `--nodes` x `--ntasks-per-node` for MPI), and for
scaling: Amdahl gives a hard ceiling of 1/f (here ~66x), Gustafson has no ceiling but needs
serial-time measurements to estimate f.
