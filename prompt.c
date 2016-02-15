#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"

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

/* Use operator string to see which operation to perform */
long eval_op(long x, char* op, long y) {
    if (strcmp(op, "+") == 0) { return x + y; }
    if (strcmp(op, "-") == 0) { return x - y; }
    if (strcmp(op, "*") == 0) { return x * y; }
    if (strcmp(op, "/") == 0) { return x / y; }
    return 0;
}

long eval(mpc_ast_t* t) {
    /* If tagged as number return it directly */
    if (strstr(t->tag, "number")) {
        return atoi(t->contents);
    }

    /* The operator is always second child */
    char* op = t->children[1]->contents;

    /* Store third child in 'x' */
    long x = eval(t->children[2]);

    /* Iterate through remaining children and combine */
    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }
    return x;
}


int main(int argc, char** argv) {

    /* Create Parsers */
    mpc_parser_t* Number    = mpc_new("number");
    mpc_parser_t* Operator  = mpc_new("operator");
    mpc_parser_t* Expr      = mpc_new("expr");
    mpc_parser_t* Lispc     = mpc_new("lispc");

    /* Define parsers with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                       \
            number   : /-?[0-9]+/ ;                             \
            operator : '+' | '-' | '*' | '/' ;                  \
            expr     : <number> | '(' <operator> <expr>+ ')' ;  \
            lispc    : /^/ <operator> <expr>+ /$/ ;             \
        ",
        Number, Operator, Expr, Lispc);

    /* Print version and exit information */
    puts("Lispc Version 0.0.3");
    puts("Press Ctrl+c to Exit\n");

    /* In a never ending loop */
    while(1) {

        /* Output our prompt and get input */
        char* input = readline("lispc> ");

        /* Add input to history */
        add_history(input);

        /* Parse the user input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispc, &r)) {
            long result = eval(r.output);
            printf("%li\n", result);
            mpc_ast_delete(r.output);
        } else {
            /* Otherwise print the error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        /* Free retrieved input */
        free(input);
    }

    mpc_cleanup(4, Number, Operator, Expr, Lispc);

    return 0;
}