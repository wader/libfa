//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

#ifndef __FA_REGEXP_H__
#define __FA_REGEXP_H__

#include <inttypes.h>

#include "fa.h"
#include "fa_regexp_bin.h"
#include "fa_regexp_class.h"

typedef enum {
  RE_SUB,
  RE_OPTIONS,
  RE_CONCAT,
  RE_UNION,
  RE_REPEAT,
  RE_STRING,
  RE_CLASS,
  RE_BINARY
} fa_regexp_type_t;

typedef struct fa_regexp_node_s {
  fa_regexp_type_t type;
#define FA_REGEXP_F_ICASE (1 << 0)
  uint32_t flags;
  union {
    struct {
      uint32_t flags;
      struct fa_regexp_node_s *sub;
    } sub;
    struct {
      int neg;
      uint32_t flags;
      struct fa_regexp_node_s *sub;
    } options;
    struct {
      struct fa_regexp_node_s *sub1;
      struct fa_regexp_node_s *sub2;
    } concat;
    struct {
      struct fa_regexp_node_s *sub1;
      struct fa_regexp_node_s *sub2;
    } union_;
    struct {
      struct fa_regexp_node_s *sub;
      int onlymin;
      int min;
      int max;
    } repeat;
    struct {
      int len;
      char *str;
    } string;
    struct {
      int neg;
      fa_regexp_class_t *class_;
    } class_;
    fa_regexp_bin_t *binary;
  } value;
  int pos;
} fa_regexp_node_t;


fa_regexp_node_t *fa_regexp_node_sub(fa_regexp_node_t *sub, int pos);
fa_regexp_node_t *fa_regexp_node_options(int neg, uint32_t flags,
                                         fa_regexp_node_t *sub, int pos);
fa_regexp_node_t *fa_regexp_node_concat(fa_regexp_node_t *sub1,
                                        fa_regexp_node_t *sub2, int pos);
fa_regexp_node_t *fa_regexp_node_union(fa_regexp_node_t *sub1,
                                       fa_regexp_node_t *sub2, int pos);
fa_regexp_node_t *fa_regexp_node_repeat(fa_regexp_node_t *sub,
                                        int onlymin, int min, int max, int pos);
fa_regexp_node_t *fa_regexp_node_string(char *str, int len, int pos);
fa_regexp_node_t *fa_regexp_node_class(int neg, fa_regexp_class_t *class_,
                                       int pos);
fa_regexp_node_t *fa_regexp_node_binary(fa_regexp_bin_t *bin, int pos);

#if 0
// dump tree
void fa_regexp_node_dump(fa_regexp_node_t *node);
// dump in latex pgf format
void fa_regexp_node_dump_pgf(fa_regexp_node_t *node);
#endif

// in fa_regexp_yacc.y
fa_regexp_node_t *fa_regexp_yacc_parse(char *str, int len,
                                       char **errstr, int *errpos);

// in fa_regexp_lex.l
void fa_regexp_lex_start(char *str, int len);
void fa_regexp_lex_stop(void);
int fa_regexp_lex_pos(void);
int yylex(void);

void fa_regexp_node_free(fa_regexp_node_t *node);
fa_t *fa_regexp_fa(char *str, char **errstr, int *errpos, fa_limit_t *limit);

#endif
