#!/bin/bash

# Exercise: keep the original 2 processes total, but spread over 2 nodes (1 per node)
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=1
#SBATCH --job-name="hello-mpi-2x1"
#SBATCH -o hello-mpi-2x1.%J.out
#SBATCH -e hello-mpi-2x1.%J.err
#SBATCH -t 00:01:00
#SBATCH -p shared

module load gcc
module load openmpi

mpicc -o hello-mpi mpi.c

mpirun ./hello-mpi
