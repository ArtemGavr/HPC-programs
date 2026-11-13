#!/bin/bash

# 1 node, 1 task, 8 CPU cores for that task's threads
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --job-name="threadtest"
#SBATCH -o threadtest.%J.out
#SBATCH -e threadtest.%J.err
#SBATCH -t 00:02:00
#SBATCH -p test

module load gcc

gcc -fopenmp -o threadtest      threadtest.c
gcc -fopenmp -o threadtest5     threadtest5.c
gcc -fopenmp -o threadtest5-safe threadtest5-safe.c

echo "===== 1. Hardware of the compute node we landed on ====="
echo "Node name: $SLURMD_NODENAME"
lscpu | grep -E "^(Model name|Socket\(s\)|Core\(s\) per socket|Thread\(s\) per core|CPU\(s\)|NUMA node\(s\))"
SOCKETS=$(lscpu | awk -F: '/^Socket\(s\)/ {gsub(/ /,"",$2); print $2}')
CORES_PER_SOCKET=$(lscpu | awk -F: '/^Core\(s\) per socket/ {gsub(/ /,"",$2); print $2}')
echo "Physical cores on this node = $SOCKETS sockets x $CORES_PER_SOCKET cores = $((SOCKETS * CORES_PER_SOCKET))"
echo "What SLURM says the node has: $SLURM_CPUS_ON_NODE CPUs allocated to this job on it"
echo "CPUs this job may actually use (nproc): $(nproc)"
echo

# Use exactly the CPUs SLURM gave us
export OMP_NUM_THREADS=$SLURM_CPUS_PER_TASK

echo "===== 2. Original threadtest ($OMP_NUM_THREADS threads) ====="
./threadtest
echo

echo "===== 3. Fixed: only thread 5 reports ====="
./threadtest5
echo

echo "===== 4. What happens with only 4 threads? ====="
OMP_NUM_THREADS=4 ./threadtest5
echo "(nothing reported: there is no thread #5)"
OMP_NUM_THREADS=4 ./threadtest5-safe
