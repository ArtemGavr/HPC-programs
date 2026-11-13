#!/bin/bash

# Serial array timing: 1 core, all 6 loop orderings, 3 optimisation levels
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=2G
#SBATCH --job-name="array-timing"
#SBATCH -o timing.%J.out
#SBATCH -e timing.%J.err
#SBATCH -t 00:10:00
#SBATCH -p test

module load gcc

echo "Node: $SLURMD_NODENAME"
lscpu | grep -E "^(Model name|L1d|L2|L3)"
echo

for OPT in O0 O2 O3; do
    gcc -$OPT -o array_timing_$OPT array_timing.c -lm
done

for OPT in O0 O2 O3; do
    echo "################ gcc -$OPT, N=64 ################"
    ./array_timing_$OPT 64 200
done

echo "################ gcc -O2, bigger arrays ################"
./array_timing_O2 128 20
./array_timing_O2 256 5
