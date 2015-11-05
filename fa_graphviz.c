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
#include <ctype.h>
#include <sys/queue.h>

#include "fa.h"
#include "fa_graphviz.h"


static fa_graphviz_state_name_f *state_name_cb = NULL;

void fa_graphviz_set_state_name_cb(fa_graphviz_state_name_f cb) {
  state_name_cb = cb;
}

static char *strescape(char *s, char *esc) {
  static char *b = NULL;
  char *p;

  if (b) {
    free(b);
    b = NULL;
  }

  if (!s)
    return NULL;

  b = malloc(strlen(s) * 2 + 1);
  p = b;

  for (; *s; s++) {
    if (strchr(esc, *s))
      *p++ = '\\';
    *p++ = *s;
  }
  *p = '\0';

  return b;
}

static char *to_escape(int c) {
  static char *t[16] = {
    ['\0'] = "\\0",
    ['\a'] = "\\a",
    ['\t'] = "\\t",
    ['\n'] = "\\n",
    ['\v'] = "\\v",
    ['\f'] = "\\f",
    ['\r'] = "\\r"
  };

  if (c < 0 || c > 15)
    return NULL;
  else
    return t[c];
}

static char *state_name(fa_state_t *state) {
  static char b[512];

  if (!(state->flags & FA_STATE_F_ACCEPTING))
    return "";

  snprintf(b, sizeof(b), "%ld", (intptr_t)state->opaque);

  return b;
}

static void fa_graphviz_output_ex_symbol(FILE *s, fa_symbol_t symbol) {
  if (symbol == FA_SYMBOL_E)
    fprintf(s, "\xc6\x90"); // UTF-8 epsilon
  else if (isprint(symbol)) {
    char b[] = {symbol, '\0'};
    fprintf(s, "%s", strescape(b, "\\\""));
  } else {
    if (to_escape(symbol))
      fprintf(s, "%s", strescape(to_escape(symbol), "\\"));
    else
      fprintf(s, "0x%x", (unsigned char)symbol);
  }
}

static int trans_cmp(const void *a, const void *b) {
  const fa_trans_t *fta = *(fa_trans_t **)a, *ftb = *(fa_trans_t **)b;

  if (fta->state->opaque_temp == ftb->state->opaque_temp)
    return fta->symfrom - ftb->symfrom;
  else
    return fta->state->opaque_temp - ftb->state->opaque_temp;
}

static void fa_graphviz_output_ex(fa_t *fa, FILE *s, char *label) {
  fa_state_t *fs;
  int i, n;
  fa_trans_t **sorted = NULL;
  int sorted_n = 0;

  i = 1;
  LIST_FOREACH(fs, &fa->states, link) {
    fs->opaque_temp = (void *)(intptr_t)i;
    i++;
  }

  fprintf(s, "digraph fa {\n");
  fprintf(s, "\trankdir=LR;\n");
  fprintf(s, "\tnode [shape=circle style=filled fillcolor=\"#f0f0f0\"]\n");
  if (label)
    fprintf(s, "\tgraph [label=\"%s\"]\n", strescape(label, "\\"));
  fprintf(s, "\tstart [shape=point];\n");
  fprintf(s, "\tstart -> %ld\n", (intptr_t)fa->start->opaque_temp);

  LIST_FOREACH(fs, &fa->states, link) {
    fprintf(s, "\t\%ld [label=\"%s\"",
    (intptr_t)fs->opaque_temp, strescape(state_name_cb(fs), "\""));

    if (fs->flags & FA_STATE_F_ACCEPTING)
      fprintf(s, " shape=doublecircle");

    fprintf(s, "];\n");
  }

  LIST_FOREACH(fs, &fa->states, link) {
    fa_trans_t *ft;

    n = 0;
    LIST_FOREACH(ft, &fs->trans, link)
      n++;

    if (n > sorted_n) {
      sorted = realloc(sorted, sizeof(sorted[0]) * n);
      sorted_n = n;
    }

    i = 0;
    LIST_FOREACH(ft, &fs->trans, link)
      sorted[i++] = ft;

    qsort(sorted, n, sizeof(sorted[0]), trans_cmp);

    for (i = 0; i < n; i++) {
      int j, k;

      for (j = i+1; j < n; j++)
        if (sorted[i]->state->opaque_temp != sorted[j]->state->opaque_temp)
          break;

      fprintf(s, "\t\%ld -> ", (intptr_t)fs->opaque_temp);
      fprintf(s, "%ld ", (intptr_t)sorted[i]->state->opaque_temp);
      fprintf(s, "[label=\"");

      for (k = i; k < j; k++) {
        fa_graphviz_output_ex_symbol(s, sorted[k]->symfrom);

        if (sorted[k]->symto - sorted[k]->symfrom > 1)
          fprintf(s, "-");
        if (sorted[k]->symto - sorted[k]->symfrom > 0) {
          if (sorted[k]->symto - sorted[k]->symfrom == 1)
            fprintf(s, ",");
          fa_graphviz_output_ex_symbol(s, sorted[k]->symto);
        }

        if (k < j - 1)
          fprintf(s, ",");
      }

      fprintf(s, "\"];\n");

      i = j - 1;
    }
  }

  if (sorted)
    free(sorted);

  // free memory
  strescape(NULL, NULL);

  fprintf(s, "}\n");
}

int fa_graphviz_output(fa_t *fa, char *arg, char *label) {
  FILE *s;

  if (strcmp(arg, "-") == 0)
    s = stdout;
  else {
    s = fopen(arg, "wb");
    if (s == NULL)
      return 0;
  }

  if (!state_name_cb)
    state_name_cb = state_name;

  fa_graphviz_output_ex(fa, s, label);

  if (s != stdout)
    fclose(s);

  return 1;
}
