/*
 * Copyright (c) 2015 Waystream AB
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the NetBSD license.  See the LICENSE file for details.
 */

/* regexp tokenizer, is used by fa_regexp_yacc.y */

%option noyywrap stack nounput never-interactive noyy_top_state

%{

// ignore ‘input’ defined but not used
#pragma GCC diagnostic ignored "-Wunused-function"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "fa_regexp_yacc.h"

#define yylval yylibfalval
#define yylloc yylibfalloc

#define T(r) \
  yylval.token.pos = pos; \
  yylloc.first_column = pos; \
  pos += yyleng; \
  return r;

/* ignore */
#define I() pos += yyleng

YYSTYPE yylibfalval;
YYLTYPE yylibfalloc;

YY_BUFFER_STATE yb;
static int pos;

void fa_regexp_lex_start(char *str, int len);
void fa_regexp_lex_stop(void);
int fa_regexp_lex_pos(void);

%}

ANY     (.|\n)
HEX     [0-9a-fA-F]
OCT     [0-7]
WHITE   [ \t\f\n\r]*
DECNUM  0|[1-9][0-9]*
HEXNUM  0x[0-9a-fA-F]+
OCTNUM  0[0-7]+
NUM     {DECNUM}|{HEXNUM}|{OCTNUM}

%x STATE_REPEAT STATE_BINARY STATE_CLASS STATE_QUOTE
%x STATE_COMMENT STATE_OPTIONS

%%

<INITIAL>
{
  "|" { T(PIPE) }
  "*" { T(STAR) }
  "+" { T(PLUS) }
  "?" { T(QMARK) }
  "(" { T(LPAREN) }
  ")" { T(RPAREN) }
  "[" { yy_push_state(STATE_CLASS); T(LSBRACKET); }
  "{" { yy_push_state(STATE_REPEAT); T(LCBRACKET); }
  "(?B" { yy_push_state(STATE_BINARY); T(LPARENQB); }
  "(?#" { yy_push_state(STATE_COMMENT); I(); }
  "(?" { yy_push_state(STATE_OPTIONS); T(LPARENQ); }
}

<STATE_OPTIONS>
{
  ")" { yy_pop_state(); T(RPAREN); }
  "-" { T(MINUS); }
  {ANY} { yylval.token.v.c = yytext[0]; T(CHAR); }
}

<STATE_COMMENT>
{
  ")" { yy_pop_state(); I(); }
  {ANY} { I(); }
}

<STATE_REPEAT,STATE_BINARY>
{
  {WHITE}+ { I(); }
  {NUM} {
    yylval.token.v.i = strtol(yytext, NULL, 0);
    T(NUMBER);
  }
}

<STATE_REPEAT>
{
  "}" {
    yy_pop_state();
    T(RCBRACKET);
  }
  "," { T(COMMA); }
}

<STATE_BINARY>
{
  ")" {
    yy_pop_state();
    T(RPAREN);
  }
  "," { T(COMMA); }
  ":" { T(COLON); }
}

