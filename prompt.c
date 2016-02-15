#include <stdio.h>

/* Declare a buffer for user input of size 2048 */
static char input[2048];

int main(int argc, char** argv) {

    /* Print version and exit information */
    puts("Lispc Version 0.0.1");
    puts("Press Ctrl+c to Exit\n");

    /* In a never ending loop */
    while(1) {

        /* Output our prompt */
        fputs("lispc> ", stdout);

        /* Read a line of user input of max size 2048 */
        fgets(input, 2048, stdin);

        /* Echo input back to user */
        printf("=> %s", input);
    }

    return 0;
}