#!/bin/bash

# Parts 1-3: serial encoder, serial decoder, OpenMP decoder with 1..8 threads
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --mem=1G
#SBATCH --job-name="descramble"
#SBATCH -o descramble.%J.out
#SBATCH -e descramble.%J.err
#SBATCH -t 00:05:00
#SBATCH -p test

module load gcc

gcc -O2 -Wall -fopenmp -o encoder     encoder.c
gcc -O2 -Wall -fopenmp -o decoder     decoder.c
gcc -O2 -Wall -fopenmp -o decoder_omp decoder_omp.c

echo "######## Part 1: serial encoder ########"
./encoder
SHEET='Zpv!uijol!zpvs!qbjo!boe!zpvs!ifbsucsfbl!bsf!voqsfdfefoufe!jo!uif!ijtupsz!pg!uif!xpsme-!cvu!uifo!zpv!sfbe/'
if [ "$(./encoder | tail -n 1)" = "$SHEET" ]; then
    echo "-> matches the scrambled text on the sheet"
else
    echo "-> does NOT match the sheet"
fi
echo

echo "######## Part 2: serial decoder ########"
./decoder
echo

echo "######## Part 3: OpenMP decoder, each thread count run 3 times ########"
for T in 1 2 4 8; do
    for run in 1 2 3; do
        echo "==== OMP_NUM_THREADS=$T, run $run ===="
        OMP_NUM_THREADS=$T ./decoder_omp
        echo
    done
done

echo "######## Part 3 summary: which thread printed, in order (t2 x13 = thread 2 printed 13 lines in a row) ########"
for T in 2 4 8; do
    for run in 1 2 3; do
        printf "T=%d run %d: " $T $run
        OMP_NUM_THREADS=$T ./decoder_omp | awk '/^thread/{print $2}' | uniq -c \
            | awk '{printf "t%s x%s, ", $2, $1} END {print ""}'
    done
done
