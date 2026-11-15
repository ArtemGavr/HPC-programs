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
