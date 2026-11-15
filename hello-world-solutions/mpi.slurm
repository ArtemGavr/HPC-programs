#!/bin/bash

# 1 node, 2 MPI processes on it
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=2
#SBATCH --job-name="hello-mpi"
#SBATCH -o hello-mpi.%J.out
#SBATCH -e hello-mpi.%J.err
#SBATCH -t 00:01:00
#SBATCH -p shared

module load gcc
module load openmpi

mpicc -o hello-mpi mpi.c

# No -np: on Hamilton mpirun reads the process count from SLURM
mpirun ./hello-mpi
