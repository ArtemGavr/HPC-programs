#!/bin/bash

# Ask for a WHOLE compute node so lscpu/nproc see all of it
#SBATCH --nodes=1
#SBATCH --exclusive
#SBATCH --job-name="cores"
#SBATCH -o cores.%J.out
#SBATCH -e cores.%J.err
#SBATCH -t 00:01:00
#SBATCH -p test

echo "Node: $SLURMD_NODENAME"
lscpu | grep -E "^(Model name|Socket\(s\)|Core\(s\) per socket|Thread\(s\) per core|CPU\(s\)|NUMA node\(s\))"
echo "nproc (CPUs this job can use): $(nproc)"
echo "SLURM_CPUS_ON_NODE: $SLURM_CPUS_ON_NODE"
echo "SLURM's own record of this node:"
scontrol show node "$SLURMD_NODENAME" | grep -E "CPUTot|Sockets|CoresPerSocket|ThreadsPerCore|RealMemory"
