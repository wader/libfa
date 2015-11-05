//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

#ifndef __FA_STATE_SET_HASH__
#define __FA_STATE_SET_HASH__

#include "fa.h"
#include "fa_state_set.h"


typedef struct fa_state_set_hash_s {
  int size;
  fa_state_set_head_t *table;
} fa_state_set_hash_t;

fa_state_set_hash_t *fa_state_set_hash_create(int size);
void fa_state_set_hash_destroy(fa_state_set_hash_t *fssh);
void fa_state_set_hash_add(fa_state_set_hash_t *fssh,
                           fa_state_set_t *fss,
                           void *opaque);
void *fa_state_set_hash_find(fa_state_set_hash_t *fssh,
                             fa_state_set_t *fss);
#endif
