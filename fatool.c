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
#include <getopt.h>

#include "fa.h"
#include "fa_graphviz.h"
#include "fa_graphviz_tikz.h"
#include "fa_text.h"
#include "fa_regexp.h"
#include "fa_sim.h"
#include "fa_misc.h"


typedef struct format_s {
  char *name;
  fa_t *(*input)(char *arg);
  int (*output)(fa_t *fa, char *arg, char *label);
} format_t;

typedef struct pattern_overlap_s {
  struct pattern_s *pat;
  int count;
} pattern_overlap_t;

typedef struct pattern_s {
  char *in;
  fa_t *fa;
  int n;
  int overlap_n;
  struct pattern_overlap_s **overlap;
  int accepting;
} pattern_t;


static void point_out(FILE *s, int width, char *str, int pos) {
  int len = strlen(str);
  int strunc, etrunc;
  int i;

  strunc = MMAX(0, MMIN(len - width, pos - (width/2)));
  etrunc = MMAX(0, MMIN(len - width, len - (pos + (width/2))));

  fprintf(s,"%s%.*s%s\n",
          strunc ? "..." : "",
          width - (strunc ? 3 : 0) - (etrunc ? 3 : 0),
          strunc + str + (strunc ? 3 : 0),
          etrunc ? "..." : "");

  for (i = 0; i < pos - strunc - 1; i++)
    fprintf(s, " ");
  fprintf(s, "^\n");
}

static fa_t *fa_regexp_input(char *arg) {
  fa_t *fa;
  char *errstr = NULL;
  int errpos;

  fa = fa_regexp_fa(arg, &errstr, &errpos, NULL);

  if (errstr) {
    fprintf(stderr, "Failed, %s at position %d:\n", errstr, errpos);
    point_out(stderr, 40, arg, errpos);

    return NULL;
  }

  return fa;
}

format_t formats[] = {
  {"text:", fa_text_input, fa_text_output},
  {"re:", fa_regexp_input, NULL},
  {"dot:", NULL, fa_graphviz_output},
  {"dottikz:", NULL, fa_graphviz_tikz_output},
  {NULL, NULL, NULL}
};

static format_t *get_format(char **str) {
  int i;

  for (i = 0; formats[i].name; i++) {
    if (strncmp(*str, formats[i].name, strlen(formats[i].name)) != 0)
      continue;

    *str += strlen(formats[i].name);
    return &formats[i];
  }

  return NULL;
}

static char *state_name(fa_state_t *state) {
  static char b[512];
  pattern_t *pat = state->opaque;

  if (!pat)
    return "";

  snprintf(b, sizeof(b), "%d", pat->n);

  return b;
}

static void pattern_overlap(pattern_t *pat, pattern_t *o) {
  int i;

  for (i = 0; i < pat->overlap_n; i++)
    if (pat->overlap[i]->pat == o) {
      pat->overlap[i]->count++;
      return;
    }

  pat->overlap = realloc(pat->overlap,
                         sizeof(pat->overlap[0]) * (pat->overlap_n + 1));
  pat->overlap[pat->overlap_n] = malloc(sizeof(*pat->overlap[0]));
  pat->overlap[pat->overlap_n]->pat = o;
  pat->overlap[pat->overlap_n]->count = 1;
  pat->overlap_n++;
}

static void *state_pri(void **opaques, int opaques_n) {
  pattern_t **pl = (pattern_t**)opaques;
  pattern_t *w;
  int i;

  w = pl[0];
  for (i = 1; i < opaques_n; i++)
    if (pl[i]->n < w->n)
      w = pl[i];

  for (i = 0; i < opaques_n; i++)
    if (pl[i] != w)
      pattern_overlap(pl[i], w);

  return w;
}

static int state_cmp(void *a, void *b) {
  if (a == b)
    return 0;
  else
    return 1;
}

static void count_accepting(void *opaque) {
  pattern_t *pat = opaque;

  pat->accepting++;
}

