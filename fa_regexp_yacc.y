//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

// regexp grammar, build regexp tree used by fa_regexp.c

%{

#include <stdio.h>
#include <string.h>

#include "fa_regexp.h"
#include "fa_regexp_class.h"
#include "fa_regexp_bin.h"


void yyerror(const char *s);
void fa_regexp_error(const char *str, int pos);

#define err(str, pos) \
  fa_regexp_error(str, pos);

static int *errorpos;
static char **errorstr;

static fa_regexp_node_t *root;

%}

%defines
%error-verbose

%token CHAR
%token PIPE STAR PLUS QMARK
%token LPAREN RPAREN LSBRACKET RSBRACKET NUMBER
%token LCBRACKET RCBRACKET
%token LPARENQB
%token GT
%token COMMA COLON CARET
%token CLASS
%token LPARENQ MINUS

%union {
  struct {
    int pos;
    union {
      unsigned char c;
      int i;
      char *s;
    } v;
  } token;
  struct fa_regexp_node_s *regexp;
  struct fa_regexp_bin_s *binary;
  struct fa_regexp_class_s *class_;
  unsigned int uint;
};

%type <token> CHAR NUMBER CLASS LPAREN RPAREN STAR QMARK PLUS LPARENQB GT
%type <token> PIPE COMMA LCBRACKET LSBRACKET LPARENQ MINUS
%type <regexp> start
%type <regexp> regexp regexp2 regexp3 regexp4
%type <regexp> group symbol binary class
%type <class_> classexps classexp
%type <binary> binexps binexp
%type <uint> options option

%destructor {
  if(*errorstr && $$)
    fa_regexp_node_free($$);
} start regexp regexp2 regexp3 regexp4 group symbol binary class

%destructor {
  if(*errorstr && $$)
    fa_regexp_bin_destroy($$);
} binexps binexp

%destructor {
  if(*errorstr && $$)
    fa_regexp_class_destroy($$);
} classexps classexp

%%

start: regexp { $$ = root = $1; }


regexp:
regexp2 |
regexp PIPE regexp {
  $$ = fa_regexp_node_union($1, $3, $2.pos);
} |
/* a(b|) etc */
/* empty */ {
  $$ = fa_regexp_node_string("", 0, 0);
} |
error PIPE error {
  err("invalid union expression", $2.pos);
  $$ = NULL;
}

regexp2:
regexp3 |
LPARENQ options RPAREN regexp2 {
  $$ = fa_regexp_node_options(0, $2, $4, $1.pos);
} |
LPARENQ MINUS options RPAREN regexp2 {
  $$ = fa_regexp_node_options(1, $3, $5, $1.pos);
} |
regexp2 regexp2 {
  $$ = fa_regexp_node_concat($1, $2, @1.first_column);
} |
/* empty */ {
  $$ = fa_regexp_node_string("", 0, 0);
}

regexp3:
regexp4 |
regexp3 STAR {
  $$ = fa_regexp_node_repeat($1, 0, 0, 0, $2.pos);
} |
regexp3 PLUS {
  $$ = fa_regexp_node_repeat($1, 0, 1, 0, $2.pos);
} |
regexp3 QMARK {
  $$ = fa_regexp_node_repeat($1, 0, 0, 1, $2.pos);
} |
regexp3 LCBRACKET NUMBER RCBRACKET {
  /* onlymin set to be able to distinguish {0} (skip) from {0,} */
  $$ = fa_regexp_node_repeat($1, 1, $3.v.i, $3.v.i, $2.pos);
} |
regexp3 LCBRACKET NUMBER COMMA RCBRACKET {
  $$ = fa_regexp_node_repeat($1, 0, $3.v.i, 0, $2.pos);
} |
regexp3 LCBRACKET NUMBER COMMA NUMBER RCBRACKET {
  $$ = fa_regexp_node_repeat($1, 0, $3.v.i, $5.v.i, $2.pos);
}

regexp4:
group |
symbol |
binary |
class

class:
CLASS {
  $$ = fa_regexp_node_class(0, fa_regexp_class_named($1.v.s), $1.pos);
} |
LSBRACKET classexps RSBRACKET {
  $$ = fa_regexp_node_class(0, $2, $1.pos);
} |
LSBRACKET CARET classexps RSBRACKET {
  $$ = fa_regexp_node_class(1, $3, $1.pos);
}

classexps:
classexp |
classexps classexp {
  $$ = fa_regexp_class_merge($1, $2);
}

classexp:
CLASS {
  $$ = fa_regexp_class_named($1.v.s);
} |
CHAR {
  char b[] = {$1.v.c};
  $$ = fa_regexp_class_list(b, 1);
} |
MINUS {
  char b[] = {'-'};
  $$ = fa_regexp_class_list(b, 1);
} |
CHAR MINUS {
  /* ugly */
  char b[] = {$1.v.c, '-'};
  $$ = fa_regexp_class_list(b, 2);
} |
CHAR CARET {
  /* ugly */
  char b[] = {$1.v.c, '^'};
  $$ = fa_regexp_class_list(b, 2);
} |
MINUS CARET {
  /* ugly */
  char b[] = {'-', '^'};
  $$ = fa_regexp_class_list(b, 2);
} |
CHAR MINUS CHAR {
  if($1.v.c > $3.v.c) {
    err("invalid character range", $1.pos);
    $$ = NULL;
  } else {
    $$ = fa_regexp_class_range((int[]){$1.v.c, $3.v.c}, 1);
  }
}

binary:
LPARENQB binexps RPAREN {
  $$ = fa_regexp_node_binary($2, $1.pos);
}

binexps:
binexp |
binexps COMMA binexp {
  $$ = fa_regexp_bin_merge($1, $3);
}

binexp:
NUMBER COLON NUMBER {
  $$ = fa_regexp_bin_create_value($1.v.i, $3.v.i);
} |
NUMBER {
  $$ = fa_regexp_bin_create_value($1.v.i, 8 /* bits */);
} |
COLON NUMBER {
  $$ = fa_regexp_bin_create_wild($2.v.i);
} |
error {
  err("invalid binary expression", yylloc.first_column);
  $$ = NULL;
}

group:
LPAREN regexp RPAREN {
  $$ = fa_regexp_node_sub($2, $1.pos);
} |
LPAREN error {
  err("unmatched sub expression start", $1.pos);
  $$ = NULL;
} |
error RPAREN {
  err("unmatched sub expression end", $2.pos);
  $$ = NULL;
}

symbol:
CHAR {
  char b[] = {$1.v.c};
  $$ = fa_regexp_node_string(b, 1, $1.pos);
}

options:
option |
options option {
  $$ = $1 | $2;
}

option:
CHAR {
  if ($1.v.c == 'i') {
    $$ = FA_REGEXP_F_ICASE;
  } else {
    err("unknown option", $1.pos);
  }
}

%%

void yyerror(const char *str) {
  fa_regexp_error(str, yylloc.first_column);
}

void fa_regexp_error(const char *str, int pos) {
  if (*errorstr)
    free(*errorstr);
  *errorpos = pos;
  *errorstr = strdup(str);
}

fa_regexp_node_t *fa_regexp_yacc_parse(char *str, int len,
                                       char **errstr, int *errpos) {
  int r;

  errorpos = errpos;
  errorstr = errstr;
  *errorpos = 0;
  *errorstr = NULL;

  fa_regexp_lex_start(str, len);
  r = yyparse();
  fa_regexp_lex_stop();

  if (r == 0)
    return root;

  return NULL;
}
