//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

#ifndef __FA_SIM_BITCOMP16_H__
#define __FA_SIM_BITCOMP16_H__

#include "fa_sim.h"

typedef struct fa_sim_bitcomp_node_s {
  uint64_t bitmap[4]; // 256 bits
  void *opaque;
  uint32_t table[0];
} fa_sim_bitcomp_node_t;

typedef struct fa_sim_bitcomp_s {
  uint32_t start;
  uint32_t size; // sim size in bytes
  uint64_t nodes[0];
} fa_sim_bitcomp_t;

fa_sim_bitcomp_t *fa_sim_bitcomp_create(fa_sim_t *sim);
void fa_sim_bitcomp_destroy(fa_sim_bitcomp_t *fsb);
void fa_sim_bitcomp_run_init(fa_sim_bitcomp_t *fsb, fa_sim_run_t *fsr);
int fa_sim_bitcomp_run(fa_sim_bitcomp_t *fsb, fa_sim_run_t *fsr,
                       uint8_t *data, int len);

#endif
