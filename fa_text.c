//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

// file format:
//
// # state 1, transition to state 2, epsilon transition state 2
// 1:
//  a -> 2
//  -> 2
//
// # state 2 with one b transition going to it self
// 2:
//  b -> b

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/queue.h>

#include "fa.h"
#include "fa_text.h"


static fa_state_t *fa_text_find_state(fa_t *fa, int n) {
  fa_state_t *fs;

  LIST_FOREACH(fs, &fa->states, link) {
    if ((intptr_t)fs->opaque_temp == n)
      return fs;
  }

  return NULL;
}

static fa_t *fa_text_input_ex(FILE *s) {
  fa_t *fa = fa_create();
  char buf[512];
  char *delim;

  while (fgets(buf, sizeof(buf), s)) {
    char *p = buf;
    int indent;
    fa_state_t *current = NULL;

    while (*p == ' ')
      p++;

    if (*p == '#')
      continue;

    indent = (p != buf);

    if (indent) {
      int symbol;
      fa_state_t *dest;

      if (!current)
        continue; // not in state yet

      delim = strstr(p, "->");
      if (!delim)
        continue;

      *delim = '\0';

      if (delim == p) {
        symbol = FA_SYMBOL_E;
      } else {
        if (strcmp(p, "0x") == 0) {
          symbol = strtod(p, NULL);
        } else {
          symbol = p[0];
        }
      }

      // skip "->"
      delim += 2;

      dest = fa_text_find_state(fa, atoi(delim));
      if (!dest) {
        dest = fa_state_create(fa);
        dest->opaque_temp = (void *)(intptr_t)atoi(delim);
      }

      fa_trans_create(current, symbol, dest);
    } else {
      delim = strchr(p, ':');
      if (!delim)
        continue;

      *delim++ = '\0';

      current = fa_text_find_state(fa, atoi(p));
      if (!current) {
        current = fa_state_create(fa);
        current->opaque_temp = (void *)(intptr_t)atoi(p);
      }

      if (strchr(delim, 's'))
        fa->start = current;
      else if (strchr(delim, 't'))
        current->flags |= FA_STATE_F_ACCEPTING;
    }
  }

  return fa;
}

fa_t *fa_text_input(char *arg) {
  FILE *s;
  fa_t *fa;

  if (strcmp(arg, "-") == 0)
    s = stdin;
  else {
    s = fopen(arg, "rb");
    if (s == NULL)
      return NULL;
  }

  fa = fa_text_input_ex(s);

  if (s != stdin)
    fclose(s);

  return fa;
}

static void fa_text_output_ex(fa_t *fa, FILE *s, char *label) {
  fa_state_t *fs;
  int i;

  i = 1;
  LIST_FOREACH(fs, &fa->states, link) {
    fs->opaque_temp = (void *)(intptr_t)i;
    i++;
  }

  if (label)
    fprintf(s, "# %s\n", label);

  LIST_FOREACH(fs, &fa->states, link) {
    fa_trans_t *ft;

    fprintf(s, "%ld:", (intptr_t)fs->opaque_temp);
    if (fs->flags & FA_STATE_F_ACCEPTING)
      fprintf(s, "t");
    if (fs == fa->start)
      fprintf(s, "s");
    fprintf(s, "\n");

    LIST_FOREACH(ft, &fs->trans, link) {
      int i;

      if (!ft->state)
        continue;

      for (i = ft->symfrom; i <= ft->symto; i++) {
        fprintf(s, "  ");
        if (i == FA_SYMBOL_E)
          ;
        else if (isprint(i))
          fprintf(s, "%c", i);
        else
          fprintf(s, "0x%x", i);
        fprintf(s, " -> %ld\n", (intptr_t)ft->state->opaque_temp);
      }
    }
  }
}

int fa_text_output(fa_t *fa, char *arg, char *label) {
  FILE *s;

  if (strcmp(arg, "-") == 0)
    s = stdout;
  else {
    s = fopen(arg, "wb");
    if (s == NULL)
      return 0;
  }

  fa_text_output_ex(fa, s, label);

  if (s != stdout)
    fclose(s);

  return 1;
}
