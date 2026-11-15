// Thread-keyed decoder: undoes encoder_threads.c, but only if it runs with
// the same number of threads as the encoder did.
// Usage:  OMP_NUM_THREADS=4 ./decoder_threads secret.txt
#include <stdio.h>
#include <omp.h>
#include "read_file.h"

#define FIRST 32
#define COUNT 95

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s secret.txt\n", argv[0]);
        return 1;
    }
    long n;
    char *text = read_file(argv[1], &n);

    #pragma omp parallel
    {
        int id  = omp_get_thread_num();
        int T   = omp_get_num_threads();
        int key = (T + id) % COUNT;              // same formula as the encoder

        #pragma omp for schedule(static)         // same split as the encoder
        for (long i = 0; i < n; i++) {
            int c = (unsigned char) text[i];
            if (c >= FIRST && c < FIRST + COUNT)
                text[i] = FIRST + (c - FIRST - key + COUNT) % COUNT;   // shift back
        }
    }

    fwrite(text, 1, n, stdout);
    fprintf(stderr, "decoded %ld bytes with %d threads\n", n, omp_get_max_threads());
    free(text);
    return 0;
}
