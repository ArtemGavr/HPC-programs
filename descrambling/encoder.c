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
