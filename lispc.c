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

/* Create enumeration of possible lval types */
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

/* Declare new lval struct */
typedef struct lval {
    int type;
    long num;
    char* err;
    char* sym;
    int count;
    struct lval** cell;
} lval;

/* Construct a pointer to a new Number lval */
lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

/* Construct a pointer to a new Error lval */
lval* lval_err(char* m) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

/* Construct a pointer to a new Symbol lval */
lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

/* A pointer to a new empty Sexpr lval */
lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* A pointer to a new empty Qexpr lval */
lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* Delete lval and free all allocated memory */
void lval_del(lval* v) {
    switch (v->type) {
        case LVAL_NUM: break;
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
        break;
    }
    free(v);
}

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

lval* lval_pop(lval* v, int i) {
    /* Find the item at 'i' */
    lval* x = v->cell[i];

    /* Shift memory after the item at 'i' over the top */
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));

    /* Decrease the count of items in the list */
    v->count--;

    /* Reallocate the memory used */
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval* lval_join(lval* x, lval* y) {
    /* For each cell in 'y' add it to 'x' */
    while (y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }

    /* Delete the empty 'y' and return 'x' */
    lval_del(y);
    return x;
}

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
    /* If Symbol or Number return conversion to that type */
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    /* If root (>) or sexpr then create empty list */
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }

    /* Fill this list with any valid expression contained within */
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }
    return x;
}

lval* lval_eval(lval* v);
lval* builtin(lval* a, char* func);

lval* lval_eval_sexpr(lval* v) {
    /* Evaluate children */
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    /* Error checking */
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { 
            return lval_take(v, i); 
        }
    }

    /* Empty expression */
    if (v->count == 0) {
        return v;
    }

    /* Single expression */
    if (v->count == 1) {
        return lval_take(v, 0);
    }

    /* Ensure first element is Symbol */
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f);
        lval_del(v);
        return lval_err("S-expression should start with a Symbol!");
    }

    /* Call builtin with operator */
    lval* result = builtin(v, f->sym);
    lval_del(f);
    return result;
}

lval* lval_eval(lval* v) {
    /* Evaluate Sexpressions */
    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(v);
    }
    /* All other lval types remain the same */
    return v;
}

lval* builtin_op(lval* a, char* op) {
    /* Ensure all arguments are numbers */
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot operate on non-number!");
        }
    }

    /* Pop the first element */
    lval* x = lval_pop(a, 0);

    /* If no arguments and sub the perform unary negation */
    if ((strcmp(op, "-") == 0) && a->count ==0) {
        x->num = -x->num;
    }

    while (a->count > 0) {
        /* Pop the next element */
        lval* y = lval_pop(a, 0);

        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x);
                lval_del(y);
                x = lval_err("Error: Division by zero!");
                break;
            }
            x->num /= y->num;
        }
        if (strcmp(op, "%") == 0) { x->num = (x->num % y->num); }

        lval_del(y);
    }
    lval_del(a);
    return x;
}

lval* builtin_head(lval* a) {
    /* Check error conditions */
    if (a->count != 1) {
        lval_del(a);
        return lval_err("Error: Function 'head' passed too many arguments!");
    }

    if (a->cell[0]->type != LVAL_QEXPR) {
        lval_del(a);
        return lval_err("Error: Function 'head' passed incorrect type!");
    }

    if (a->cell[0]->count == 0) {
        lval_del(a);
        return lval_err("Error: Function 'head' passed empty list {}!");
    }

    lval* v = lval_take(a, 0);

    while(v->count > 1) {
        lval_del(lval_pop(v, 1));
    }
    return v;
}

lval* builtin_tail(lval* a) {
    /* Check error conditions */
    if (a->count != 1) {
        lval_del(a);
        return lval_err("Error: Function 'tail' passed too many arguments!");
    }

    if (a->cell[0]->type != LVAL_QEXPR) {
        lval_del(a);
        return lval_err("Error: Function 'tail' passed incorrect type!");
    }

    if (a->cell[0]->count == 0) {
        lval_del(a);
        return lval_err("Error: Function 'tail' passed empty list {}!");
    }

    lval* v = lval_take(a, 0);

    lval_del(lval_pop(v, 0));
    return v;
}

lval* builtin_list(lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lval* a) {
    if (a->count != 1) {
        lval_del(a);
        return lval_err("Error: Function 'eval' passed too many arguments!");
    }

    if (a->cell[0]->type != LVAL_QEXPR) {
        lval_del(a);
        return lval_err("Error: Function 'eval' passed incorrect type!");
    }

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
}

lval* builtin_join(lval* a) {
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_QEXPR) {
            lval_del(a);
            return lval_err("Error: Function 'join' passed incorrect type!");
        }
    }

    lval* x = lval_pop(a, 0);

    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

lval* builtin(lval* a, char* func) {
    if (strcmp("list", func) == 0) { return builtin_list(a); }
    if (strcmp("head", func) == 0) { return builtin_head(a); }
    if (strcmp("tail", func) == 0) { return builtin_tail(a); }
    if (strcmp("join", func) == 0) { return builtin_join(a); }
    if (strcmp("eval", func) == 0) { return builtin_eval(a); }
    if (strstr("+-/*", func))  { return builtin_op(a, func); }
    lval_del(a);
    return lval_err("Unknown Function!");
}

void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);
        /* Don't print trailing space if last element */
        if (i != (v->count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

/* Print an lval */
void lval_print(lval* v) {
    switch (v->type) {
        case LVAL_NUM:      printf("%li", v->num); break;
        case LVAL_ERR:      printf("Error: %s", v->err); break;
        case LVAL_SYM:      printf("%s", v->sym); break;
        case LVAL_SEXPR:    lval_expr_print(v, '(', ')'); break;
        case LVAL_QEXPR:    lval_expr_print(v, '{', '}'); break;
    }
}

/* Print an lval followed by a newline */
void lval_println(lval* v) { lval_print(v); putchar('\n'); }



int main(int argc, char** argv) {

    /* Create Parsers */
    mpc_parser_t* Number    = mpc_new("number");
    mpc_parser_t* Symbol    = mpc_new("symbol");
    mpc_parser_t* Sexpr     = mpc_new("sexpr");
    mpc_parser_t* Qexpr     = mpc_new("qexpr");
    mpc_parser_t* Expr      = mpc_new("expr");
    mpc_parser_t* Lispc     = mpc_new("lispc");

    /* Define parsers with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                       \
            number : /-?[0-9]+/ ;                               \
            symbol : '+' | '-' | '*' | '/' | '%' | \"list\"     \
                | \"head\" | \"tail\" | \"join\" | \"eval\" ;   \
            sexpr  : '(' <expr>* ')' ;                          \
            qexpr  : '{' <expr>* '}' ;                          \
            expr   : <number> | <symbol> | <sexpr> | <qexpr> ;  \
            lispc  : /^/ <expr>* /$/ ;                          \
        ",
        Number, Symbol, Sexpr, Qexpr, Expr, Lispc);

    /* Print version and exit information */
    puts("Lispc Version 0.0.5");
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
            lval* result = lval_eval(lval_read(r.output));
            lval_println(result);
            lval_del(result);
            mpc_ast_delete(r.output);
        } else {
            /* Otherwise print the error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        /* Free retrieved input */
        free(input);
    }

    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispc);

    return 0;
}