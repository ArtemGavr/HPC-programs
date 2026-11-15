# Message Descrambling, explained step by step

This is the long version. `WALKTHROUGH.md` gives the answers, and this file explains *why* each answer is true and what every line of the code does. Read it in order, because each part builds on the one before.

**The whole exercise in one sentence:** to a computer a letter is just a small number, so "scrambling" a message is adding a number to every letter, and because every letter can be changed on its own, the work splits perfectly between threads.

Contents

1. [Characters are numbers](#1-characters-are-numbers)
2. [Strings in C: arrays of characters with an end marker](#2-strings-in-c)
3. [Part 1: the serial encoder (whole code, big picture, line by line)](#3-part-1-the-serial-encoder)
4. [Part 2: the serial decoder](#4-part-2-the-serial-decoder)
5. [OpenMP theory for this exercise](#5-openmp-theory-for-this-exercise)
6. [Part 3: the OpenMP decoder, and what you observe](#6-part-3-the-openmp-decoder)
7. [Part 4 theory: keys that depend on the threads](#7-part-4-theory-keys-that-depend-on-the-threads)
8. [The file reader `read_file.h`, line by line](#8-the-file-reader-read_fileh)
9. [Part 4: the thread-keyed encoder and decoder](#9-part-4-the-thread-keyed-encoder-and-decoder)
10. [Part 5 theory: why a short text can't keep threads busy](#10-part-5-theory)
11. [Part 5: the timing program and the plot](#11-part-5-the-timing-program-and-the-plot)
12. [The SLURM scripts, line by line](#12-the-slurm-scripts-line-by-line)
13. [The commands to run, line by line](#13-the-commands-to-run)
14. [Glossary](#14-glossary)

---

## 1. Characters are numbers

### 1.1 A computer only stores numbers

Memory is a row of boxes, and each box (a **byte**) holds a whole number from 0 to 255. There are no letters in memory. To store text, everyone agrees on a table that gives each character a number. The basic table is called **ASCII**, and it covers 128 characters.

Some entries you'll meet in this exercise:

| character | number | | character | number |
|---|---|---|---|---|
| space `' '` | 32 | | `'Y'` | 89 |
| `'!'` | 33 | | `'Z'` | 90 |
| `','` | 44 | | `'a'` | 97 |
| `'-'` | 45 | | `'o'` | 111 |
| `'.'` | 46 | | `'p'` | 112 |
| `'/'` | 47 | | `'u'` | 117 |
| `'A'` | 65 | | `'v'` | 118 |
| `'B'` | 66 | | `'~'` | 126 |

Two things are worth noticing. The letters are in order (`'a'` is 97, `'b'` is 98, and so on), so "the next letter" is just "plus one". And punctuation has numbers too, sitting right next to other punctuation.

In C, a character in single quotes *is* its number. `'A'` and `65` are the same value, so `'A' + 1` is 66, which is `'B'`.

### 1.2 The Caesar cipher

A **Caesar cipher** replaces each letter with the letter a fixed number of places further on. The fixed number is the **key**. With key 1, A becomes B, c becomes d, and so on.

The sheet's opening point is that a coded message only makes sense if you have some outside information. Here that information is "the key is 1, and you subtract it". Without it, `Zpv!uijol` is noise. With it, the message reads back.

### 1.3 Worked example: encoding "You "

The encoder adds 1 to the number of every character, including the space:

| character | number | + 1 | new character |
|---|---|---|---|
| `Y` | 89 | 90 | `Z` |
| `o` | 111 | 112 | `p` |
| `u` | 117 | 118 | `v` |
| space | 32 | 33 | `!` |

So "You " becomes "Zpv!", which is exactly how the scrambled text on the sheet starts. That also explains the odd look of the scrambled text:

- every `!` in it was a space (32 + 1 = 33),
- the `-` was a comma (44 + 1 = 45),
- the `/` at the end was a full stop (46 + 1 = 47).

This encoder doesn't "wrap around" the alphabet like a textbook Caesar cipher, where Z would become A. It shifts the raw number, so `'Z'` would become `'['` (91). For this message that never matters, but it explains why punctuation changes too.

---

## 2. Strings in C

### 2.1 A string is an array of characters plus an end marker

C has no special "string" type. A piece of text is an **array of `char`**, which is a row of bytes, one character per byte. After the last real character, C puts one extra byte with the value 0, written `'\0'`. This byte is the **end marker** (also called the null terminator). It is how functions know where the text stops.

```c
char word[] = "Hi!";
```

This creates an array of **4** bytes, not 3:

| index | 0 | 1 | 2 | 3 |
|---|---|---|---|---|
| character | `'H'` | `'i'` | `'!'` | `'\0'` |
| number | 72 | 105 | 33 | 0 |

The empty brackets `[]` mean "make the array exactly as big as the text needs", and the compiler counts for you.

### 2.2 Two ways to get the length: `sizeof` and `strlen`

The sheet asks you to find the length "programmatically", which means the program works it out instead of you typing `105`. There are two ways.

**`sizeof(word)`** is worked out by the *compiler*. It is the number of bytes the whole array takes up. For `"Hi!"` that is 4, because it includes the `'\0'`. So the number of real characters is `sizeof(word) - 1`.

**`strlen(word)`** is a *function* that runs while the program runs. It starts at index 0 and counts characters until it meets the `'\0'`, and it does not count the `'\0'`. For `"Hi!"` it returns 3. It lives in `<string.h>`.

For the secret message, `sizeof` gives 106 and `strlen` gives 105, so both ways agree on 105 real characters.

**A trap to know about.** `sizeof` only gives the array size when it can see the real array. If you have a **pointer** to the text instead, such as `char *p = word;`, then `sizeof(p)` is the size of the pointer itself, which is 8 bytes on a 64-bit machine, whatever the text is. `strlen(p)` still works. This is why `strlen` is the safer habit, and why the file-reading parts later have to track the length themselves.

### 2.3 `%c` against `%s` in printf

The sheet's comment mentions a "subtle difference":

- `printf("%c", word[0]);` prints **one** character. You hand it one `char` value.
- `printf("%s", word);` prints a **whole string**. You hand it the start of the array, and printf walks along printing bytes until it hits the `'\0'`.

So `%s` depends completely on the end marker being there.

### 2.4 What goes wrong if you shift the end marker too

Say you loop over `sizeof(mymessage)` characters (106) instead of 105. The last step adds 1 to the `'\0'`, so it becomes byte 1, which is an invisible control character. Now the string has **no end marker**. `printf("%s")` keeps walking past the end of your array into whatever memory happens to come next. It prints garbage until it happens to meet a 0 byte, or the program crashes.

That is the "fix" part of Part 1: the loop must cover the 105 real characters and leave the 106th byte alone.

---

## 3. Part 1: the serial encoder

### 3.1 The whole program

Here is the finished `encoder.c`, so you can see all of it at once before we pull it apart.

```c
#include <stdio.h>      // printf
#include <string.h>     // strlen

// Start of program
int main(void)
{
    // A "character array": the letters of the message, one byte each,
    // followed by an invisible '\0' byte that marks where the text ends.
    // This is the original message (found with decoder.c), so encoding it
    // must give back exactly the scrambled text on the exercise sheet.
    char mymessage[] = "You think your pain and your heartbreak are unprecedented "
                       "in the history of the world, but then you read.";

    // Work out the length instead of typing 105 by hand. Two ways:
    //   sizeof(mymessage) is the size of the whole array in bytes, which
    //   includes the '\0' at the end, so subtract 1.
    //   strlen(mymessage) counts characters up to (not including) the '\0'.
    int n_sizeof = sizeof(mymessage) - 1;
    int n        = strlen(mymessage);
    printf("Length: sizeof says %d, strlen says %d\n", n_sizeof, n);

    // The MISSING CODE: shift every character up by one (A->B, c->d, ' '->'!').
    // The loop stops before the '\0', so the end marker survives.
    for (int i = 0; i < n; i++) {
        mymessage[i] = mymessage[i] + 1;
    }

    printf("%s\n", mymessage);   // %s prints characters until it meets '\0'

    return 0;                    // Exit program
}
```

### 3.2 The big picture: what it does, and how

The program has one job: take a message and shift every character up by one.

It does that in four steps, in this order:

1. **Store the message** in a character array. I replaced the sheet's placeholder with the real message (found in Part 2), so the output can be checked against the sheet.
2. **Work out the length** in two ways, `sizeof - 1` and `strlen`, and print both so you can see that they agree.
3. **Loop over the 105 real characters**, adding 1 to each. This is the sheet's "MISSING CODE".
4. **Print the result** with `%s`.

**Why it's correct:** the loop runs from `i = 0` up to `i = n - 1`, where `n` is 105. So it touches indexes 0 to 104, which are exactly the real characters. Index 105, the end marker, is never changed, so `%s` still knows where to stop.

**How you know it's right:** the last line it prints is exactly the scrambled text on the sheet. `descramble.slurm` compares the two and prints `-> matches the scrambled text on the sheet`.

### 3.3 Line by line

```c
#include <stdio.h>      // printf
#include <string.h>     // strlen
```

`#include` pastes in a **header file**, which tells the compiler which library functions exist and how to call them. `<stdio.h>` ("standard input/output") gives `printf`. `<string.h>` gives `strlen`.

The sheet's version also had `#include <omp.h>`, but the serial encoder uses no OpenMP functions, so I left it out. Leaving it in would do no harm.

```c
int main(void)
{
```

Every C program starts running at `main`. `int` means it hands back a whole number to whoever started it (0 means "all went fine"). `(void)` means it takes no inputs. The sheet writes `main()`, which works too; `(void)` is the stricter, modern way to say "no arguments".

```c
    char mymessage[] = "You think your pain and your heartbreak are unprecedented "
                       "in the history of the world, but then you read.";
```

This makes a character array and fills it with the message, as in section 2.1. The text is split over two lines only to keep the line short. In C, two string literals written next to each other, `"abc" "def"`, are glued into one, `"abcdef"`. Note that the first piece ends with a space, so the words don't run together.

```c
    int n_sizeof = sizeof(mymessage) - 1;
    int n        = strlen(mymessage);
```

The two ways to get the length from section 2.2. `sizeof(mymessage)` is 106, so `n_sizeof` is 105. `strlen(mymessage)` walks to the `'\0'` and returns 105. We use `n` in the loop.

(Side note: `strlen` actually returns a `size_t`, an unsigned whole-number type made for sizes. Storing it in an `int` is fine for a 105-character message.)

```c
    printf("Length: sizeof says %d, strlen says %d\n", n_sizeof, n);
```

`%d` is the placeholder for an `int`. Each `%d` is replaced, in order, by the values listed after the text. `\n` is a newline. Output: `Length: sizeof says 105, strlen says 105`.

```c
    for (int i = 0; i < n; i++) {
        mymessage[i] = mymessage[i] + 1;
    }
```

The missing loop. A C `for` loop has three parts inside the brackets, separated by semicolons:

- `int i = 0` runs once, at the start. It makes a counter `i` set to 0.
- `i < n` is checked before every pass. When it becomes false, the loop stops.
- `i++` runs after every pass. It adds 1 to `i`.

So `i` takes the values 0, 1, 2, ..., 104, and the body runs 105 times.

The body reads character number `i`, adds 1 to its number, and stores it back in the same place. On the first pass, `mymessage[0]` is `'Y'` (89), and it becomes 90, which is `'Z'`.

```c
    printf("%s\n", mymessage);
```

Prints the whole array as text, up to the untouched `'\0'`.

```c
    return 0;
}
```

Ends `main` and tells the operating system the program succeeded.

---

## 4. Part 2: the serial decoder

### 4.1 The whole program

```c
#include <stdio.h>      // printf
#include <string.h>     // strlen

int main(void)
{
    // The scrambled message from the exercise sheet.
    char message[] = "Zpv!uijol!zpvs!qbjo!boe!zpvs!ifbsucsfbl!bsf!voqsfdfefoufe"
                     "!jo!uif!ijtupsz!pg!uif!xpsme-!cvu!uifo!zpv!sfbe/";

    int n = strlen(message);     // 105 characters, '\0' not counted

    // Undo the encoder: it added 1 to every character, so subtract 1.
    for (int i = 0; i < n; i++) {
        message[i] = message[i] - 1;
    }

    printf("Decoded %d characters:\n%s\n", n, message);
    return 0;
}
```

### 4.2 The big picture

The decoder is the encoder run backwards. The encoder added 1, so the decoder subtracts 1. Undoing every step of a process in reverse is how you undo any cipher; here there's only one step.

1. Store the scrambled text from the sheet.
2. Measure it with `strlen`: 105.
3. Subtract 1 from each of the 105 characters.
4. Print the result.

### 4.3 Line by line

Only the parts that differ from the encoder:

```c
    char message[] = "Zpv!uijol!zpvs!qbjo!boe!zpvs!ifbsucsfbl!bsf!voqsfdfefoufe"
                     "!jo!uif!ijtupsz!pg!uif!xpsme-!cvu!uifo!zpv!sfbe/";
```

The scrambled text, copied from the sheet and split over two lines in the same way. This time the second piece starts with the `!`, which was the space.

You can paste this text straight into C because it contains no `"` or `\` characters. Those two would need a backslash in front of them inside a C string.

```c
        message[i] = message[i] - 1;
```

The only real change: minus instead of plus. Worked example for the first four characters:

| scrambled | number | − 1 | decoded |
|---|---|---|---|
| `Z` | 90 | 89 | `Y` |
| `p` | 112 | 111 | `o` |
| `v` | 118 | 117 | `u` |
| `!` | 33 | 32 | space |

```c
    printf("Decoded %d characters:\n%s\n", n, message);
```

One printf with two placeholders: `%d` takes `n` and `%s` takes `message`.

### 4.4 The answer

```
Decoded 105 characters:
You think your pain and your heartbreak are unprecedented in the history of the world, but then you read.
```

It is a quote by James Baldwin, and it is a fitting choice for an exercise that ends with you encoding a whole novel.

---

## 5. OpenMP theory for this exercise

You met these ideas in the Thread Test and Array Timing exercises. This is a refresher, aimed at what matters here.

### 5.1 Threads share one memory

A **thread** is an independent worker inside your program. All threads of one program see the **same memory**, so they can all read and write the same `message` array. That's what "shared memory parallelism" means.

Sharing is convenient, and it is also dangerous. If two threads write the same byte at the same time, or one reads while another writes, the result depends on timing. That is called a **race condition**.

### 5.2 Why this loop is safe to split

Look at the loop body: `message[i] = message[i] - 1;`. Pass number `i` only reads and writes byte `i`. Pass 5 never looks at byte 6. So the passes are **independent**: they can run in any order, or all at once, and the result is the same.

Splitting it between threads therefore can't cause a race, as long as each `i` is handled by exactly one thread. `#pragma omp parallel for` guarantees that.

Loops like this one, where every step is independent, are called **embarrassingly parallel**. That just means "no cleverness needed to split them".

### 5.3 What `#pragma omp parallel for` does

```c
#pragma omp parallel for
for (int i = 0; i < n; i++) { ... }
```

1. **Fork:** the program starts a team of threads. The size of the team comes from `OMP_NUM_THREADS`.
2. **Split:** the values of `i` (0 to 104) are shared out among the team.
3. **Work:** each thread runs the loop body for its own values of `i`.
4. **Join:** at the end of the loop there is an invisible **barrier**. Every thread waits there until all threads have finished, and then only the original thread carries on.

The barrier is why the `printf` *after* the loop always sees the fully decoded message.

### 5.4 How the 105 characters are split: the static schedule

GCC's default schedule for `parallel for` is **static**. That means each thread gets **one solid block** of consecutive iterations, as equal in size as possible, decided before the loop starts.

The arithmetic GCC uses, worked for `n = 105`:

- `q = n / T` (whole-number division) is the basic block size.
- `r = n % T` (the remainder) is how many iterations are left over.
- The first `r` threads get one extra iteration each.

| threads T | q = 105 / T | r = 105 % T | blocks |
|---|---|---|---|
| 1 | 105 | 0 | t0: 0-104 |
| 2 | 52 | 1 | t0: 0-52 (53), t1: 53-104 (52) |
| 3 | 35 | 0 | t0: 0-34, t1: 35-69, t2: 70-104 |
| 4 | 26 | 1 | t0: 0-26 (27), t1: 27-52, t2: 53-78, t3: 79-104 |
| 8 | 13 | 1 | t0: 0-13 (14), t1: 14-26, ..., t7: 92-104 |

I checked these against real runs of `decoder_omp`, and they match.

### 5.5 Private and shared variables

Inside a parallel loop:

- **The loop counter `i` is private.** Every thread has its own copy, so they don't trample each other's counters.
- **Variables declared inside the loop body are private.** That covers `before` and `after` in `decoder_omp.c`. Each thread gets its own, and each pass gets a fresh one.
- **Variables declared before the loop are shared.** That covers `message` and `n`. All threads see the same array, and that is what we want here, because they each write to different parts of it.

### 5.6 Printing from many threads

`printf` is **thread-safe**. Before writing, it locks the output, so one thread's line is never cut in half by another thread's line. But nothing controls *which* thread gets the lock first. So whole lines appear in an unpredictable order.

That lock also means the threads queue up to print. Printing is the slow part here, so with printing turned on, the parallel version spends most of its time waiting in that queue.

---

## 6. Part 3: the OpenMP decoder

### 6.1 The whole program

```c
#include <stdio.h>      // printf
#include <string.h>     // strlen
#include <omp.h>        // omp_get_thread_num, omp_get_num_threads, omp_get_max_threads

int main(void)
{
    char message[] = "Zpv!uijol!zpvs!qbjo!boe!zpvs!ifbsucsfbl!bsf!voqsfdfefoufe"
                     "!jo!uif!ijtupsz!pg!uif!xpsme-!cvu!uifo!zpv!sfbe/";
    int n = strlen(message);

    // Each character can be decoded on its own, so the loop is split between
    // the threads. Every thread gets its own copy of i, before and after.
    #pragma omp parallel for
    for (int i = 0; i < n; i++) {
        char before = message[i];
        char after  = before - 1;
        message[i]  = after;
        printf("thread %2d of %2d decoded message[%3d]: '%c' -> '%c'\n",
               omp_get_thread_num(), omp_get_num_threads(), i, before, after);
    }
    // The parallel region has ended here, so all threads have finished.

    printf("\nDecoded with up to %d threads:\n%s\n", omp_get_max_threads(), message);
    return 0;
}
```

### 6.2 The big picture

It is the serial decoder plus two things:

1. **One pragma** (`#pragma omp parallel for`) splits the loop between threads, as in section 5.3.
2. **One printf per character** reports which thread decoded it and what it became, as the sheet asks: "prints the current thread number and decryption result as the decoder progresses".

After the loop, the barrier guarantees that every character is done, and the program prints the whole message.

### 6.3 Line by line

```c
#include <omp.h>
```

Gives the OpenMP library functions: `omp_get_thread_num`, `omp_get_num_threads` and `omp_get_max_threads`. You must also compile with `-fopenmp`. Without that flag, gcc ignores the pragma and the program runs on one thread.

```c
    #pragma omp parallel for
    for (int i = 0; i < n; i++) {
```

A **pragma** is a note to the compiler. This one says "run the next `for` loop in parallel". It must sit directly above the loop, and the loop has to be in the simple "canonical" shape from the lecture, `for (start; i < end; i++)`, so OpenMP can count the iterations before it starts.

```c
        char before = message[i];
        char after  = before - 1;
        message[i]  = after;
```

The decoding, split into three steps so we keep the old and new characters for printing. `before` and `after` are declared inside the loop, so each thread has its own copies (section 5.5).

Why not just print `message[i]` afterwards? We could, because only this thread touches byte `i`. Keeping both in local variables just makes the printout clear: you see what came in and what went out.

```c
        printf("thread %2d of %2d decoded message[%3d]: '%c' -> '%c'\n",
               omp_get_thread_num(), omp_get_num_threads(), i, before, after);
```

- `omp_get_thread_num()` returns this thread's number, from 0 to T−1.
- `omp_get_num_threads()` returns the size of the current team, which is T.
- `%2d` and `%3d` print a whole number padded to 2 or 3 characters wide, so the columns line up.
- `%c` prints one character, as in section 2.3.

A typical line: `thread  2 of  4 decoded message[ 53]: 'o' -> 'n'`.

```c
    printf("\nDecoded with up to %d threads:\n%s\n", omp_get_max_threads(), message);
```

This runs after the loop, so we're back to one thread. Calling `omp_get_num_threads()` here would return 1, because outside a parallel region the team is just the main thread (you saw that in the Thread Test). `omp_get_max_threads()` returns the team size a parallel region *would* get, which is your `OMP_NUM_THREADS`.

### 6.4 What you observe (real runs)

I ran it 3 times each with 2, 4 and 8 threads on a 4-core machine. To make the pattern easy to see, `descramble.slurm` squeezes each run down to "which thread printed, and how many lines in a row". For example, `t1 x26` means thread 1 printed 26 lines in a row.

```
T=2 run 1: t0 x53, t1 x52
T=2 run 2: t0 x21, t1 x52, t0 x32
T=2 run 3: t0 x44, t1 x26, t0 x1, t1 x1, t0 x8, t1 x25
T=4 run 1: t0 x27, t1 x26, t3 x26, t2 x26
T=4 run 2: t1 x26, t3 x26, t2 x26, t0 x27
T=4 run 3: t0 x27, t3 x26, t1 x26, t2 x26
T=8 run 1: t1 x1, t5 x13, t2 x1, t0 x14, t2 x12, t6 x13, t7 x13, t1 x12, t3 x5, t4 x13, t3 x8
T=8 run 2: t7 x13, t3 x5, t1 x1, t6 x13, t1 x12, t2 x13, t3 x1, t0 x1, t4 x13, t0 x13, t3 x7, t5 x13
```

What to write down as your observations:

1. **The decoded message is correct every time**, with any thread count. The loop has no race (section 5.2).
2. **With one thread the lines come out in order, 0 to 104.** With two or more threads the order changes from run to run. Threads run at the same time, and whichever grabs the printf lock first prints first.
3. **The thread blocks match the static schedule** (section 5.4). With 4 threads the counts are 27, 26, 26, 26: thread 0 gets the extra character because 105 = 4 × 26 + 1.
4. **Within one thread, the order is always kept.** Thread 2 always prints 53, 54, 55, ... in order. Only the blocks shuffle against each other.
5. **Whole blocks often print in one go with few threads.** A block of 26 characters takes a few microseconds, and starting a thread takes about as long, so one thread often finishes before the next one even begins. With 8 threads on 4 cores, the threads have to share cores, the operating system swaps them in and out, and the blocks get chopped up and mixed.
6. **Thread 0 doesn't necessarily go first.** In `T=4 run 2`, thread 1 printed first and thread 0 printed last.
7. **With more threads than characters, some threads get nothing.** With `OMP_NUM_THREADS=200` there are still exactly 105 lines, so 95 threads had no character to decode.
8. **It isn't faster.** Subtracting 1 takes about a nanosecond and a printf takes about a microsecond, roughly 1000 times longer. Printing dominates, and threads take turns to print. This short text can't show a speedup, which is exactly the point the sheet makes in Part 5.

On Hamilton each thread gets its own core, so you may see even more "one whole block at a time" runs. The order will still change between runs.

---

## 7. Part 4 theory: keys that depend on the threads

### 7.1 What the sheet wants

"An encoder/decoder pair which only produces the correct message if the number of threads used by the sender is the same as the number used by the receiver."

So the number of threads has to become part of the **key**. If the receiver uses a different number, they are using the wrong key, and the result is garbage.

### 7.2 The idea: every thread uses its own shift

In the parallel encoder, each thread handles one block of characters. Let each thread shift its block by its own amount:

```
key = T + id
```

where `T` is the number of threads and `id` is the thread's number (0 to T−1). Two different things now depend on `T`:

1. **The size of every key.** With 4 threads the keys are 4, 5, 6, 7. With 3 threads they are 3, 4, 5.
2. **Which characters each key is used on**, because the block boundaries move with `T` (section 5.4).

A decoder with the same `T` gets the same blocks *and* the same keys, so it subtracts exactly what was added. A decoder with a different `T` gets both wrong.

### 7.3 Keeping the secret printable: wrapping at 95

If we just added the key to the raw number, characters near the top of the table would land on unprintable values. That makes the secret file awkward to look at.

So the pair only shifts the **95 printable characters**, from space (32) to `~` (126), and **wraps around** like a clock face with 95 positions. Going past `~` lands you back at space. Anything else, such as the newline at the end of the file, is left alone.

The recipe for one character `c` with key `k`:

1. `c − 32`: turn the character into a position from 0 to 94.
2. `+ k`: move forward `k` positions.
3. `% 95`: wrap around. `%` is the **remainder** after division, so 97 % 95 = 2.
4. `+ 32`: turn the position back into a character.

**Worked example, encoding:** thread 0 of 4 has key (4 + 0) % 95 = 4, and the first character is `'Y'` (89).

- 89 − 32 = 57
- 57 + 4 = 61
- 61 % 95 = 61 (no wrap needed)
- 61 + 32 = 93, which is `']'`

And that's how the 4-thread secret starts: `]sy$`.

**Worked example with a wrap:** `'~'` (126) with key 4.

- 126 − 32 = 94
- 94 + 4 = 98
- 98 % 95 = 3 (it went past the end and wrapped)
- 3 + 32 = 35, which is `'#'`

### 7.4 Decoding, and a trap with `%` and negative numbers

Decoding moves backwards: subtract the key instead of adding it. Undo the wrap example: `'#'` (35) with key 4.

- 35 − 32 = 3
- 3 − 4 = −1

In maths, −1 mod 95 is 94. **But in C, `-1 % 95` is −1**, because C keeps the sign of the left-hand number. That would give a wrong character.

The fix is to add 95 before taking the remainder. Adding a whole lap of the clock face doesn't change where you end up, but it keeps the number positive:

- 3 − 4 + 95 = 94
- 94 % 95 = 94
- 94 + 32 = 126, which is `'~'`. That's correct.

This works because the key is always below 95: `key = (T + id) % 95` guarantees it. So `c − 32 − key` is at least −94, and adding 95 always makes it zero or more.

### 7.5 Why `schedule(static)` must be written out

The whole trick relies on character `i` going to the **same thread** in the encoder and the decoder. The static schedule (section 5.4) decides that from `n` and `T` alone, before the loop starts, so it is repeatable.

Other schedules wouldn't work. With `schedule(dynamic)`, threads grab chunks as they become free, so which thread gets which character depends on timing. Even with the same `T`, the decoder would pick different keys, and the message would come out scrambled.

GCC uses static by default anyway, but the pair says `schedule(static)` explicitly. Then nobody can break it by changing the default (for example with the `OMP_SCHEDULE` environment variable).

### 7.6 Worked example: what a wrong thread count does

Encode with 4 threads, decode with 3:

| characters | encoder thread (T=4) | encoder key | decoder thread (T=3) | decoder key | net shift |
|---|---|---|---|---|---|
| 0-26 | t0 | 4 | t0 | 3 | +1 |
| 27-34 | t1 | 5 | t0 | 3 | +2 |
| 35-52 | t1 | 5 | t1 | 4 | +1 |
| 53-69 | t2 | 6 | t1 | 4 | +2 |
| 70-78 | t2 | 6 | t2 | 5 | +1 |
| 79-104 | t3 | 7 | t2 | 5 | +2 |

Every character ends up shifted by 1 or 2 too many. Look at the real output: the start reads `Zpv!uijol!zpvs`, which is the sheet's original +1 scramble. Further on it has `zpvt"jgctvdt`, the +2 parts.

### 7.7 Is it really "only if the same"?

I tested every combination of encoder threads (1 to 128) against decoder threads (1 to 128), which is 16,384 pairs, on the 105-character message:

- **Same thread count:** the message came back perfectly in all 128 cases.
- **Different thread counts, both 47 or fewer:** **not a single character** came out right in any pair.
- **Different thread counts, up to 128:** the best case got 34 of the 105 characters right (encoder 69 threads, decoder 128). Never the whole message.

Why exactly 47? Using fewer threads means bigger blocks, so every character sits with a thread whose number is the same or lower. So the side with fewer threads always has a smaller key (smaller T, and an id that's no bigger), and two keys can never match. That holds as long as no key wraps past 95. The largest key is T + (T − 1) = 2T − 1, and that stays below 95 as long as T ≤ 47. Above that, keys wrap round the clock face and a few can match by accident, but never all of them.

### 7.8 Honest weak points

This is a teaching cipher, not real security:

- **Anyone who knows the method can just try every thread count**, from 1 to 128. It takes a fraction of a second.
- **The two sides must agree on more than T.** Both need `schedule(static)`, and `OMP_DYNAMIC` must be off. With dynamic adjustment on, OpenMP is allowed to give you fewer threads than you asked for. `threadkey.slurm` sets `OMP_DYNAMIC=false`.
- **The exact static split is up to the compiler.** The OpenMP standard only promises "blocks of roughly equal size", and GCC's rule is the one in section 5.4. A program built with the Intel or LLVM compiler could split 105 characters differently, and then decoding fails even with the same `T`.

---

## 8. The file reader `read_file.h`

Parts 4 and 5 read their text from a file instead of having it typed into the program. Three programs need the same "read a whole file" code, so it lives once in a small header file that each of them includes.

### 8.1 The whole file

```c
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
```

### 8.2 The big picture

The function takes the name of a file, and gives back two things: the file's contents in memory, and the number of bytes.

The challenge is the length. C has no portable "how big is this file?" function. The sheet suggests the quick trick:

1. **Open** the file.
2. **Jump to the end** (`fseek`).
3. **Ask where you are** (`ftell`). At the end, your position is the file's size in bytes.
4. **Jump back to the start**.
5. **Make room** for that many bytes plus one (`malloc`).
6. **Read** the whole file into that room (`fread`).
7. **Add a `'\0'`** at the end, so the text can also be printed with `%s`.
8. **Close** the file, and hand back the memory and the size.

A C function can only `return` one value, so it returns the memory, and passes the size back through a pointer (section 8.4).

### 8.3 Why "technically incorrect"?

The sheet calls `fseek`/`ftell` "quick and technically incorrect". Two reasons:

- On Windows, a file opened in *text* mode turns each two-byte line ending (`\r\n`) into one byte (`\n`) as you read. Then `ftell`'s position doesn't match the number of bytes `fread` gives you. We open in **binary** mode (`"rb"`), and on Linux and macOS text and binary mode are the same anyway.
- The C standard doesn't promise that seeking to the end of a binary file works everywhere. On Linux and macOS it does.

The fully correct way is `fstat` from `<sys/stat.h>`, which asks the operating system for the size directly. For this exercise the quick way is fine, and the program checks that `fread` really read every byte.

### 8.4 Line by line

```c
#ifndef READ_FILE_H
#define READ_FILE_H
...
#endif
```

An **include guard**. `#include` pastes the file's text in, so if one program included it twice, the function would be defined twice and the compiler would refuse. The first time through, `READ_FILE_H` isn't defined yet, so everything up to `#endif` is kept and `READ_FILE_H` gets defined. A second time, `#ifndef` ("if not defined") is false, so everything is skipped. Every header should have one.

Note that the programs write `#include "read_file.h"` with quotes, not angle brackets. Quotes mean "look in my own folder first". Angle brackets mean "look in the system's folders".

```c
static char *read_file(const char *path, long *n_out)
```

Reading it piece by piece:

- `char *` is the **return type**: a pointer to a char, which here means "the address of the first byte of the text".
- `read_file` is the function's name.
- `const char *path` is the file name, passed as the address of its first character. `const` promises the function won't change it.
- `long *n_out` is the address of a `long` (a big whole number) **that belongs to the caller**. The function writes the size there. This is how C hands back a second result.
- `static` means "only visible inside this program file". Normally, defining a function in a header is a problem when several `.c` files of one program include it. `static` makes it safe. Each of our programs is a single `.c` file anyway.

```c
    FILE *f = fopen(path, "rb");
    if (f == NULL) { perror(path); exit(1); }
```

`fopen` opens the file and returns a `FILE *`, a handle that the other file functions use. `"rb"` means read, binary.

If the file doesn't exist, `fopen` returns `NULL`, a special "points nowhere" address. Always check it. `perror` prints your message followed by the system's reason, such as `ASCII-Frankenstein.txt: No such file or directory`. `exit(1)` stops the whole program and reports failure.

```c
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { perror("ftell"); exit(1); }
```

`fseek(f, 0, SEEK_END)` moves the file's reading position to "0 bytes from the end". `ftell` returns the current position, counted in bytes from the start, so here it returns the size. For the ASCII Frankenstein, `n` is 439,155. `fseek(f, 0, SEEK_SET)` moves back to "0 bytes from the start", ready to read. If something went wrong, `ftell` returns −1, so we check for that.

```c
    char *buf = malloc(n + 1);
    if (buf == NULL) { fprintf(stderr, "out of memory\n"); exit(1); }
```

`malloc(k)` asks the system for `k` bytes of memory and returns the address of the first one. We ask for one byte more than the file needs, to fit the `'\0'`. If there isn't enough memory it returns `NULL`.

`fprintf(stderr, ...)` is printf to the **error stream** instead of normal output. That matters here because the encoder's normal output *is* the secret, and we don't want error messages mixed into it.

Why `malloc` and not an array like `char buf[n]`? Ordinary local arrays live on the **stack**, which is small (often 8 MB). The 1000-copy test needs 440 MB, which has to come from the **heap**, the big pool `malloc` hands out.

```c
    if (fread(buf, 1, n, f) != (size_t) n) {
        fprintf(stderr, "could not read all of %s\n", path);
        exit(1);
    }
```

`fread(buf, 1, n, f)` reads up to `n` items, each 1 byte big, from `f` into `buf`. It returns how many items it actually read. If that isn't `n`, something went wrong, so we stop.

`(size_t) n` is a **cast**: it converts `n` to the same type `fread` returns, so the comparison is between like types and the compiler doesn't warn.

```c
    buf[n] = '\0';
    fclose(f);
```

Puts the end marker after the last byte (indexes 0 to n−1 hold the file, index n is the extra byte), then closes the file.

```c
    *n_out = n;
    return buf;
```

`*n_out` means "the variable that `n_out` points to". So this writes the size into the caller's variable. Then we return the address of the text.

In the caller it looks like this:

```c
long n;
char *text = read_file(argv[1], &n);
```

`&n` means "the address of `n`". The function writes through that address, so after the call `n` holds the size.

The caller must eventually `free(text)` to give the memory back. The programs do that at the end.

---

## 9. Part 4: the thread-keyed encoder and decoder

### 9.1 The whole encoder

```c
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
```

### 9.2 The whole decoder

```c
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
```

### 9.3 The big picture

The two programs are mirror images. The **only** real difference is one line: the encoder adds the key and the decoder subtracts it (plus the `+ COUNT` fix from section 7.4).

Each one does this, in order:

1. **Check** that it was given a file name.
2. **Read** the whole file into memory with `read_file`.
3. **Start a team of threads.** Each thread works out its own key from `T` and its own `id`.
4. **Split the characters** between the threads with a fixed static split. Each thread shifts its own characters by its own key, wrapping within the 95 printable characters.
5. **Write the result** to the screen (standard output). The encoder's output is meant to go into a file with `>`.

How you use them together:

```bash
OMP_NUM_THREADS=4 ./encoder_threads message.txt > secret.txt   # Alice encodes with 4
OMP_NUM_THREADS=4 ./decoder_threads secret.txt                  # Bob decodes with 4: works
OMP_NUM_THREADS=3 ./decoder_threads secret.txt                  # Eve guesses 3: garbage
```

`> secret.txt` is the shell's **redirect**. It sends the program's normal output into a file instead of the screen. The note "encoded 106 bytes with 4 threads" still shows on screen, because it goes to `stderr`, which `>` doesn't redirect. (106 bytes: the 105 characters plus the newline at the end of `message.txt`.)

### 9.4 Line by line (encoder)

```c
#define FIRST 32
#define COUNT 95
```

`#define` makes a named constant. Before compiling, every `FIRST` in the code is replaced by `32` and every `COUNT` by `95`. Names make the formula readable: 32 is the space character, and 95 is how many printable characters there are (32 to 126).

```c
int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s plain.txt > secret.txt\n", argv[0]);
        return 1;
    }
```

This form of `main` receives the **command-line arguments**:

- `argc` ("argument count") is how many words were typed, including the program's own name.
- `argv` ("argument vector") is a list of those words as strings. `char **` means "a pointer to a list of pointers to chars", which is a list of strings.

For `./encoder_threads message.txt`, `argc` is 2, `argv[0]` is `"./encoder_threads"` and `argv[1]` is `"message.txt"`. If the user forgot the file name, `argc` is 1, so we print how to use the program and stop with `return 1` (failure).

```c
    long n;
    char *text = read_file(argv[1], &n);
```

Reads the file named on the command line (section 8.4). Afterwards `text` points at the contents and `n` holds the size.

```c
    #pragma omp parallel
    {
```

This starts a team of threads, and **every** thread runs everything inside the curly braces. Notice there is no `for` on this pragma: it's a plain `parallel` region. We need that because each thread must first work out its own key, *before* the loop is shared out.

```c
        int id  = omp_get_thread_num();
        int T   = omp_get_num_threads();
        int key = (T + id) % COUNT;
```

These are declared inside the parallel region, so every thread has its **own** `id`, `T` and `key` (section 5.5). With 4 threads, thread 0 computes key 4, thread 1 key 5, thread 2 key 6 and thread 3 key 7.

The `% COUNT` keeps the key below 95. That only matters above 47 threads, and it's what makes the decoder's `+ COUNT` trick safe (section 7.4).

```c
        #pragma omp for schedule(static)
        for (long i = 0; i < n; i++) {
```

`#pragma omp for` (without `parallel`) means: "we already have a team, so share this loop's iterations among it". `schedule(static)` fixes the split, as section 7.5 explains. The counter is a `long` because files can be bigger than an `int` can count.

```c
            int c = (unsigned char) text[i];
```

Reads the character as a number from 0 to 255. Why the `(unsigned char)` cast? On most machines a plain `char` is **signed**, which means bytes 128 to 255 are read as −128 to −1. Our file is plain ASCII, but if it ever contained such bytes, the cast keeps the value positive so the range check below behaves.

```c
            if (c >= FIRST && c < FIRST + COUNT)
                text[i] = FIRST + (c - FIRST + key) % COUNT;
```

`&&` means "and". So: if `c` is between 32 and 126, shift it with the wrap-around recipe from section 7.3. Otherwise, as with the newline (10), leave it alone.

```c
        }
    }
```

The end of the loop, which has an invisible barrier, and then the end of the parallel region, where the team joins. After this point, only the main thread is running and every character is done.

```c
    fwrite(text, 1, n, stdout);
```

Writes `n` bytes, each 1 byte big, from `text` to standard output. Why not `printf("%s", text)`? Because `fwrite` writes exactly `n` bytes, whatever they are. If the data ever contained a 0 byte, `%s` would stop there.

```c
    fprintf(stderr, "encoded %ld bytes with %d threads\n", n, omp_get_max_threads());
    free(text);
    return 0;
```

A note to the error stream (`%ld` is the placeholder for a `long`), then give the memory back and finish.

### 9.5 Line by line (decoder): the one line that differs

```c
                text[i] = FIRST + (c - FIRST - key + COUNT) % COUNT;   // shift back
```

`- key` instead of `+ key`, and `+ COUNT` to stop the number going negative before `%` (section 7.4). Everything else is the same as the encoder, on purpose. If the two disagreed on the key formula or the schedule, even a matching `T` would fail.

### 9.6 Real output

From the `threadkey.slurm` test run:

```
######## Encoded with 4 threads ########
]sy$xlmro$}syv$temr$erh$}syw%mjfwygwjfp%fwj%zsuwjhjijszkj&ot&znk&noyzux &ul&znk&~vysk3'i|{'{olu'!v|'ylhk5
decoded with 1 threads: \rx#wklqn#|rxu#sdlq#dqg#|rxv$lievxfvieo$evi$yrtvigihiryji%ns%ymj%mnxytw~%tk%ymj%}uxrj2&h{z&znkt& u{&xkgj4
decoded with 2 threads: [qw"vjkpm"{qwt"rckp"cpf"{qwu#khduweuhdn#duh#xqsuhfhghpwhg#lq#wkh#klvwru|#ri#wkh#{svph0$fyx$xlir$}sy$vieh2
decoded with 3 threads: Zpv!uijol!zpvs!qbjo!boe!zpvt"jgctvdtfbl!bsf!voqsfdfefovgf"kp"vjg"jkuvqtz!pg!uif!yqtnf."dwv"vjgp"{qw"tgcf0
decoded with 4 threads: You think your pain and your heartbreak are unprecedented in the history of the world, but then you read.
decoded with 5 threads: Xnt~sghmj~xntq~o`hm~`mb}wmsq~gd`qsaqd`j~`qd}slnpcacbclsdc~hm~sgd}fgqrmpw}md}rfc}vnqkc*}`sr}rfcl}wms}pc_b,
decoded with 8 threads: Ukq{pdejg{ukqnzk\diz\i_ztjpnzc`\mo]m`\fz\l_yohjl_]_^_hn_^ychynb_ybclmhkrxh_xma^xphke]%x[nmxma]fwqgmwj]Y\&
```

Notice that encoding with **1 thread** uses key 1 + 0 = 1 for everything, which is exactly the sheet's original cipher. The new pair is a generalisation of the old one.

---

## 10. Part 5 theory

### 10.1 Speedup and efficiency, a quick recap

From the scaling lecture:

- **Speedup** with T threads is `S(T) = t(1) / t(T)`: how many times faster than one thread.
- **Efficiency** is `E(T) = S(T) / T`: what fraction of the ideal you got. 1.0 means perfect.

Worked example from my test machine, 1 copy of the book: t(1) = 0.1117 ms and t(4) = 0.0295 ms. So S(4) = 0.1117 / 0.0295 = 3.78, and E(4) = 3.78 / 4 = 0.945, which is 94.5%.

The plot puts speedup against threads, with a dashed "ideal" line where S = T.

### 10.2 Why 105 characters can't tax even one thread

Adding 1 to a byte takes well under a nanosecond. The compiler can even do 16 or 32 bytes at once with vector instructions (Flynn's SIMD, from lecture 1). So 105 characters take a few nanoseconds.

Starting a parallel region costs **microseconds**: waking the threads, handing out the work, and the barrier at the end. That's a thousand times more than the work itself. With 105 characters, parallel can only be slower.

### 10.3 Overhead against work: the grain size

Think of a team of movers. Moving one box isn't worth phoning 128 friends, because the phone calls take longer than the move. Moving a whole house is worth it.

For the encoder, let's estimate. My test machine encodes about 8 bytes per nanosecond on one core (from the `GB_per_s` column, 7.9 GB/s). The book is 439,155 bytes, so one encode takes about 0.11 ms on one thread.

| threads | bytes per thread | work per thread |
|---|---|---|
| 1 | 439,155 | ~110 µs |
| 8 | ~55,000 | ~14 µs |
| 32 | ~13,700 | ~3.5 µs |
| 128 | ~3,400 | under 1 µs |

The fork, share and barrier cost grows with the number of threads, and for 128 threads it is several microseconds. So for one copy of the book, the speedup should flatten somewhere past 16 to 32 threads, and may even drop. This is **strong scaling** (same problem, more workers) hitting its limit, just as Amdahl's law predicts: the fixed overhead acts like a serial part.

That's why `frankenstein_timing` can glue the book to itself (`copies`). 100 copies (44 MB) gives each of 128 threads about 340 KB, enough work for the overhead to fade into the background.

### 10.4 The real limit for big texts: memory bandwidth

The encoder does almost no arithmetic: for each byte it does one read, one add and one write. So its speed is set by **how fast bytes can travel between memory and the cores**, which is the **memory bandwidth**. Code like this is called **bandwidth-bound** (you met the idea in Array Timing).

One core can't use the node's whole memory bandwidth on its own. More threads do help, but only until the memory system is saturated. After that, extra threads just wait for memory. So for 100 and 1000 copies, expect speedup to rise well and then flatten below the ideal line.

The program prints a `GB_per_s` column to show this. Each byte is read once and written once, so

```
GB/s = 2 × bytes / best_time / 10^9
```

Worked example from my test machine, 1000 copies on 1 thread: 2 × 439,155,000 / 0.24195 / 10^9 = 3.63 GB/s. When this number stops growing as you add threads, you have hit the bandwidth ceiling.

### 10.5 Caches: why the three sizes behave differently

Memory has levels. Small, fast **caches** sit near each core, and the big, slow main memory (RAM) sits further away.

- **1 copy** (0.44 MB, plus 0.44 MB of output) fits in cache. After the first pass, repeats are served from cache, which is fast. On my test machine one thread managed 7.9 GB/s.
- **1000 copies** (440 MB in, 440 MB out) is far too big for any cache, so every pass streams from RAM. On my test machine one thread managed only 3.6 GB/s, half as fast.
- **100 copies** (44 MB in, 44 MB out) sits in between. A Hamilton node has a lot of cache in total: each AMD EPYC 7702 has 256 MB of level-3 cache, spread in 16 MB slices over groups of cores. One thread can only use its own slice, so the data comes from RAM. But spread over all 128 cores, each slice only holds a few MB of the data, and the whole thing may fit in the caches together. **So with many threads you may see a speedup that is better than the bandwidth argument suggests.** That's a real effect, not a bug: more threads also bring more cache. The 1000-copy run is there so you have a "definitely from RAM" case to compare against.

### 10.6 NUMA and first touch

A Hamilton node has two processors (sockets), each with its own memory. A core reads memory attached to its own socket faster than memory attached to the other socket. That's **NUMA** (Non-Uniform Memory Access), from the OpenMP lecture.

Linux places each page of memory next to the core that **first writes to it**, which is called **first touch**. So `frankenstein_timing` fills its buffers with a parallel loop that uses the *same* static split as the encoder. Each thread first touches the part it will later encode, so that part lands in its own socket's memory.

### 10.7 UTF-8, ASCII and the `iconv` trap

The book file is **UTF-8**. Plain English letters take 1 byte each, but characters like curly quotes `“ ”` and the long dash `—` take **3 bytes** each. For this encoder that isn't a correctness problem: it shifts raw bytes, and the decoder shifts them back exactly. It does mean the *encoded* file isn't valid UTF-8 any more. It also means "number of bytes" isn't "number of characters", which is the sheet's worry.

The sheet suggests converting to pure ASCII with:

```bash
iconv -f UTF-8 -t ASCII Frankenstein.txt > ASCII-Frankenstein.txt
```

**That command fails on this file.** ASCII has no curly quotes, so `iconv` stops with "illegal input sequence at position 1449" and leaves a file of only **1,449 bytes**: the first few lines of the book. If you didn't check the size, you'd be timing a tiny file and wondering why nothing scales.

The fix is `//TRANSLIT`, which tells `iconv` to **transliterate**, meaning to replace each character with the nearest ASCII one (`“` becomes `"` and `—` becomes `-`):

```bash
iconv -f UTF-8 -t ASCII//TRANSLIT Frankenstein.txt > ASCII-Frankenstein.txt
```

That gives 439,155 bytes, with no non-ASCII bytes left. `frankenstein.slurm` uses this version and prints both file sizes with `wc -c` so you can check.

### 10.8 How to time fairly

The same rules as Array Timing, applied here:

1. **Use a wall clock:** `omp_get_wtime()`. `clock()` adds up CPU time over all threads, so it would make more threads look *slower*.
2. **Do one warm-up call first.** The first parallel region creates the threads, and the first pass loads the caches. Neither should count.
3. **Repeat many times and keep the best.** One encode of one copy takes about 0.1 ms, and a single hiccup from the operating system can be bigger than that. The **best** (minimum) time is the cleanest measure of what the code can do. The **mean** is printed too, so you can see how noisy it was.
4. **Same input every time.** The encoder writes to a separate `out` buffer, so `in` never changes and every repeat does identical work.
5. **Check the answer.** After timing, the program undoes the shift and compares it with the original, byte by byte. A fast wrong answer is worth nothing.

---

## 11. Part 5: the timing program and the plot

### 11.1 The whole program

```c
// Time the +1 encoder on a whole book with 1, 2, 4, ... threads.
// Usage:  ./frankenstein_timing ASCII-Frankenstein.txt [copies] [reps] > results.csv
//   copies: glue the book to itself this many times (default 1) to make the job bigger
//   reps:   how many timed repetitions per thread count (default 200)
// The table goes to stdout as CSV; notes go to stderr.
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include "read_file.h"

// The encoder: every byte goes up by one. in and out are separate buffers,
// so every repetition does exactly the same work on the same input.
static void encode(const unsigned char *in, unsigned char *out, long n)
{
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++)
        out[i] = in[i] + 1;          // unsigned char: 255 + 1 wraps to 0, still reversible
}

// Time one thread count: one warm-up call, then reps timed calls.
static void time_threads(int T, const unsigned char *in, unsigned char *out, long n,
                         int reps, double *best, double *mean)
{
    omp_set_num_threads(T);
    encode(in, out, n);              // warm-up: wakes the threads, warms the caches

    *best = 1e30;
    double sum = 0.0;
    for (int r = 0; r < reps; r++) {
        double t0 = omp_get_wtime();
        encode(in, out, n);
        double t = omp_get_wtime() - t0;
        sum += t;
        if (t < *best) *best = t;
    }
    *mean = sum / reps;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s book.txt [copies] [reps]\n", argv[0]);
        return 1;
    }
    int copies = (argc > 2) ? atoi(argv[2]) : 1;
    int reps   = (argc > 3) ? atoi(argv[3]) : 200;

    long len;
    char *book = read_file(argv[1], &len);
    long n = len * copies;

    unsigned char *in  = malloc(n);
    unsigned char *out = malloc(n);
    if (in == NULL || out == NULL) { fprintf(stderr, "out of memory\n"); return 1; }

    // Fill the buffers in parallel with the same static split the encoder uses,
    // so each thread first touches (and so owns, on a NUMA machine) its own part.
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++) {
        in[i]  = (unsigned char) book[i % len];
        out[i] = 0;
    }

    int maxT = omp_get_max_threads();
    fprintf(stderr, "%s: %ld bytes x %d copies = %ld bytes, %d reps, up to %d threads\n",
            argv[1], len, copies, n, reps, maxT);

    printf("threads,best_s,mean_s,speedup,efficiency,GB_per_s\n");
    double t1 = 0.0;
    int T = 1;                                  // 1, 2, 4, 8, ... and finally maxT
    while (1) {
        double best, mean;
        time_threads(T, in, out, n, reps, &best, &mean);
        if (T == 1) t1 = best;
        double speedup = t1 / best;
        double gbps    = 2.0 * n / best / 1e9;  // one byte read + one byte written each
        printf("%d,%.4e,%.4e,%.2f,%.3f,%.2f\n", T, best, mean, speedup, speedup / T, gbps);
        if (T == maxT) break;
        T = 2 * T;
        if (T > maxT) T = maxT;                 // e.g. maxT = 6: 1, 2, 4, then 6
    }

    // Check the answer: undo the shift and compare with the original book.
    long wrong = 0;
    for (long i = 0; i < n; i++)
        if ((unsigned char) (out[i] - 1) != in[i]) wrong++;
    fprintf(stderr, "check: %ld wrong bytes (should be 0)\n", wrong);

    free(in); free(out); free(book);
    return wrong != 0;
}
```

### 11.2 The big picture

The program has three functions:

| function | job |
|---|---|
| `encode` | The parallel loop that adds 1 to every byte. This is the part being timed. |
| `time_threads` | For one thread count: set the count, warm up, then time `reps` calls and report the best and mean. |
| `main` | Read the book, build the input, run `time_threads` for 1, 2, 4, ... threads, print a table, check the answer. |

What happens when you run `./frankenstein_timing ASCII-Frankenstein.txt 100 50`, in order:

1. Read the arguments: file name, `copies` = 100, `reps` = 50.
2. Read the book into memory: 439,155 bytes.
3. Make an input buffer of 100 × 439,155 = 43,915,500 bytes, and an output buffer the same size.
4. Fill the input with the book repeated 100 times, in parallel, for first touch (section 10.6).
5. Find the largest thread count: `omp_get_max_threads()`, which is 128 in the Hamilton job.
6. For T = 1, 2, 4, 8, 16, 32, 64, 128:
   - call `time_threads`, which warms up once and then times 50 encodes,
   - work out the speedup against T = 1, the efficiency and the GB/s,
   - print one line of the CSV table.
7. Check every output byte, and report how many were wrong (it should be 0).

How the functions call each other:

```
main
 ├─ read_file              (once)
 ├─ fill buffers           (parallel, once)
 ├─ time_threads(T = 1)    ─┬─ encode (warm-up)
 │                          └─ encode × reps   (each one timed)
 ├─ time_threads(T = 2)    ...
 ├─ ...
 ├─ time_threads(T = 128)
 └─ check the answer
```

**Output format.** The table goes to **stdout** as CSV (comma-separated values), so you can redirect it straight into a file for the plot script. The notes ("439155 bytes x 100 copies...", "check: 0 wrong bytes") go to **stderr**, so they show in the `.err` file and don't spoil the CSV.

### 11.3 Line by line: `encode`

```c
static void encode(const unsigned char *in, unsigned char *out, long n)
```

- `void` means it returns nothing.
- `const unsigned char *in` is the address of the input bytes. `const` promises not to change them.
- `unsigned char` is a byte from 0 to 255 (section 9.4 explains why unsigned).
- `static` means "only used in this file".

```c
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++)
        out[i] = in[i] + 1;
```

The whole encoder, in parallel. With `unsigned char`, 255 + 1 wraps to 0. Plain ASCII never has byte 255, but even if it did, the decoder's −1 would turn 0 back into 255, so nothing is lost.

A loop body that is a single statement doesn't need curly braces.

### 11.4 Line by line: `time_threads`

```c
static void time_threads(int T, const unsigned char *in, unsigned char *out, long n,
                         int reps, double *best, double *mean)
```

It needs to hand back two results, best and mean, so it takes two pointers to write them into (the same trick as `n_out` in section 8.4).

```c
    omp_set_num_threads(T);
```

Sets the team size for the *next* parallel regions. This overrides `OMP_NUM_THREADS` from inside the program, so one run can test every thread count.

```c
    encode(in, out, n);
```

The warm-up (section 10.8). Its time is thrown away.

```c
    *best = 1e30;
    double sum = 0.0;
```

`1e30` means 1 × 10^30, an absurdly large time. Any real measurement will be smaller, so the first one becomes the best. `*best` writes to the caller's variable.

```c
    for (int r = 0; r < reps; r++) {
        double t0 = omp_get_wtime();
        encode(in, out, n);
        double t = omp_get_wtime() - t0;
        sum += t;
        if (t < *best) *best = t;
    }
    *mean = sum / reps;
```

The standard timing pattern: read the clock, do the work, read the clock again, subtract. `omp_get_wtime()` returns wall-clock seconds as a `double`. `sum += t` is short for `sum = sum + t`. After the loop, the mean is the total divided by the count.

### 11.5 Line by line: `main`

```c
    int copies = (argc > 2) ? atoi(argv[2]) : 1;
    int reps   = (argc > 3) ? atoi(argv[3]) : 200;
```

`condition ? A : B` is C's "if-else in one expression": if the condition is true, the value is A, otherwise B. So if a second argument was given, `atoi` ("ASCII to integer") turns the text `"100"` into the number 100. If not, the default is used.

```c
    long len;
    char *book = read_file(argv[1], &len);
    long n = len * copies;
```

Reads the book (`len` = 439,155) and works out the total size.

```c
    unsigned char *in  = malloc(n);
    unsigned char *out = malloc(n);
    if (in == NULL || out == NULL) { fprintf(stderr, "out of memory\n"); return 1; }
```

Two buffers from the heap. For 1000 copies that's 439 MB each, 878 MB in total, which is why the job asks for 4 GB. `||` means "or".

```c
    #pragma omp parallel for schedule(static)
    for (long i = 0; i < n; i++) {
        in[i]  = (unsigned char) book[i % len];
        out[i] = 0;
    }
```

Fills the input with the book over and over. `i % len` is the remainder, so it counts 0, 1, ..., 439,154 and then starts again at 0. With `len` = 439,155:

- `i` = 5 gives `i % len` = 5,
- `i` = 439,155 gives 0, the start of the second copy,
- `i` = 439,160 gives 5 again.

Writing `out[i] = 0` here matters too. It's the first touch of the output buffer, so its pages are placed in the right socket's memory (section 10.6).

```c
    int maxT = omp_get_max_threads();
```

The largest team allowed. In the Hamilton job that's 128. SLURM doesn't set `OMP_NUM_THREADS` for you, but when it isn't set, OpenMP's default is "one thread per CPU the program can see", and `--cpus-per-task=128` lets the job see 128 CPUs. To test fewer, run it with `OMP_NUM_THREADS=16` in front, for example.

```c
    printf("threads,best_s,mean_s,speedup,efficiency,GB_per_s\n");
```

The CSV header line. The plot script uses these names to find the columns.

```c
    double t1 = 0.0;
    int T = 1;                                  // 1, 2, 4, 8, ... and finally maxT
    while (1) {
        double best, mean;
        time_threads(T, in, out, n, reps, &best, &mean);
```

`while (1)` loops forever, until a `break` inside it stops it. `&best` and `&mean` pass the addresses of these variables, so `time_threads` can fill them in.

```c
        if (T == 1) t1 = best;
        double speedup = t1 / best;
        double gbps    = 2.0 * n / best / 1e9;
        printf("%d,%.4e,%.4e,%.2f,%.3f,%.2f\n", T, best, mean, speedup, speedup / T, gbps);
```

The first pass (T = 1) saves the one-thread time, and every speedup is measured against it. Then one CSV row is printed:

- `%.4e` is scientific notation with 4 decimals. For example, `1.1169e-04` means 1.1169 × 10⁻⁴ seconds, which is 0.11 ms.
- `%.2f` is a normal decimal with 2 places.

```c
        if (T == maxT) break;
        T = 2 * T;
        if (T > maxT) T = maxT;
    }
```

Stop after the largest count. Otherwise double `T`, but don't overshoot. With `maxT` = 128 that gives 1, 2, 4, ..., 128. With `maxT` = 6 it gives 1, 2, 4, then 6.

```c
    long wrong = 0;
    for (long i = 0; i < n; i++)
        if ((unsigned char) (out[i] - 1) != in[i]) wrong++;
    fprintf(stderr, "check: %ld wrong bytes (should be 0)\n", wrong);
```

The check from section 10.8: decode each output byte and compare it with the input. `!=` means "not equal". The cast back to `unsigned char` makes 0 − 1 wrap to 255, matching the encoder's wrap.

```c
    free(in); free(out); free(book);
    return wrong != 0;
```

Gives the memory back. `wrong != 0` is 1 (true) if anything was wrong and 0 (false) if not. So the program exits with "failure" if the check failed, and with "success" if everything matched.

### 11.6 Results from my test machine (not Hamilton)

These come from a 4-core cloud machine, just so you can see what the output looks like. The Hamilton numbers will be different, and they are the ones for your report.

```
1 copy:
threads,best_s,mean_s,speedup,efficiency,GB_per_s
1,1.1169e-04,1.3636e-04,1.00,1.000,7.86
2,5.6757e-05,7.2289e-05,1.97,0.984,15.47
4,2.9542e-05,5.0693e-05,3.78,0.945,29.73

100 copies:
1,2.3130e-02,2.6060e-02,1.00,1.000,3.80
2,1.2706e-02,1.4856e-02,1.82,0.910,6.91
4,6.7231e-03,7.7707e-03,3.44,0.860,13.06

1000 copies:
1,2.4195e-01,2.6809e-01,1.00,1.000,3.63
2,1.2687e-01,1.3935e-01,1.91,0.954,6.92
4,6.8838e-02,7.7665e-02,3.51,0.879,12.76
```

How to read them:

- **Every size speeds up well to 4 threads.** Four threads is nowhere near the point where overhead or bandwidth bites.
- **One copy runs at about twice the GB/s of the big sizes** (7.9 against 3.6 to 3.8 on one thread). That's the cache effect from section 10.5.
- **The mean is always above the best**, sometimes by 50% for the tiny case. That's the noise that taking the best removes.

When I asked for **6 threads on the 4-core machine**, the 1-copy speedup *dropped* to 1.2 to 1.8x. Two threads had no core of their own, so they kept being paused, and everyone waited for them at the barrier. That's why the Hamilton job asks for 128 CPUs and never runs more threads than cores.

### 11.7 What to look for in your Hamilton plot

- **1 copy:** the line leaves the ideal early and probably bends down at 64 to 128 threads (section 10.3).
- **100 and 1000 copies:** the line follows the ideal for longer, then flattens as memory bandwidth runs out (section 10.4). The `GB_per_s` column stops growing at the same point.
- **100 copies might beat 1000 copies at high thread counts**, because 44 MB can fit in the whole node's caches but 440 MB can't (section 10.5).

A good report sentence is something like: *"For the original text, parallel overhead dominates beyond about N threads because each thread receives only a few KB; scaling the input by 100x recovers near-linear speedup up to M threads, after which the kernel becomes memory-bandwidth bound at about X GB/s."* Fill in N, M and X from your own run.

### 11.8 The plot script, `plot_speedup.py`

```python
"""Plot speedup against threads from one or more CSV files made by frankenstein_timing.

Usage:  python3 plot_speedup.py speedup.png results_1copy.csv "1 copy (0.44 MB)" \
                                            results_100copies.csv "100 copies (44 MB)"
Arguments after the PNG name come in pairs: a CSV file, then the label for it.
"""
import csv
import sys

import matplotlib
matplotlib.use("Agg")                     # draw to a file, no window needed
import matplotlib.pyplot as plt

COLOURS = ["#2a78d6", "#eb6834", "#1baf7a", "#6250d6"]   # one colour per data set


def read_csv(path):
    threads, speedup = [], []
    with open(path) as f:
        for row in csv.DictReader(f):
            threads.append(int(row["threads"]))
            speedup.append(float(row["speedup"]))
    return threads, speedup


def main():
    if len(sys.argv) < 4 or len(sys.argv) % 2 != 0:
        sys.exit(__doc__)
    out_png = sys.argv[1]
    pairs = list(zip(sys.argv[2::2], sys.argv[3::2]))

    fig, ax = plt.subplots(figsize=(7, 4.5), dpi=150)
    max_t = 1
    ends = []                             # last point of each line, for its label
    for (path, label), colour in zip(pairs, COLOURS):
        t, s = read_csv(path)
        max_t = max(max_t, max(t))
        ax.plot(t, s, marker="o", markersize=6, linewidth=2, color=colour, label=label)
        ends.append((t[-1], s[-1]))

    ideal = sorted({2 ** k for k in range(max_t.bit_length())} | {max_t})   # 1, 2, 4, ..., max_t
    ax.plot(ideal, ideal, linestyle="--", linewidth=1.5, color="#8a8984",
            label="ideal (speedup = threads)")

    ax.set_xscale("log", base=2)
    ax.set_yscale("log", base=2)
    ax.set_xticks(ideal)
    ax.set_xticklabels([str(x) for x in ideal])
    ax.set_yticks(ideal)
    ax.set_yticklabels([str(x) for x in ideal])
    ax.set_xlabel("OpenMP threads")
    ax.set_ylabel("speedup  t(1) / t(threads)")
    ax.set_title("Encoding Frankenstein: speedup against threads")
    ax.grid(True, which="major", color="#e6e5e0", linewidth=0.8)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    ax.legend(frameon=False, loc="upper left")

    # Label the final speedup of each line, nudging labels apart so they never overlap.
    fig.canvas.draw()
    ys = sorted(ends, key=lambda p: p[1])
    gap = 9 * 1.4 * fig.dpi / 72          # one 9-point line of text, in pixels
    last_pix = None
    for x, y in ys:
        pix = ax.transData.transform((x, y))[1]
        shift = 0.0
        if last_pix is not None and pix - last_pix < gap:
            shift = gap - (pix - last_pix)
        last_pix = pix + shift
        ax.annotate(f"{y:.1f}x", (x, y), textcoords="offset pixels", xytext=(6 * fig.dpi / 72, shift),
                    va="center", fontsize=9, color="#52514e")
    fig.tight_layout()
    fig.savefig(out_png)
    print("saved", out_png)


if __name__ == "__main__":
    main()
```

The big picture: read each CSV file, draw speedup against threads as one coloured line per file, add the dashed ideal line, and save a PNG.

The parts worth knowing:

- **`sys.argv`** is Python's version of C's `argv`. After the PNG name, arguments come in pairs: a CSV file, then its label. `sys.argv[2::2]` takes every second item starting at index 2 (the files), and `sys.argv[3::2]` takes every second item starting at index 3 (the labels). `zip` pairs them up.
- **`csv.DictReader`** reads each row as a dictionary keyed by the header names, so `row["speedup"]` is that row's speedup, as text. `float(...)` turns it into a number.
- **`matplotlib.use("Agg")`** draws straight to a file. You need it on Hamilton, where there's no screen.
- **Both axes are log base 2.** Thread counts double each step, so a log axis spaces them evenly. On a log-log plot, perfect speedup S = T is a straight diagonal line, and anything that bends away from it is lost efficiency.
- **The label at the end of each line** (for example, `3.8x`) shows the final speedup, so you don't have to read it off the axis. `fig.canvas.draw()` lays the plot out first, so the script can find where each label lands in pixels. Then any label closer than one line of text to the one below it is nudged up, so labels never overlap.

---

## 12. The SLURM scripts, line by line

### 12.1 `descramble.slurm` (Parts 1 to 3)

```bash
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
```

The header lines, which are the same pattern as before:

| line | meaning |
|---|---|
| `#!/bin/bash` | Run this file with the bash shell. |
| `#SBATCH --nodes=1` | One compute node. |
| `#SBATCH --ntasks=1` | One process. OpenMP threads all live inside one process. |
| `#SBATCH --cpus-per-task=8` | 8 cores for that process, enough for the 8-thread runs. |
| `#SBATCH --mem=1G` | 1 GB of memory, which is plenty. |
| `#SBATCH --job-name=...` | The name shown in `squeue`. |
| `#SBATCH -o descramble.%J.out` | Normal output goes here. `%J` is replaced by the job number. |
| `#SBATCH -e descramble.%J.err` | Error output (stderr) goes here. |
| `#SBATCH -t 00:05:00` | Kill the job after 5 minutes. It needs a few seconds. |
| `#SBATCH -p test` | The `test` partition: quick to start, 15 minutes max, one node. |

The body:

- `module load gcc` makes the GCC compiler available.
- Three `gcc` lines compile the three programs. `-O2` turns on optimisation, `-Wall` turns on warnings, and `-fopenmp` turns on OpenMP. `-o name` sets the program's file name.
- `SHEET='...'` stores the sheet's scrambled text in a shell variable. Single quotes stop the shell from treating `!` specially.
- `"$(./encoder | tail -n 1)"` runs the encoder and keeps only its last line. `$( ... )` means "replace this with the command's output". The `if [ A = B ]` test compares the two strings and prints whether they match.
- `for T in 1 2 4 8; do ... done` repeats the inner part with T = 1, 2, 4 and 8, and `for run in 1 2 3` repeats each of those 3 times.
- `OMP_NUM_THREADS=$T ./decoder_omp`: putting `NAME=value` in front of a command sets that environment variable for that one command only.
- The summary loop pipes (`|`) the output through three tools. `awk '/^thread/{print $2}'` keeps the thread number from each line that starts with "thread". `uniq -c` collapses repeated lines into a count, so twenty-six "2"s in a row become `26 2`. The last `awk` prints that as `t2 x26`.

### 12.2 `threadkey.slurm` (Part 4)

```bash
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
```

- `export OMP_DYNAMIC=false` makes OpenMP give exactly the number of threads asked for (section 7.8). `export` keeps it set for every command after it.
- The outer loop encodes with 1, 4 and 8 threads into `secret_1.txt`, `secret_4.txt` and `secret_8.txt`. `2>/dev/null` throws away the stderr note, so the output stays tidy. `/dev/null` is the system's bin.
- The inner loop decodes each secret with 1, 2, 3, 4, 5 and 8 threads. Only the matching row should read as English.
- `printf "decoded with %d threads: " $TD` is the shell's own printf, with the same `%d` idea as C.

### 12.3 `frankenstein.slurm` (Part 5)

```bash
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
```

What's new compared with the others:

- `--cpus-per-task=128` asks for a whole node, so each of up to 128 threads gets its own core.
- `--mem=4G`: the 1000-copy run needs about 0.9 GB.
- `iconv ... //TRANSLIT` makes the ASCII copy correctly (section 10.7). `wc -c` prints both file sizes, so you can see that the ASCII file is about 439 KB, not 1.4 KB.
- `OMP_PLACES=cores` and `OMP_PROC_BIND=close` pin each thread to its own core, packed next to each other. Without pinning, the operating system may move threads between cores mid-run, which adds noise. With `close`, 8 threads share one region of the chip, and 128 fill both sockets.
- `> results_1copy.csv` and the two lines after it save each table to its own file. The stderr notes still go to the `.err` file.
- The `\` at the end of a line means "this command carries on onto the next line".
- `python3 plot_speedup.py ... || echo "..."`: `||` means "if the first command fails, run the second". So if Hamilton's default Python lacks matplotlib, you get a friendly note instead of a crash, and the CSVs are still there for plotting on your Mac.

---

## 13. The commands to run

### 13.1 On your Mac: push the code

```bash
cd ~/Documents/Durham/HPC/programs
git status
git add openmp-descrambling
git commit -m "Descrambling exercise: encoder, decoders, thread-keyed pair, Frankenstein timing"
git push
```

| command | what it does |
|---|---|
| `cd ...` | Go to your local copy of the repo. |
| `git status` | Shows what changed. You should see the new files in `openmp-descrambling/`. |
| `git add openmp-descrambling` | Marks the whole folder to be saved. The `.gitignore` in it keeps compiled programs and job outputs out. |
| `git commit -m "..."` | Saves a snapshot with a message. |
| `git push` | Sends the snapshot to GitHub. |

### 13.2 On Hamilton: pull, submit, read

```bash
ssh hamilton
cd HPC-programs            # or wherever you cloned the repo
git pull
cd openmp-descrambling
sbatch descramble.slurm
sbatch threadkey.slurm
sbatch frankenstein.slurm
squeue -u $USER
```

| command | what it does |
|---|---|
| `ssh hamilton` | Log in, using your `~/.ssh/config` alias. |
| `git pull` | Fetches what you just pushed. |
| `sbatch file.slurm` | Puts the job in the queue and prints its number. |
| `squeue -u $USER` | Lists your jobs that are waiting (`PD`) or running (`R`). When they're gone from the list, they've finished. |

Then read the results:

```bash
cat descramble.*.out
cat threadkey.*.out
cat frankenstein.*.out
cat frankenstein.*.err      # the notes, including "check: 0 wrong bytes"
ls results_*.csv speedup.png
```

If a `.err` file says something like "No module named matplotlib", plot on your Mac instead (next section).

### 13.3 Plotting on your Mac, if needed

```bash
cd ~/Documents/Durham/HPC/programs/openmp-descrambling
scp 'hamilton:HPC-programs/openmp-descrambling/results_*.csv' .
python3 -m pip install matplotlib        # only the first time
python3 plot_speedup.py speedup.png \
    results_1copy.csv "1 copy (0.44 MB)" \
    results_100copies.csv "100 copies (44 MB)" \
    results_1000copies.csv "1000 copies (440 MB)"
open speedup.png
```

`scp` copies files over ssh. The quotes stop your Mac's shell from expanding the `*` itself, so Hamilton expands it instead. The final `.` means "into the current folder". Change `HPC-programs` if you cloned the repo somewhere else.

(Side note: Hamilton may also have a Python module with matplotlib. `module avail python` lists them, and `module load` one before running the script there.)

---

## 14. Glossary

| term | meaning |
|---|---|
| **ASCII** | The basic table that gives each of 128 characters a number from 0 to 127. Space is 32, `'A'` is 65 and `'a'` is 97. |
| **bandwidth-bound** | Code whose speed is set by how fast data moves to and from memory, not by arithmetic. |
| **barrier** | A point where every thread waits until all threads have arrived. There is a hidden one at the end of every `parallel for`. |
| **byte** | One memory box, holding a whole number from 0 to 255. |
| **cache** | Small, fast memory near the cores that keeps recently used data. |
| **Caesar cipher** | Shift every letter by a fixed amount, the key. |
| **cast** | `(type) value`: convert a value to another type. |
| **CSV** | "Comma-separated values": a plain-text table with one row per line and commas between columns. |
| **embarrassingly parallel** | Work made of fully independent pieces, which need no coordination. |
| **end marker / `'\0'`** | The byte with value 0 that ends every C string. |
| **first touch** | Linux puts a memory page next to the core that first writes to it. |
| **fork / join** | Start a team of threads, and later wait for them all and carry on with one. |
| **heap** | The large memory pool that `malloc` hands out from, and `free` returns to. |
| **include guard** | `#ifndef X / #define X / ... / #endif`, which stops a header being pasted in twice. |
| **key** | The secret extra information needed to decode. Here it's a shift amount. |
| **modulo (`%`)** | The remainder after division: 98 % 95 = 3. In C, the result keeps the sign of the left-hand number. |
| **NUMA** | Non-Uniform Memory Access: memory attached to your own socket is faster than the other socket's. |
| **overhead** | Time spent organising parallel work (starting threads, sharing out the work, barriers) rather than doing it. |
| **pointer** | A variable that holds a memory address. `*p` is the thing it points at, and `&x` is the address of `x`. |
| **pragma** | A note to the compiler. `#pragma omp ...` lines are OpenMP's instructions. |
| **private / shared** | Whether each thread gets its own copy of a variable, or they all use the same one. |
| **race condition** | A result that depends on the timing of threads touching the same data. |
| **redirect (`>`)** | A shell feature that sends a program's output into a file. |
| **schedule(static)** | Splits loop iterations into one fixed, equal-as-possible block per thread before the loop starts. |
| **speedup / efficiency** | `t(1) / t(T)`, and speedup divided by `T`. |
| **stdout / stderr** | A program's normal output stream and its error and notes stream. `>` only redirects stdout. |
| **strong scaling** | Same problem size, more threads: how much faster does it get? |
| **transliterate** | Replace a character with the closest one in a smaller character set (`“` becomes `"`). |
| **UTF-8** | The usual text encoding today. ASCII characters take 1 byte, and others take 2 to 4. |
