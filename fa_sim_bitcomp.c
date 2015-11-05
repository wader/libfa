//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

// TODO: current fa_sim_bitcomp_create uses a fa_sim_t, could be independent

// fa sim using compressed transition tables. Compression is done by storing
// when there is a change of destination state in the transition table. This
// is efficient when most states have few transitions.
//
// State memory looks like this:
//
// 256 bit       transition change bitmap, 1=change, 0=no change
// sizeof(*void) accepting state opaque pointer
// 32 bit * N    compressed transitions
//
// Lookup of next state is done by counting number of bits left of index
// (input byte) in change bitmap. Count is then use as offset into compressed
// transition table.
// first bit in change bitmap is always zero so it is used to indicates if
// state is accepting.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "fa_sim_bitcomp.h"
#include "fa_sim.h"

#define BITFIELD64_TEST(f,b)  ((f)[(b)/64] & 1ULL<<(63-((b)&63)))
#define BITFIELD64_SET(f,b)   ((f)[(b)/64] |= 1ULL<<(63-((b)&63)))


// only count if node is NULL
static uint32_t fa_sim_bitcomp_table_compress(uint32_t *in,
                                              fa_sim_bitcomp_node_t *node,
                                              uint32_t *locs) {
  uint32_t prev = in[0];
  int n = 1;
  int i;

  // always at least one value

  if (node)
    node->table[0] = locs[in[0]];

  // skip 0, first bit i always zero (index 0)
  for (i = 1; i < 256; i++) {
    if (in[i] != prev) {
      if (node) {
        BITFIELD64_SET(node->bitmap, i);
        node->table[n] = locs[in[i]];
      }
      prev = in[i];
      n++;
    }
  }

  return n;
}

fa_sim_bitcomp_t *fa_sim_bitcomp_create(fa_sim_t *sim) {
  uint32_t size;
  uint32_t i;
  uint32_t *locs;
  fa_sim_bitcomp_t *fsb;

  // each fa_sim_t state iteration skips state 0, it is reserved for no match

  // locs it used to store offset (in 64 bit steps) to each node
  locs = malloc(sizeof(locs[0]) * sim->nodes_n);
  locs[0] = 0; // fa_sim_t uses 0 as not match state, map it to 0

  // size of header
  size = sizeof(fa_sim_bitcomp_t);

  // size state 0
  size += sizeof(uint64_t);
  // size rest of states
  for (i = 1;i < sim->nodes_n; i++) {
    locs[i] = (size - sizeof(fa_sim_bitcomp_t)) / sizeof(fsb->nodes[0]);

    size +=
      sizeof(fa_sim_bitcomp_node_t) +
      sizeof(uint32_t) *
        fa_sim_bitcomp_table_compress(sim->nodes[i].table, NULL, NULL);

    // 64 bit align
    if (size % 8 != 0)
      size += 8 - (size % 8);
  }

  fsb = calloc(1, size);
  fsb->size = size;

  // store bitmap and tables
  for (i = 1;i < sim->nodes_n; i++) {
    fa_sim_bitcomp_node_t *fsbn =
      (fa_sim_bitcomp_node_t*)&fsb->nodes[locs[i]];

    fa_sim_bitcomp_table_compress(sim->nodes[i].table, fsbn, locs);
    if (sim->nodes[i].flags & FA_SIM_NODE_F_ACCEPTING) {
      BITFIELD64_SET(fsbn->bitmap, 0);
      fsbn->opaque = sim->nodes[i].opaque;
    }
  }

  fsb->start = locs[sim->start];

  free(locs);

  return fsb;
}

void fa_sim_bitcomp_destroy(fa_sim_bitcomp_t *fsb) {
  free(fsb);
}

static uint32_t popcount_bitmap(uint64_t *bitmap, uint8_t index) {
  int p, l;

  // 2 highest bits is number of 64 bit maps we need to popcount
  l = index >> 6;

  // first map only popcount bits left of index
  p = __builtin_popcountll(bitmap[l] >> (63 - (index & 63)));

  // popcount rest of maps using jump table
  switch(l) {
    case 3: p += __builtin_popcountll(bitmap[--l]);
    case 2: p += __builtin_popcountll(bitmap[--l]);
    case 1: p += __builtin_popcountll(bitmap[--l]);
    // case 0 is nop
  }

  // -1 to not count accepting bit
  if (BITFIELD64_TEST(bitmap, 0))
    p--;

  return p;
}

void fa_sim_bitcomp_run_init(fa_sim_bitcomp_t *fsb, fa_sim_run_t *fsr) {
  fsr->current = fsb->start;
}

int fa_sim_bitcomp_run(fa_sim_bitcomp_t *fsb, fa_sim_run_t *fsr,
                       uint8_t *data, int len) {
  int i;
  uint32_t current = fsr->current;
  fa_sim_bitcomp_node_t *node =
    (fa_sim_bitcomp_node_t*)&fsb->nodes[current];

  for (i = 0; i < len; i++) {
    current = node->table[popcount_bitmap(node->bitmap, data[i])];
    node = (fa_sim_bitcomp_node_t*)&fsb->nodes[current];

    if (current == 0)
      return FA_SIM_RUN_REJECT;
  }

  if (BITFIELD64_TEST(node->bitmap, 0)) {
    fsr->opaque = node->opaque;
    return FA_SIM_RUN_ACCEPT;
  }

  fsr->current = current;

  return FA_SIM_RUN_MORE;
}
