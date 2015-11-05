//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

#ifndef __FA_STATE_GROUP__H
#define __FA_STATE_GROUP__H

#include <sys/queue.h>
#include <inttypes.h>

#include "fa.h"

typedef LIST_HEAD(fa_state_group_head_s, fa_state_group_s)
  fa_state_group_head_t;

typedef struct fa_state_group_s {
  LIST_ENTRY(fa_state_group_s) link;
  fa_state_tqhead_t states;
} fa_state_group_t;

void fa_state_group_init(void);
fa_state_group_t *fa_state_group_create(void);
void fa_state_group_destroy(fa_state_group_t *fsg);
void fa_state_group_assign(fa_state_group_t *fsg, fa_state_t *state);
void fa_state_group_change(fa_state_group_t *from,
                           fa_state_group_t *to,
                           fa_state_t *state);
int fa_state_group_has_state(fa_state_group_t *fsg, fa_state_t *state);

#endif
