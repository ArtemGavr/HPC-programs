#!/bin/bash

# Part 5: time the encoder on Frankenstein with 1..128 threads on a whole node
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=128
#SBATCH --mem=4G              # 1000 copies needs about 0.9 GB
#SBATCH --job-name="frankenstein"
#SBATCH -o frankenstein.%J.out
#SBATCH -e frankenstein.%J.err
#SBATCH -t 00:10:00
#SBATCH -p test

module load gcc

gcc -O2 -Wall -fopenmp -o frankenstein_timing frankenstein_timing.c

# Make a pure-ASCII copy. //TRANSLIT turns curly quotes and dashes into plain ones;
# without it iconv stops at the first one and leaves a file of only 1449 bytes.
iconv -f UTF-8 -t ASCII//TRANSLIT Frankenstein.txt > ASCII-Frankenstein.txt
wc -c Frankenstein.txt ASCII-Frankenstein.txt

# one thread per core, threads packed next to each other
export OMP_PLACES=cores
export OMP_PROC_BIND=close

echo "Node: $SLURMD_NODENAME, CPUs for this job: $(nproc)"

# The book as it is (0.44 MB), many repetitions because each one is so short
./frankenstein_timing ASCII-Frankenstein.txt 1 1000 > results_1copy.csv
# The book glued together 100 times (44 MB): enough work for 128 threads
./frankenstein_timing ASCII-Frankenstein.txt 100 50 > results_100copies.csv
# 1000 times (440 MB): too big for all the node's caches together, so it measures RAM speed
./frankenstein_timing ASCII-Frankenstein.txt 1000 10 > results_1000copies.csv

echo; echo "1 copy:";       cat results_1copy.csv
echo; echo "100 copies:";   cat results_100copies.csv
echo; echo "1000 copies:";  cat results_1000copies.csv

# Make the plot here if Python with matplotlib is available, otherwise copy the CSVs to your Mac
python3 plot_speedup.py speedup.png \
    results_1copy.csv "1 copy (0.44 MB)" \
    results_100copies.csv "100 copies (44 MB)" \
    results_1000copies.csv "1000 copies (440 MB)" \
    || echo "No matplotlib here: plot on your Mac instead (see EXPLAINED.md part 11)"
