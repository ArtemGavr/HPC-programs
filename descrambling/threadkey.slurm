#!/bin/bash

# Part 4: the thread-keyed encoder/decoder pair
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --mem=1G
#SBATCH --job-name="threadkey"
#SBATCH -o threadkey.%J.out
#SBATCH -e threadkey.%J.err
#SBATCH -t 00:05:00
#SBATCH -p test

module load gcc

gcc -O2 -Wall -fopenmp -o encoder_threads encoder_threads.c
gcc -O2 -Wall -fopenmp -o decoder_threads decoder_threads.c

export OMP_DYNAMIC=false      # the runtime must give us exactly the threads we ask for

echo "Plain text:"
cat message.txt
echo

for TE in 1 4 8; do
    OMP_NUM_THREADS=$TE ./encoder_threads message.txt > secret_$TE.txt 2>/dev/null
    echo "######## Encoded with $TE threads ########"
    cat secret_$TE.txt
    for TD in 1 2 3 4 5 8; do
        printf "decoded with %d threads: " $TD
        OMP_NUM_THREADS=$TD ./decoder_threads secret_$TE.txt 2>/dev/null
    done
    echo
done
