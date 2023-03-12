#include "mpc/mpc.h"

#ifdef _WIN32

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
#include <readline/readline.h>
#include <readline/history.h>
#endif

enum
{
  PVAL_ERR,
  PVAL_NUM,
  PVAL_SYM,
  PVAL_SEXPR,
  PVAL_QEXPR
};

typedef struct pval
{
  int type;
  long num;
  char *err;
  char *sym;
  int count;
  struct pval **cell;
} pval;

pval *pval_num(long x)
{
  pval *v = malloc(sizeof(pval));
  v->type = PVAL_NUM;
  v->num = x;
  return v;
}

pval *pval_err(char *m)
{
  pval *v = malloc(sizeof(pval));
  v->type = PVAL_ERR;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

pval *pval_sym(char *s)
{
  pval *v = malloc(sizeof(pval));
  v->type = PVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

pval *pval_sexpr(void)
{
  pval *v = malloc(sizeof(pval));
  v->type = PVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

pval *pval_qexpr(void)
{
  pval *v = malloc(sizeof(pval));
  v->type = PVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void pval_del(pval *v)
{

  switch (v->type)
  {
  case PVAL_NUM:
    break;

  case PVAL_ERR:
    free(v->err);
    break;
  case PVAL_SYM:
    free(v->sym);
    break;

  case PVAL_QEXPR:
  case PVAL_SEXPR:
    for (int i = 0; i < v->count; i++)
    {
      pval_del(v->cell[i]);
    }
    free(v->cell);
    break;
  }

  free(v);
}

pval *pval_add(pval *v, pval *x)
{
  v->count++;
  v->cell = realloc(v->cell, sizeof(pval *) * v->count);
  v->cell[v->count - 1] = x;
  return v;
}

pval *pval_pop(pval *v, int i)
{
  pval *x = v->cell[i];

  memmove(&v->cell[i], &v->cell[i + 1],
          sizeof(pval *) * (v->count - i - 1));

  v->count--;

  v->cell = realloc(v->cell, sizeof(pval *) * v->count);
  return x;
}

pval *pval_take(pval *v, int i)
{
  pval *x = pval_pop(v, i);
  pval_del(v);
  return x;
}

void pval_print(pval *v);

void pval_expr_print(pval *v, char open, char close)
{
  putchar(open);
  for (int i = 0; i < v->count; i++)
  {

    pval_print(v->cell[i]);

    if (i != (v->count - 1))
    {
      putchar(' ');
    }
  }
  putchar(close);
}

void pval_print(pval *v)
{
  switch (v->type)
  {
  case PVAL_NUM:
    printf("%li", v->num);
    break;
  case PVAL_ERR:
    printf("Error: %s", v->err);
    break;
  case PVAL_SYM:
    printf("%s", v->sym);
    break;
  case PVAL_SEXPR:
    pval_expr_print(v, '(', ')');
    break;
  case PVAL_QEXPR:
    pval_expr_print(v, '{', '}');
    break;
  }
}

void pval_println(pval *v)
{
  pval_print(v);
  putchar('\n');
}

pval *pval_eval(pval *v);

#define PASSERT(args, cond, err) \
  if (!(cond))                   \
  {                              \
    pval_del(args);              \
    return pval_err(err);        \
  }

pval *builtin_head(pval *a)
{
  PASSERT(a, a->count == 1,
          "Function 'head' passed too many arguments!");
  PASSERT(a, a->cell[0]->type == PVAL_QEXPR,
          "Function 'head' passed incorrect type!");
  PASSERT(a, a->cell[0]->count != 0,
          "Function 'head' passed {}!");

  pval *v = pval_take(a, 0);

  while (v->count > 1)
  {
    pval_del(pval_pop(v, 1));
  }
  return v;
}

pval *builtin_tail(pval *a)
{
  PASSERT(a, a->count == 1,
          "Function 'tail' passed too many arguments!");
  PASSERT(a, a->cell[0]->type == PVAL_QEXPR,
          "Function 'tail' passed incorrect type!");
  PASSERT(a, a->cell[0]->count != 0,
          "Function 'tail' passed {}!");

  pval *v = pval_take(a, 0);

  pval_del(pval_pop(v, 0));
  return v;
}

pval *builtin_list(pval *a)
{
  a->type = PVAL_QEXPR;
  return a;
}

pval *builtin_eval(pval *a)
{
  PASSERT(a, a->count == 1,
          "Function 'eval' passed too many arguments!");
  PASSERT(a, a->cell[0]->type == PVAL_QEXPR,
          "Function 'eval' passed incorrect type!");

  pval *x = pval_take(a, 0);
  x->type = PVAL_SEXPR;
  return pval_eval(x);
}

pval *pval_join(pval *x, pval *y)
{
  while (y->count)
  {
    x = pval_add(x, pval_pop(y, 0));
  }

  pval_del(y);
  return x;
}

pval *builtin_join(pval *a)
{

  for (int i = 0; i < a->count; i++)
  {
    PASSERT(a, a->cell[i]->type == PVAL_QEXPR,
            "Function 'join' passed incorrect type.");
  }

  pval *x = pval_pop(a, 0);

  while (a->count)
  {
    x = pval_join(x, pval_pop(a, 0));
  }

  pval_del(a);
  return x;
}

pval *builtin_op(pval *a, char *op)
{

  for (int i = 0; i < a->count; i++)
  {
    if (a->cell[i]->type != PVAL_NUM)
    {
      pval_del(a);
      return pval_err("Cannot operate on non-number!");
    }
  }

  pval *x = pval_pop(a, 0);

  if ((strcmp(op, "-") == 0) && a->count == 0)
  {
    x->num = -x->num;
  }

  while (a->count > 0)
  {

    pval *y = pval_pop(a, 0);

    if (strcmp(op, "+") == 0)
    {
      x->num += y->num;
    }
    if (strcmp(op, "-") == 0)
    {
      x->num -= y->num;
    }
    if (strcmp(op, "*") == 0)
    {
      x->num *= y->num;
    }
    if (strcmp(op, "/") == 0)
    {
      if (y->num == 0)
      {
        pval_del(x);
        pval_del(y);
        x = pval_err("Division By Zero.");
        break;
      }
      x->num /= y->num;
    }

    pval_del(y);
  }

  pval_del(a);
  return x;
}

pval *builtin(pval *a, char *func)
{
  if (strcmp("list", func) == 0)
  {
    return builtin_list(a);
  }
  if (strcmp("head", func) == 0)
  {
    return builtin_head(a);
  }
  if (strcmp("tail", func) == 0)
  {
    return builtin_tail(a);
  }
  if (strcmp("join", func) == 0)
  {
    return builtin_join(a);
  }
  if (strcmp("eval", func) == 0)
  {
    return builtin_eval(a);
  }
  if (strstr("+-/*", func))
  {
    return builtin_op(a, func);
  }
  pval_del(a);
  return pval_err("Unknown Function!");
}

pval *pval_eval_sexpr(pval *v)
{

  for (int i = 0; i < v->count; i++)
  {
    v->cell[i] = pval_eval(v->cell[i]);
  }

  for (int i = 0; i < v->count; i++)
  {
    if (v->cell[i]->type == PVAL_ERR)
    {
      return pval_take(v, i);
    }
  }

  if (v->count == 0)
  {
    return v;
  }

  if (v->count == 1)
  {
    return pval_take(v, 0);
  }

  pval *f = pval_pop(v, 0);
  if (f->type != PVAL_SYM)
  {
    pval_del(f);
    pval_del(v);
    return pval_err("S-expression Does not start with symbol.");
  }

  pval *result = builtin(v, f->sym);
  pval_del(f);
  return result;
}

pval *pval_eval(pval *v)
{
  if (v->type == PVAL_SEXPR)
  {
    return pval_eval_sexpr(v);
  }
  return v;
}

pval *pval_read_num(mpc_ast_t *t)
{
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? pval_num(x) : pval_err("invalid number");
}

pval *pval_read(mpc_ast_t *t)
{

  if (strstr(t->tag, "number"))
  {
    return pval_read_num(t);
  }
  if (strstr(t->tag, "symbol"))
  {
    return pval_sym(t->contents);
  }

  pval *x = NULL;
  if (strcmp(t->tag, ">") == 0)
  {
    x = pval_sexpr();
  }
  if (strstr(t->tag, "sexpr"))
  {
    x = pval_sexpr();
  }
  if (strstr(t->tag, "qexpr"))
  {
    x = pval_qexpr();
  }

  for (int i = 0; i < t->children_num; i++)
  {
    if (strcmp(t->children[i]->contents, "(") == 0)
    {
      continue;
    }
    if (strcmp(t->children[i]->contents, ")") == 0)
    {
      continue;
    }
    if (strcmp(t->children[i]->contents, "}") == 0)
    {
      continue;
    }
    if (strcmp(t->children[i]->contents, "{") == 0)
    {
      continue;
    }
    if (strcmp(t->children[i]->tag, "regex") == 0)
    {
      continue;
    }
    x = pval_add(x, pval_read(t->children[i]));
  }

  return x;
}

int main(int argc, char **argv)
{

  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Qexpr = mpc_new("qexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Plisp = mpc_new("plisp");

  mpca_lang(MPCA_LANG_DEFAULT,
            "                                                        \
    number : /-?[0-9]+/ ;                                  \
    symbol : \"list\" | \"head\" | \"tail\"                \
           | \"join\" | \"eval\" | '+' | '-' | '*' | '/' ; \
    sexpr  : '(' <expr>* ')' ;                             \
    qexpr  : '{' <expr>* '}' ;                             \
    expr   : <number> | <symbol> | <sexpr> | <qexpr> ;     \
    plisp  : /^/ <expr>* /$/ ;                             \
  ",
            Number, Symbol, Sexpr, Expr, Plisp);

  puts("Plisp Version 0.0.0.0.5");
  puts("Press Ctrl+c to Exit\n");

  while (1)
  {

    char *input = readline("plisp> ");
    add_history(input);

    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Plisp, &r))
    {
      pval *x = pval_eval(pval_read(r.output));
      pval_println(x);
      pval_del(x);
      mpc_ast_delete(r.output);
    }
    else if (strcmp(input, "exit") == 0)
    {
      break;
    }
    else if (strcmp(input, "clear") == 0)
    {
      system("clear");
    }
    else
    {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  mpc_cleanup(5, Number, Symbol, Sexpr, Qexpr, Expr, Plisp);

  return 0;
}