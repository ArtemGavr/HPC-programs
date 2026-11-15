#!/bin/bash

# Exercise: 2 nodes x 2 tasks per node = 4 MPI processes
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=2
#SBATCH --job-name="hello-mpi-2n"
#SBATCH -o hello-mpi-2n.%J.out
#SBATCH -e hello-mpi-2n.%J.err
#SBATCH -t 00:01:00
#SBATCH -p shared

module load gcc
module load openmpi

mpicc -o hello-mpi mpi.c

mpirun ./hello-mpi
