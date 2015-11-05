//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

#ifndef __FA_SIM_H__
#define __FA_SIM_H__

#include "fa.h"


typedef struct fa_sim_node_s {
  uint8_t flags;
#define FA_SIM_NODE_F_ACCEPTING (1 << 0)
  void *opaque;
  uint32_t table[256];
} fa_sim_node_t;

typedef struct fa_sim_s {
  uint32_t start;
  uint32_t nodes_n;
  uint32_t size; // sim size in bytes
  struct fa_sim_node_s nodes[0];
} fa_sim_t;

typedef struct fa_sim_run_s {
  uint32_t current;
  void *opaque;
} fa_sim_run_t;


fa_sim_t *fa_sim_create(fa_t *fa);
void fa_sim_destroy(fa_sim_t *sim);
#define FA_SIM_RUN_ACCEPT 1
#define FA_SIM_RUN_REJECT 2
#define FA_SIM_RUN_MORE	  3
void fa_sim_run_init(fa_sim_t *sim, fa_sim_run_t *fsr);
int fa_sim_run(fa_sim_t *sim, fa_sim_run_t *fsr,
               uint8_t *bytes, int len);

#endif
