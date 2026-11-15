#!/bin/bash

# 1 node, 1 task, 8 CPU cores for that task's threads
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --job-name="hello-openmp"
#SBATCH -o hello-openmp.%J.out
#SBATCH -e hello-openmp.%J.err
#SBATCH -t 00:01:00
#SBATCH -p shared

module load gcc

gcc -fopenmp -o hello-openmp openmp.c
gcc -fopenmp -o hello-openmp-last openmp-last.c

# One thread per CPU SLURM gave us (change --cpus-per-task above to try other counts)
export OMP_NUM_THREADS=$SLURM_CPUS_PER_TASK

echo "--- every thread says hello ---"
./hello-openmp
echo "--- only the highest thread says hello ---"
./hello-openmp-last
