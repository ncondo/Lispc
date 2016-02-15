#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>


int main(int argc, char** argv) {

    /* Print version and exit information */
    puts("Lispc Version 0.0.1");
    puts("Press Ctrl+c to Exit\n");

    /* In a never ending loop */
    while(1) {

        /* Output our prompt and get input */
        char* input = readline("lispc> ");

        /* Add input to history */
        add_history(input);

        /* Echo input back to user */
        printf("=> %s\n", input);

        /* Free retrieved input */
        free(input);
    }

    return 0;
}