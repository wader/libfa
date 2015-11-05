//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

#ifndef __FA_REGEXP_CLASS_H__
#define __FA_REGEXP_CLASS_H__

#include <sys/queue.h>
#include <inttypes.h>

#include "fa.h"


typedef struct fa_regexp_class_s {
  LIST_HEAD(, fa_regexp_class_chars_s) head;
} fa_regexp_class_t;

typedef struct fa_regexp_class_chars_s {
  LIST_ENTRY(fa_regexp_class_chars_s) link;
  int linked;
  int neg;
  uint8_t map[256 / 8];
} fa_regexp_class_chars_t;

extern int fa_regexp_class_dot_all;

void fa_regexp_class_destroy(fa_regexp_class_t *rc);
fa_regexp_class_t *fa_regexp_class_clone(fa_regexp_class_t *c);
fa_regexp_class_t *fa_regexp_class_merge(fa_regexp_class_t *a,
					                               fa_regexp_class_t *b);
fa_regexp_class_t *fa_regexp_class_named(char *name);
fa_regexp_class_t *fa_regexp_class_range(int *pairs, int pairs_n);
fa_regexp_class_t *fa_regexp_class_list(char *chars, int len);
int fa_regexp_class_has_chars(fa_regexp_class_t *rc);
fa_t *fa_regexp_class_fa(fa_regexp_class_t *rc, int neg, int icase);

#endif
