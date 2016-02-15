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

/* Create enumeration of possible error types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/* Create enumeration of possible lval types */
enum { LVAL_NUM, LVAL_ERR };

/* Declare new lval struct */
typedef struct {
    int type;
    long num;
    int err;
} lval;

/* Create a new number type lval */
lval lval_num(long x) {
    lval v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}

/* Create a new error type lval */
lval lval_err(int x) {
    lval v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}

/* Print an lval */
void lval_print(lval v) {
    switch (v.type) {
        /* If type is a number print it */
        case LVAL_NUM: printf("%li", v.num); break;

        /* If type is an error print specific error */
        case LVAL_ERR:
            if (v.err == LERR_DIV_ZERO) {
                printf("Error: Division by zero!");
            }
            if (v.err == LERR_BAD_OP) {
                printf("Error: Invalid operation!");
            }
            if (v.err == LERR_BAD_NUM) {
                printf("Error: Invalid number!");
            }
        break;
    }
}

/* Print an lval followed by a newline */
void lval_println(lval v) { lval_print(v); putchar('\n'); }

/* Use operator string to see which operation to perform */
lval eval_op(lval x, char* op, lval y) {
    /* If either value is an error return it */
    if (x.type == LVAL_ERR) { return x; }
    if (y.type == LVAL_ERR) { return y; }
    /* Otherwise perform operations */
    if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
    if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
    if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
    if (strcmp(op, "/") == 0) { 
        /* Check for division by 0 */
        return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
    }
    return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {
    /* If tagged as number return it directly */
    if (strstr(t->tag, "number")) {
        /* Check for error conversion */
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }

    /* The operator is always second child */
    char* op = t->children[1]->contents;

    /* Store third child in 'x' */
    lval x = eval(t->children[2]);

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
    puts("Lispc Version 0.0.4");
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
            lval result = eval(r.output);
            lval_println(result);
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