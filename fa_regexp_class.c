//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

// build fa for matching regexp character classes
// [a-z], \w, [:digit:], unions, negation, etc

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/queue.h>
#include <ctype.h>
#include <assert.h>

#include "fa_regexp_class.h"
#include "fa_misc.h"


// set if dot should match any character, otherwise anything except \n
int fa_regexp_class_dot_all = 0;

static fa_regexp_class_chars_t *fa_regexp_class_chars_create(void) {
  fa_regexp_class_chars_t *rcc = calloc(1, sizeof(*rcc));

  rcc->linked = 0;
  rcc->neg = 0;

  return rcc;
}

static void fa_regexp_class_chars_destroy(fa_regexp_class_chars_t *rcc) {
  if (rcc->linked)
    LIST_REMOVE(rcc, link);
  free(rcc);
}

static void fa_regexp_class_chars_neg(fa_regexp_class_chars_t *rcc) {
  int i;

  for (i = 0; i < ARRAYSIZEOF(rcc->map); i++)
    rcc->map[i] = ~rcc->map[i];
}

static int fa_regexp_class_chars_is_empty(fa_regexp_class_chars_t *rcc) {
  int i;

  for (i = 0; i < ARRAYSIZEOF(rcc->map); i++)
    if (rcc->map[i])
      return 0;

  return 1;
}

static fa_regexp_class_t *fa_regexp_class_create(void) {
  fa_regexp_class_t *rc;

  rc = malloc(sizeof(*rc));
  LIST_INIT(&rc->head);

  return rc;
}

void fa_regexp_class_destroy(fa_regexp_class_t *rc) {

  while (!LIST_EMPTY(&rc->head))
    fa_regexp_class_chars_destroy(LIST_FIRST(&rc->head));

  free(rc);
}

static void fa_regexp_class_add_chars(fa_regexp_class_t *rc,
                                      fa_regexp_class_chars_t *rcc) {
  LIST_INSERT_HEAD(&rc->head, rcc, link);
  rcc->linked = 1;
}

fa_regexp_class_t *fa_regexp_class_merge(fa_regexp_class_t *a,
                                         fa_regexp_class_t *b) {
  if (!a || !b) {
    if (a)
      fa_regexp_class_destroy(a);
    if (b)
      fa_regexp_class_destroy(b);

    return NULL;
  }

  while (!LIST_EMPTY(&b->head)) {
    fa_regexp_class_chars_t *rcc = LIST_FIRST(&b->head);
    LIST_REMOVE(rcc, link);
    fa_regexp_class_add_chars(a, rcc);
  }

  fa_regexp_class_destroy(b);

  return a;
}

static fa_regexp_class_chars_t *fa_regexp_class_flatten(fa_regexp_class_t *rc,
                                                        int neg, int icase) {
  fa_regexp_class_chars_t *f = fa_regexp_class_chars_create();
  fa_regexp_class_chars_t *rcc;
  int r;

  LIST_FOREACH(rcc, &rc->head, link) {
    int i;

    for (i = 0; i < 256; i++) {
      r = 0;
      if (icase && isalpha(i)) {
        if (BITFIELD_TEST(rcc->map, toupper(i)) ||
           BITFIELD_TEST(rcc->map, tolower(i)))
          r = 1;
      } else {
        if (BITFIELD_TEST(rcc->map, i))
          r = 1;
      }

      if (rcc->neg)
        r = !r;

      if (!r)
        continue;

      BITFIELD_SET(f->map, i);
    }
  }

  if (neg)
    fa_regexp_class_chars_neg(f);

  return f;
}

