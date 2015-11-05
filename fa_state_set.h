//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

#ifndef __FA_STATE_SET__H
#define __FA_STATE_SET__H

#include <inttypes.h>

#include "fa.h"

typedef LIST_HEAD(fa_state_set_head_s, fa_state_set_s) fa_state_set_head_t;


typedef struct fa_state_set_syms_s {
  int symbols_n;
  int symbols_alloc_n;
  fa_symbol_t *symbols;
  uint8_t map[256 / 8]; // symbol bitmap
} fa_state_set_syms_t;

typedef struct fa_state_set_s {
  LIST_ENTRY(fa_state_set_s) temp;

  uint32_t flags;

  void *opaque; // used by fa_state_set_hash

  int states_n;
  int states_alloc_n;
  fa_state_t **states;

  fa_state_set_syms_t *syms;

} fa_state_set_t;

void fa_state_set_init(void);
fa_state_set_t *fa_state_set_create(void);
void fa_state_set_destroy(fa_state_set_t *fss);
int fa_state_set_has_state(fa_state_set_t *fss, fa_state_t *state);
int fa_state_set_has_symbol(fa_state_set_t *fss, fa_symbol_t symbol);
int fa_state_set_add(fa_state_set_t *fss, fa_state_t *state);
void fa_state_set_syms(fa_state_set_t *fss);
void fa_state_set_sort(fa_state_set_t *fss);
int fa_state_set_cmp(fa_state_set_t *a, fa_state_set_t *b);
void fa_state_set_dump(fa_state_set_t *fss);

#endif
