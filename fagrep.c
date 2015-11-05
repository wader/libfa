//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fa.h"
#include "fa_regexp.h"
#include "fa_sim.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s regex\n", argv[0]);
    return 1;
  }

  fa_init();

  int errpos;
  char *errstr;
  fa_t *fa = fa_regexp_fa(argv[1], &errstr, &errpos, NULL);
  if (fa == NULL) {
    fprintf(stderr, "%d:%s\n", errpos, errstr);
    return 1;
  }

  fa_t *dfa = fa_determinize(fa);
  fa_destroy(fa);
  fa_t *mdfa = fa_minimize(dfa);
  fa_destroy(dfa);
  fa_sim_t *sim = fa_sim_create(mdfa);
  fa_destroy(mdfa);

  char b[65536];
  while (fgets(b, sizeof(b), stdin)) {
    fa_sim_run_t fsr;

    fa_sim_run_init(sim, &fsr);
    if (fa_sim_run(sim, &fsr, (uint8_t *)b, strlen(b)) == FA_SIM_RUN_ACCEPT)
      fprintf(stdout, "%s", b);
  }

  fa_sim_destroy(sim);

  return 0;
}
