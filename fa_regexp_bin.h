//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

#ifndef __FA_REGEXP_BIN_H__
#define __FA_REGEXP_BIN_H__

#include <sys/queue.h>
#include <inttypes.h>

#include "fa.h"


typedef struct fa_regexp_bin_part_s {
  TAILQ_ENTRY(fa_regexp_bin_part_s) link;
  int bits;
  int bytes; // number of bytes aligned
  uint8_t *buf; // NULL if wildcard
} fa_regexp_bin_part_t;

// is own type mostly just to make it easy to change things later
typedef struct fa_regexp_bin_s {
  TAILQ_HEAD(, fa_regexp_bin_part_s) head;
} fa_regexp_bin_t;


fa_regexp_bin_t *fa_regexp_bin_create_value(uint32_t value, uint32_t bits);
fa_regexp_bin_t *fa_regexp_bin_create_wild(uint32_t bits);
void fa_regexp_bin_destroy(fa_regexp_bin_t *bin);
fa_regexp_bin_t *fa_regexp_bin_merge(fa_regexp_bin_t *a, fa_regexp_bin_t *b);
int fa_regexp_bin_append(fa_regexp_bin_t *bin, uint32_t value, uint32_t bits);
int fa_regexp_bin_append_wild(fa_regexp_bin_t *bin, uint32_t bits);
fa_t *fa_regexp_bin_fa(fa_regexp_bin_t *bin);
int fa_regexp_bin_bitlen(fa_regexp_bin_t *bin);

#endif
