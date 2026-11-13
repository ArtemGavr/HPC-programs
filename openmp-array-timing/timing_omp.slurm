#!/bin/bash

# Parallel array timing: a whole compute node (128 cores), 1..128 threads
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=128
#SBATCH --mem=8G
#SBATCH --job-name="array-timing-omp"
#SBATCH -o timing_omp.%J.out
#SBATCH -e timing_omp.%J.err
#SBATCH -t 00:10:00
#SBATCH -p test

module load gcc

gcc -O2 -fopenmp -o array_timing_omp array_timing_omp.c -lm

# one thread per core, threads packed next to each other
export OMP_PLACES=cores
export OMP_PROC_BIND=close

echo "Node: $SLURMD_NODENAME, CPUs for this job: $(nproc)"
echo

echo "######## Tiny demo: N=4 has only 4 values of i, so pfor can keep at most 4 threads busy ########"
OMP_NUM_THREADS=8 ./array_timing_omp 4 1000
echo

for N in 64 256; do
    if [ $N -eq 64 ]; then M=200; else M=10; fi
    for T in 1 2 4 8 16 32 64 128; do
        echo "######## N=$N, $T threads ########"
        OMP_NUM_THREADS=$T ./array_timing_omp $N $M
        echo
    done
done
