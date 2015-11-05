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
#include <stdarg.h>
#include <inttypes.h>

#include "fa.h"
#include "fa_regexp.h"
#include "fa_sim.h"
#include "fa_graphviz.h"


static void *state_pri(void **opaques, int opaques_n) {
  // prioritized lowest number
  void *opaque = opaques[0];
  for (int i = 1; i < opaques_n; i++) {
    if ((intptr_t)opaques[i] < (intptr_t)opaque)
      opaque = opaques[i];
  }

  return opaque;
}

static int state_cmp(void *a, void *b) {
  // distinguish if different state opaque value
  return a != b;
}

int main(int argc, char **argv) {
  char *errstr = NULL;
  int errpos = 0;

  // setup memory allocation etc
  fa_init();

  // create union NFA of two regexp NFAs
  fa_t *a_fa = fa_regexp_fa("^aa*$", &errstr, &errpos, NULL);
  // assign 0 to all accepting states for first regexp
  fa_set_accepting_opaque(a_fa, (void *)0);
  fa_t *b_fa = fa_regexp_fa("^a(a|b)$", &errstr, &errpos, NULL);
  // assign 1 to all accepting states for second regexp
  fa_set_accepting_opaque(b_fa, (void *)1);
  fa_t *union_fa = fa_union(a_fa, b_fa);
  fa_graphviz_output(union_fa, "union.dot", NULL);

  // determinize and choose lowest opaque number when more than one opaque
  // value end up in same accepting state.
  // this happens when two regexps are overlapping.
  fa_t *dfa = fa_determinize_ex(union_fa, state_pri, NULL, NULL);
  fa_graphviz_output(dfa, "dfa.dot", NULL);

  // minimize and treat states with different opaque values as distinguishable.
  // this is to be able to know which original regexp has a match when using
  // the minimize DFA
  fa_t *mdfa = fa_minimize_ex(dfa, state_cmp, NULL);
  fa_destroy(dfa);
  fa_graphviz_output(mdfa, "mdfa.dot", NULL);

  // convert minimal DFA into format suitable for running it on input
  fa_sim_t *sim = fa_sim_create(mdfa);
  fa_destroy(mdfa);

  fa_sim_run_t fsr;

  fa_sim_run_init(sim, &fsr);
  if (fa_sim_run(sim, &fsr, (uint8_t *)"aa", 2) == FA_SIM_RUN_ACCEPT)
    printf("matches %p\n", fsr.opaque); // outputs matches 0x0
  fa_sim_run_init(sim, &fsr);
  if (fa_sim_run(sim, &fsr, (uint8_t *)"ab", 2) == FA_SIM_RUN_ACCEPT)
    printf("matches %p\n", fsr.opaque); // outputs matches 0x1

  fa_sim_destroy(sim);

  return 0;
}
