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
  int i;
  for (i = 1; i < opaques_n; i++) {
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
  // (1) fa_t *a_fa = fa_regexp_fa("^(a|b)*ab(a|b)*|(a|b)*a|b*$", &errstr, &errpos, NULL);
  // (2) fa_t *a_fa = fa_regexp_fa("^(a|b)*(ab|ba)(a|b)*|a*|b*$", &errstr, &errpos, NULL);
  fa_t *a_fa = fa_regexp_fa("^(_|[a-zA-Z])(_|[a-zA-Z]|[0-9])*$", &errstr, &errpos, NULL);


  // assign 0 to all accepting states for first regexp
  fa_set_accepting_opaque(a_fa, (void *)0);
  // fa_t *b_fa = fa_regexp_fa("^a(a|b)$", &errstr, &errpos, NULL);
  // assign 1 to all accepting states for second regexp
  // fa_set_accepting_opaque(b_fa, (void *)1);
  // fa_t *union_fa = fa_union(a_fa, b_fa);
  fa_graphviz_output(a_fa, "pics/nfa.dot", NULL);

  // determinize and choose lowest opaque number when more than one opaque
  // value end up in same accepting state.
  // this happens when two regexps are overlapping.
  fa_t *dfa = fa_determinize_ex(a_fa, state_pri, NULL, NULL);
  fa_graphviz_output(dfa, "pics/dfa.dot", NULL);

  // minimize and treat states with different opaque values as distinguishable.
  // this is to be able to know which original regexp has a match when using
  // the minimize DFA
  fa_t *mdfa = fa_minimize_ex(dfa, state_cmp, NULL);
  fa_destroy(dfa);
  fa_graphviz_output(mdfa, "pics/mdfa.dot", NULL);

  return 0;
}
