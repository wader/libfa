//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

#include <stdlib.h>
#include <sys/queue.h>

#include "fa_state_set_hash.h"
#include "fa_state_set.h"
#include "fa.h"


fa_state_set_hash_t *fa_state_set_hash_create(int size) {
  int i;

  fa_state_set_hash_t *fssh = malloc(sizeof(*fssh));
  fssh->size = size;
  fssh->table = malloc(sizeof(fssh->table[0]) * fssh->size);
  for (i = 0; i < fssh->size; i++)
    LIST_INIT(&fssh->table[i]);

  return fssh;
}

void fa_state_set_hash_destroy(fa_state_set_hash_t *fssh) {
  free(fssh->table);
  free(fssh);
}

static uint32_t fa_state_set_hash_fn(fa_state_set_t *fss) {
  int i;
  uint32_t s;

  s = 0;
  for (i = 0; i < fss->states_n; i++)
    s = ((s << 5) + s) + (uintptr_t)fss->states[i];

  return s;
}

void fa_state_set_hash_add(fa_state_set_hash_t *fssh,
                           fa_state_set_t *fss,
                           void *opaque) {
  fss->opaque = opaque;
  LIST_INSERT_HEAD(&fssh->table[fa_state_set_hash_fn(fss) % fssh->size],
                   fss, temp);
}

void *fa_state_set_hash_find(fa_state_set_hash_t *fssh,
                             fa_state_set_t *fss) {
  fa_state_set_t *tfss;

  LIST_FOREACH(tfss, &fssh->table[fa_state_set_hash_fn(fss) % fssh->size],
               temp)
    if (fa_state_set_cmp(tfss, fss))
      return tfss->opaque;

  return NULL;
}
