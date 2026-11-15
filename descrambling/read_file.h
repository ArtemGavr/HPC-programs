// read_file.h: one helper shared by the file-based programs.
// It reads a whole file into memory and tells you how many bytes it has.
#ifndef READ_FILE_H
#define READ_FILE_H

#include <stdio.h>      // fopen, fseek, ftell, fread, fclose, perror
#include <stdlib.h>     // malloc, exit

static char *read_file(const char *path, long *n_out)
{
    FILE *f = fopen(path, "rb");             // "rb" = read, binary (no newline translation)
    if (f == NULL) { perror(path); exit(1); }

    // The quick way to get the size: jump to the end, ask where we are, jump back.
    fseek(f, 0, SEEK_END);
    long n = ftell(f);                       // position at the end = number of bytes
    fseek(f, 0, SEEK_SET);
    if (n < 0) { perror("ftell"); exit(1); }

    char *buf = malloc(n + 1);               // +1 for a '\0' so it can be printed with %s
    if (buf == NULL) { fprintf(stderr, "out of memory\n"); exit(1); }

    if (fread(buf, 1, n, f) != (size_t) n) { // read n items of 1 byte each
        fprintf(stderr, "could not read all of %s\n", path);
        exit(1);
    }
    buf[n] = '\0';
    fclose(f);

    *n_out = n;                              // hand the size back through the pointer
    return buf;
}

#endif