<STATE_CLASS>
{
  "]" {
    yy_pop_state();
    T(RSBRACKET);
  }
  "^" { T(CARET); }
  "-" { T(MINUS); }
  /* only inside class, \b is word boundary out side class */
  "\\b" { yylval.token.v.c = '\b'; T(CHAR); }
  "[:alnum:]" { yylval.token.v.s = "alnum"; T(CLASS); }
  "[:alpha:]" { yylval.token.v.s = "alpha"; T(CLASS); }
  "[:ascii:]" { yylval.token.v.s = "ascii"; T(CLASS); }
  "[:blank:]" { yylval.token.v.s = "blank"; T(CLASS); }
  "[:cntrl:]" { yylval.token.v.s = "cntrl"; T(CLASS); }
  "[:digit:]" { yylval.token.v.s = "digit"; T(CLASS); }
  "[:graph:]" { yylval.token.v.s = "graph"; T(CLASS); }
  "[:lower:]" { yylval.token.v.s = "lower"; T(CLASS); }
  "[:print:]" { yylval.token.v.s = "print"; T(CLASS); }
  "[:punct:]" { yylval.token.v.s = "punct"; T(CLASS); }
  "[:space:]" { yylval.token.v.s = "space"; T(CLASS); }
  "[:upper:]" { yylval.token.v.s = "upper"; T(CLASS); }
  "[:word:]"  { yylval.token.v.s = "word"; T(CLASS); }
  "[:xdigit:]" { yylval.token.v.s = "xdigit"; T(CLASS); }
  "[:^alnum:]" { yylval.token.v.s = "ALNUM"; T(CLASS); }
  "[:^alpha:]" { yylval.token.v.s = "ALPHA"; T(CLASS); }
  "[:^ascii:]" { yylval.token.v.s = "ASCII"; T(CLASS); }
  "[:^blank:]" { yylval.token.v.s = "BLANK"; T(CLASS); }
  "[:^cntrl:]" { yylval.token.v.s = "CNTRL"; T(CLASS); }
  "[:^digit:]" { yylval.token.v.s = "DIGIT"; T(CLASS); }
  "[:^graph:]" { yylval.token.v.s = "GRAPH"; T(CLASS); }
  "[:^lower:]" { yylval.token.v.s = "LOWER"; T(CLASS); }
  "[:^print:]" { yylval.token.v.s = "PRINT"; T(CLASS); }
  "[:^punct:]" { yylval.token.v.s = "PUNCT"; T(CLASS); }
  "[:^space:]" { yylval.token.v.s = "SPACE"; T(CLASS); }
  "[:^upper:]" { yylval.token.v.s = "UPPER"; T(CLASS); }
  "[:^word:]"  { yylval.token.v.s = "WORD"; T(CLASS); }
  "[:^xdigit:]" { yylval.token.v.s = "XDIGIT"; T(CLASS); }
  "." { yylval.token.v.c = yytext[0]; T(CHAR); }
}

<STATE_QUOTE>
{
  "\\E" { yy_pop_state(); I(); }
  {ANY} { yylval.token.v.c = yytext[0]; T(CHAR); }
}

<INITIAL,STATE_CLASS>
{
  "\\Q" { yy_push_state(STATE_QUOTE); I(); }
  "\\0" { yylval.token.v.c = '\0'; T(CHAR); }
  "\\a" { yylval.token.v.c = '\a'; T(CHAR); }
  "\\e" { yylval.token.v.c = '\e'; T(CHAR); }
  "\\f" { yylval.token.v.c = '\f'; T(CHAR); }
  "\\t" { yylval.token.v.c = '\t'; T(CHAR); }
  "\\n" { yylval.token.v.c = '\n'; T(CHAR); }
  "\\r" { yylval.token.v.c = '\r'; T(CHAR); }
  "\\c". {
    yylval.token.v.c =
      /* to upper and invert bit 7 */
      (toupper(yytext[2]) & ~0x40) |
      (~toupper(yytext[2]) & 0x40);
    T(CHAR);
  }
  "\\d" { yylval.token.v.s = "d"; T(CLASS); }
  "\\D" { yylval.token.v.s = "D"; T(CLASS); }
  "\\s" { yylval.token.v.s = "s"; T(CLASS); }
  "\\S" { yylval.token.v.s = "S"; T(CLASS); }
  "\\h" { yylval.token.v.s = "h"; T(CLASS); }
  "\\H" { yylval.token.v.s = "H"; T(CLASS); }
  "\\v" { yylval.token.v.s = "v"; T(CLASS); }
  "\\V" { yylval.token.v.s = "V"; T(CLASS); }
  "\\w" { yylval.token.v.s = "w"; T(CLASS); }
  "\\W" { yylval.token.v.s = "W"; T(CLASS); }
  "." { yylval.token.v.s = "."; T(CLASS); }
  "\\x"{HEX}?{HEX}? {
    yylval.token.v.c = strtol(yytext + 2, NULL, 16);
    T(CHAR);
  }
  "\\"[0-3]?{OCT}?{OCT}? {
    yylval.token.v.c = strtol(yytext + 1, NULL, 8);
    T(CHAR);
  }
  "\\". { yylval.token.v.c = yytext[1]; T(CHAR); }
}

<INITIAL,STATE_REPEAT,STATE_BINARY,STATE_CLASS>
{
  {ANY} { yylval.token.v.c = yytext[0]; T(CHAR); }
}

%%

void fa_regexp_lex_start(char *str, int len) {
  pos = 1;
  BEGIN(INITIAL);
  yy_scan_bytes(str, len);
}

void fa_regexp_lex_stop(void) {
  yylibfalex_destroy();
}

int fa_regexp_lex_pos(void) {
  return pos;
}