int main(int argc, char **argv) {
  int dfa = 0;
  int min = 0;
  char *label = NULL;
  char *in = NULL;
  char *out = NULL;
  char *test = NULL;
  format_t *inh = NULL, *outh = NULL;
  fa_t *fa, *tfa;
  pattern_t **inpat = NULL;
  int inpat_n = 0;
  int i;

  fa_init();

  fa_graphviz_set_state_name_cb(state_name);

  while (1) {
    int c;
    int index;
    struct option options[] = {
      // name, has_arg, flag, val
      {"in", 1, NULL, 'i'},
      {"out", 1, NULL, 'o'},
      {"label", 1, NULL, 'l'},
      {"dfa", 0, &dfa, 1},
      {"min", 0, &min, 1},
      {"test", 1, NULL, 't'},
      {0, 0, 0, 0}
    };

    c = getopt_long(argc, argv, "i:o:l:dmt:", options, &index);
    if (c == -1)
      break;

    switch (c) {
      case 0:
        break; // flag i set
      case 'i':

        in = optarg;

        inpat = realloc(inpat, sizeof(inpat[0]) * (inpat_n + 1));
        inpat[inpat_n] = malloc(sizeof(*inpat[0]));
        inpat[inpat_n]->in = strdup(in);
        inpat[inpat_n]->n = inpat_n;
        inpat[inpat_n]->overlap = NULL;
        inpat[inpat_n]->overlap_n = 0;
        inpat[inpat_n]->accepting = 0;

        inh = get_format(&in);
        if (!inh || !inh->input) {
          fprintf(stderr, "--in not supported for format %s\n", in);
          exit(1);
        }

        inpat[inpat_n]->fa = inh->input(in);
        if (!inpat[inpat_n]->fa) {
          fprintf(stderr, "in format %s failed with argument %s\n",
                  inh->name, in);
          exit(1);
        }
        fa_set_accepting_opaque(inpat[inpat_n]->fa, inpat[inpat_n]);
        inpat_n++;

        break;
      case 'o':
        out = optarg;
        break;
      case 'l':
        label = optarg;
        break;
      case 't':
        test = optarg;
        break;
      case '?':
        break;
      default:
        fprintf(stderr, "unknown option %d\n", c);
        exit(1);
        break;
    }
  }

  if (inpat_n == 0) {
    fprintf(stderr, "please specify --in\n");
    exit(1);
  }

  if (test == NULL && !out) {
    fprintf(stderr, "please specify --out or --test %p\n", test);
    exit(1);
  }

  if (!test) {
    outh = get_format(&out);
    if (test == NULL && (!outh || !outh->output)) {
      fprintf(stderr, "--out not supported for format %s\n", out);
      exit(1);
    }
  }

  if (inpat_n > 1) {
    fa_t **infa;

    infa = malloc(sizeof(inpat[0]) * inpat_n);
    for (i = 0; i < inpat_n; i++) {
      infa[i] = inpat[i]->fa;
      fprintf(stderr, "NFA[%s] states=%d trans=%d\n",
              (char*)inpat[i]->in, inpat[i]->fa->states_n,
              inpat[i]->fa->trans_n);
    }

    fa = fa_union_list(infa, inpat_n);
    free(infa);
  } else {
    fa = inpat[0]->fa;
  }

  fprintf(stderr, "NFA: states=%d trans=%d\n", fa->states_n, fa->trans_n);

  if (dfa) {
    tfa = fa;
    fa = fa_determinize_ex(fa, state_pri, NULL, NULL);
    fa_destroy(tfa);
    fprintf(stderr, "DFA: states=%d trans=%d\n", fa->states_n, fa->trans_n);
  }

  if (min) {
    tfa = fa;
    fa = fa_minimize_ex(fa, state_cmp, NULL);
    fa_destroy(tfa);
    fprintf(stderr, "MDFA: states=%d trans=%d\n", fa->states_n, fa->trans_n);
  }

  fa_foreach_accepting(fa, count_accepting);

  for (i = 0; i < inpat_n; i++) {
    int j;

    fprintf(stderr, "%s: overlap_n=%d accepting=%d %s\n",
            inpat[i]->in,
            inpat[i]->overlap_n, inpat[i]->accepting,
            inpat[i]->overlap_n == 0 ? "no overlap" :
            inpat[i]->overlap_n > 0 && inpat[i]->accepting ? "partial overlap" :
            "full overlap");
    for (j = 0; j < inpat[i]->overlap_n; j++)
      fprintf(stderr, "  %s (%d)\n",
              inpat[i]->overlap[j]->pat->in,
              inpat[i]->overlap[j]->count);
  }

  if (test) {
    fa_sim_t *sim;
    fa_sim_run_t run;

    sim = fa_sim_create(fa);
    fa_sim_run_init(sim, &run);

    switch (fa_sim_run(sim, &run, (uint8_t *)test, strlen(test))) {
    case FA_SIM_RUN_ACCEPT:
      fprintf(stderr, "match %d\n", ((pattern_t*)run.opaque)->n);
      break;
    case FA_SIM_RUN_REJECT:
      fprintf(stderr, "no match\n");
      break;
    case FA_SIM_RUN_MORE:
      fprintf(stderr, "more\n");
      break;
    }

    fa_sim_destroy(sim);
  } else if (!outh->output(fa, out, label)) {
    fprintf(stderr, "out format %s failed with argument %s\n", outh->name, out);
    exit(1);
  }

  for (i = 0; i < inpat_n; i++) {
    int j;

    for (j = 0; j < inpat[i]->overlap_n; j++)
      free(inpat[i]->overlap[j]);

    free(inpat[i]->overlap);
    free(inpat[i]->in);
    free(inpat[i]);
  }
  free(inpat);

  fa_destroy(fa);

  return 0;
}