fa_regexp_class_t *fa_regexp_class_named(char *name) {
  fa_regexp_class_t *rc = NULL;
  fa_regexp_class_chars_t *rcc;

  if (strcmp(name, ".") == 0) {
    if (fa_regexp_class_dot_all)
      rc = fa_regexp_class_range((int[]){0, 255}, 1);
    else
      rc = fa_regexp_class_range((int[]){0, 9, 11, 255}, 2);
  } else if (strcasecmp(name, "d") == 0 || strcasecmp(name, "digit") == 0)
    rc = fa_regexp_class_range((int[]){'0', '9'}, 1);
  else if (strcasecmp(name, "s") == 0 || strcasecmp(name, "space") == 0)
    // before pcre 8.34 \v was not part of \s
    rc = fa_regexp_class_list(" \t\r\v\f\n", 6);
  else if (strcasecmp(name, "h") == 0)
    // pcre matches non-ascii \xa0 (NBSP) when in non-utf8 mode
    rc = fa_regexp_class_list(" \t\xa0", 3);
  else if (strcasecmp(name, "v") == 0)
    // pcre matches non-ascii \x85 (NEL, next line) when in non-utf8 mode
    rc = fa_regexp_class_list("\r\v\f\n\x85", 5);
  else if (strcasecmp(name, "w") == 0 || strcasecmp(name, "word") == 0) {
    rc = fa_regexp_class_range((int[]){'a', 'z', 'A', 'Z', '0', '9'}, 3);
    rc = fa_regexp_class_merge(rc, fa_regexp_class_list("_", 1));
  } else if (strcasecmp(name, "alnum") == 0) {
    rc = fa_regexp_class_range((int[]){'a', 'z', 'A', 'Z', '0', '9'}, 3);
  } else if (strcasecmp(name, "alpha") == 0) {
    rc = fa_regexp_class_range((int[]){'a', 'z', 'A', 'Z'}, 2);
  } else if (strcasecmp(name, "ascii") == 0) {
    rc = fa_regexp_class_range((int[]){0, 127}, 1);
  } else if (strcasecmp(name, "blank") == 0) {
    rc = fa_regexp_class_list(" \t", 2);
  } else if (strcasecmp(name, "cntrl") == 0) {
    rc = fa_regexp_class_range((int[]){0, 0x1f}, 1);
    rc = fa_regexp_class_merge(rc, fa_regexp_class_list("\x7f", 1));
  } else if (strcasecmp(name, "graph") == 0) {
    // space (0x20) not included
    rc = fa_regexp_class_range((int[]){0x21, 0x7e}, 1);
  } else if (strcasecmp(name, "lower") == 0) {
    rc = fa_regexp_class_range((int[]){'a', 'z'}, 1);
  } else if (strcasecmp(name, "upper") == 0) {
    rc = fa_regexp_class_range((int[]){'A', 'Z'}, 1);
  } else if (strcasecmp(name, "print") == 0) {
    rc = fa_regexp_class_range((int[]){0x20, 0x7e}, 1);
  } else if (strcasecmp(name, "punct") == 0) {
    // print - digits and letters
    rc = fa_regexp_class_list("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~", 32);
  } else if (strcasecmp(name, "xdigit") == 0) {
    rc = fa_regexp_class_range((int[]){'a', 'f', 'A', 'F', '0', '9'}, 3);
  }

  assert(rc);

  // negate class, WORD ([:^word:]), \W etc
  if (isalpha(name[0]) && isupper(name[0])) {
    rcc = fa_regexp_class_flatten(rc, 0, 0);
    // negate when flatten, fixes issues with "(?i)[:^lower:]"
    // would match anything
    rcc->neg = 1;
    fa_regexp_class_destroy(rc);
    rc = fa_regexp_class_create();
    fa_regexp_class_add_chars(rc, rcc);
  }

  return rc;
}

fa_regexp_class_t *fa_regexp_class_range(int *pairs, int pairs_n) {
  fa_regexp_class_t *rc;
  fa_regexp_class_chars_t *rcc;
  int i;
  int j;

  rc = fa_regexp_class_create();
  rcc = fa_regexp_class_chars_create();
  
  for (i = 0; i < pairs_n; i++) {
    for (j = pairs[i*2]; j <= pairs[i*2+1]; j++)
      BITFIELD_SET(rcc->map, j);
  }

  fa_regexp_class_add_chars(rc, rcc);

  return rc;
}

fa_regexp_class_t *fa_regexp_class_list(char *chars, int len) {
  fa_regexp_class_t *rc;
  fa_regexp_class_chars_t *rcc;
  int i;

  rc = fa_regexp_class_create();
  rcc = fa_regexp_class_chars_create();

  for (i = 0; i < len; i++)
    BITFIELD_SET(rcc->map, (uint8_t)chars[i]);

  fa_regexp_class_add_chars(rc, rcc);

  return rc;
}

fa_t *fa_regexp_class_fa(fa_regexp_class_t *rc, int neg, int icase) {
  fa_t *fa;
  fa_regexp_class_chars_t *rcc;

  fa = NULL;
  rcc = fa_regexp_class_flatten(rc, neg, icase);
  if (!fa_regexp_class_chars_is_empty(rcc)) {
    fa_state_t *end;
    int i;

    fa = fa_create();
    fa->start = fa_state_create(fa);
    end = fa_state_create(fa);
    end->flags |= FA_STATE_F_ACCEPTING;

    for (i = 0; i < 256; i++)
      if (BITFIELD_TEST(rcc->map, i))
        fa_trans_create(fa->start, i, end);
  }

  fa_regexp_class_chars_destroy(rcc);

  return fa;
}
