//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fa.h"
#include "fa_sim.h"


fa_sim_t *fa_sim_create(fa_t *fa) {
  fa_sim_t *sim;
  fa_state_t *fs;
  int i, s;

  i = 1; // 0 reserved for no match state
  LIST_FOREACH(fs, &fa->states, link)
    fs->opaque_temp = (void *)(intptr_t)i++;

  s = sizeof(*sim) + sizeof(sim->nodes[0]) * i;
  sim = calloc(1, s);
  sim->size = s;

  sim->start = (intptr_t)fa->start->opaque_temp;
  sim->nodes_n = i;

  LIST_FOREACH(fs, &fa->states, link) {
    fa_trans_t *ft;
    int node = (intptr_t)fs->opaque_temp;

    if (fs->flags & FA_STATE_F_ACCEPTING)
      sim->nodes[node].flags |= FA_SIM_NODE_F_ACCEPTING;
    sim->nodes[node].opaque = fs->opaque;

    LIST_FOREACH(ft, &fs->trans, link) {
      // unused symbols relies on calloc to be transitions to state 0
      for (i = ft->symfrom; i <= ft->symto; i++)
        sim->nodes[node].table[i] = (intptr_t)ft->state->opaque_temp;
    }
  }

  return sim;
}

void fa_sim_destroy(fa_sim_t *sim) {
  free(sim);
}

void fa_sim_run_init(fa_sim_t *sim, fa_sim_run_t *fsr) {
  fsr->current = sim->start;
}

int fa_sim_run(fa_sim_t *sim, fa_sim_run_t *fsr, uint8_t *bytes, int len) {
  int i;

  for (i = 0; i < len; i++) {
    fsr->current = sim->nodes[fsr->current].table[bytes[i]];
    if (fsr->current == 0)
      return FA_SIM_RUN_REJECT;
  }

  if (sim->nodes[fsr->current].flags & FA_SIM_NODE_F_ACCEPTING) {
    fsr->opaque = sim->nodes[fsr->current].opaque;
    return FA_SIM_RUN_ACCEPT;
  }

  return FA_SIM_RUN_MORE;
}
