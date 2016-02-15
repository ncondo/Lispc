#include <stdio.h>
#include <stdlib.h>

/* If we are compiling on Windows use these functions */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* Faux readline function for Windows compatibility */
char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer)+1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy)-1] = '\0';
    return cpy;
}

/* Faux add_history function for Windows compatibility */
void add_history(char* unused) {}


/* Otherwise, include editline headers for Linux/Mac */
#else
#include <editline/readline.h>
#endif


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