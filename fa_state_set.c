//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

// TODO: use lists instead arrays, maybe faster

#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include "fa_state_set.h"
#include "fa_misc.h"
#include "fa.h"


static void *fa_state_set_t_pool;
static void *fa_state_set_syms_t_pool;

void fa_state_set_init(void) {
  fa_state_set_t_pool = fa_mempool_create("fa_state_set_t",
                                          sizeof(fa_state_set_t));
  fa_state_set_syms_t_pool = fa_mempool_create("fa_state_set_syms_t",
                                               sizeof(fa_state_set_syms_t));
}


static fa_state_set_syms_t *fa_state_set_syms_create(void) {
  return fa_mempool_alloc(fa_state_set_syms_t_pool);
}

static void fa_state_set_syms_destroy(fa_state_set_syms_t *fsss) {
  fa_mempool_free(fa_state_set_syms_t_pool, fsss);
}

fa_state_set_t *fa_state_set_create(void) {
  fa_state_set_t *fss = fa_mempool_alloc(fa_state_set_t_pool);

  fss->flags = 0;
  fss->states_n = 0;
  fss->states_alloc_n = 0;
  fss->states = NULL;
  fss->syms = NULL;

  return fss;
}

void fa_state_set_destroy(fa_state_set_t *fss) {
  if (fss->syms)
    fa_state_set_syms_destroy(fss->syms);
  free(fss->states);
  fa_mempool_free(fa_state_set_t_pool, fss);
}

int fa_state_set_has_state(fa_state_set_t *fss, fa_state_t *state) {
  int i;

  for (i = 0; i < fss->states_n; i++)
    if (fss->states[i] == state)
      return 1;

  return 0;
}

int fa_state_set_add(fa_state_set_t *fss, fa_state_t *state) {
  if (fa_state_set_has_state(fss, state))
    return 0;

  if (state->flags & FA_STATE_F_ACCEPTING)
    fss->flags |= FA_STATE_F_ACCEPTING;

  if (fss->states_n == fss->states_alloc_n) {
    fss->states_alloc_n += 16; // tests have shown that 16 is quite good
    fss->states = realloc(fss->states,
                          sizeof(fss->states[0]) * fss->states_alloc_n);
  }
  fss->states[fss->states_n++] = state;

  return 1;
}

void fa_state_set_syms(fa_state_set_t *fss) {
  fa_trans_t *ft;
  int i, j;

  if (fss->syms)
    return;

  fss->syms = fa_state_set_syms_create();

  for (i = 0; i < fss->states_n; i++)
    LIST_FOREACH(ft, &fss->states[i]->trans, link)
      for (j = ft->symfrom; j <= ft->symto; j++)
        if (ft->symfrom != FA_SYMBOL_E)
          BITFIELD_SET(fss->syms->map, j);
}

// state set cmp and sort are used by fa_determinize
static int fa_state_set_sort_cmp(const void *a, const void *b) {
    return (intptr_t)a - (intptr_t)b;
}

void fa_state_set_sort(fa_state_set_t *fss) {
  qsort(fss->states, fss->states_n, sizeof(fss->states[0]),
        fa_state_set_sort_cmp);
}

int fa_state_set_cmp(fa_state_set_t *a, fa_state_set_t *b) {
  return
    a->states_n == b->states_n &&
    memcmp(a->states, b->states, a->states_n * sizeof(a->states[0])) == 0;
}
