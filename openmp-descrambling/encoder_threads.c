// Thread-keyed encoder: the shift applied to a character depends on how many
// threads are running AND on which thread handles that character.
// Usage:  OMP_NUM_THREADS=4 ./encoder_threads message.txt > secret.txt
#include <stdio.h>
#include <omp.h>
#include "read_file.h"

// Only the 95 printable ASCII characters ' ' (32) to '~' (126) are shifted,
// and they wrap around, so the secret is still printable text.
#define FIRST 32
#define COUNT 95

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s plain.txt > secret.txt\n", argv[0]);
        return 1;
    }
    long n;
    char *text = read_file(argv[1], &n);

    #pragma omp parallel
    {
        int id  = omp_get_thread_num();          // 0, 1, ..., T-1
        int T   = omp_get_num_threads();         // team size
        int key = (T + id) % COUNT;              // this thread's private shift

        // schedule(static): character i always goes to the same thread for a
        // given T, so the receiver can rebuild exactly the same keys.
        #pragma omp for schedule(static)
        for (long i = 0; i < n; i++) {
            int c = (unsigned char) text[i];
            if (c >= FIRST && c < FIRST + COUNT)
                text[i] = FIRST + (c - FIRST + key) % COUNT;
        }
    }

    fwrite(text, 1, n, stdout);                  // the secret goes to stdout
    fprintf(stderr, "encoded %ld bytes with %d threads\n", n, omp_get_max_threads());
    free(text);
    return 0;
}
