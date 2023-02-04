#include <stdio.h>
#include <stdlib.h>
#include "mpc/mpc.h"

/* If we are compiling on Windows compile these functions */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

char *readline(char *prompt)
{
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char *cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy) - 1] = '\0';
  return cpy;
}

void add_history(char *unused) {}

#else
#include <readline/history.h>
#include <readline/readline.h>
#endif

int main(int argc, char **argv)
{

  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Operator = mpc_new("operator");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *PLisp = mpc_new("lisp");

  mpca_lang(MPCA_LANG_DEFAULT,
            "                                                     \
    number   : /-?[0-9]+(\\.[0-9]+)?/ ;                    \
    operator : '+' | '-' | '*' | '/' | '%' ;                    \
    expr     : <number> | '(' <operator> <expr>+ ')' ;    \
    lisp     : /^/ <operator> <expr>+ /$/ ;               \
  ",
            Number, Operator, Expr, PLisp);

  puts("Plisp Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  while (1)
  {
    mpc_result_t r;
    char *input = readline("plisp> ");

    add_history(input);
    if (mpc_parse("<stdin>", input, PLisp, &r))
    {
      /* On Success Print the AST */
      mpc_ast_print(r.output);
      mpc_ast_delete(r.output);
    }
    else
    {
      /* Otherwise Print the Error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    free(input);
  }

  mpc_cleanup(4, Number, Operator, Expr, PLisp);
  return 0;
}