//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

// state can only be a member of one state group at a time, this is
// different from state set where one state can be a member of many
// state sets

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include <assert.h>

#include "fa.h"
#include "fa_state_group.h"


static void *fa_state_group_t_pool;

void fa_state_group_init(void) {
  fa_state_group_t_pool = fa_mempool_create("fa_state_group_t",
                                            sizeof(fa_state_group_t));
}

fa_state_group_t *fa_state_group_create(void) {
  fa_state_group_t *fsg = fa_mempool_alloc(fa_state_group_t_pool);

  TAILQ_INIT(&fsg->states);

  return fsg;
}

void fa_state_group_destroy(fa_state_group_t *fsg) {
  fa_mempool_free(fa_state_group_t_pool, fsg);
}

void fa_state_group_assign(fa_state_group_t *fsg, fa_state_t *fs) {
  TAILQ_INSERT_HEAD(&fsg->states, fs, tempq);
}

void fa_state_group_change(fa_state_group_t *from,
                           fa_state_group_t *to,
                           fa_state_t *fs) {
  TAILQ_REMOVE(&from->states, fs, tempq);
  TAILQ_INSERT_HEAD(&to->states, fs, tempq);
}

int fa_state_group_has_state(fa_state_group_t *fsg, fa_state_t *fs) {
  fa_state_t *fst;

  TAILQ_FOREACH(fst, &fsg->states, tempq)
    if (fst == fs)
      return 1;

  return 0;
}
