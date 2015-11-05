//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

// build fa for matching at bit level, <:4,0:4>
// there are some constraints:
// must be byte aligned
// bin parts are currently limited to max 32 bit

// TODO: binary base numbers?
// TODO: arbitrary large numbers?

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <errno.h>
#include <arpa/inet.h> // htonl
#include <inttypes.h>

#include "fa_regexp_bin.h"
#include "fa_misc.h"
#include "fa.h"

// do not use BITFEIELD_SET as it stores bits in reverse in each byte
#define _BIT_SET(f,b) ((f)[(b)/8] |= 1<<(7-((b)&7)))

static fa_regexp_bin_t *fa_regexp_bin_create(uint32_t value, uint32_t bits,
                                             int iswild) {
  fa_regexp_bin_t *bin;
  fa_regexp_bin_part_t *bp;

  bin = malloc(sizeof(*bin));
  TAILQ_INIT(&bin->head);

  if (bits > sizeof(value) * 8)
    bits = sizeof(value) * 8;

  bp = calloc(1, sizeof(*bp));
  bp->bits = bits;
  if (bits % 8 == 0)
    bp->bytes = bits / 8;
  else
    bp->bytes = (bits + (8 - (bits % 8))) / 8;

  if (!iswild) {
    uint8_t *p = (uint8_t*)&value;

    bp->buf = malloc(bp->bytes);
    value = htonl(value);
    memcpy(bp->buf, p + sizeof(value) - bp->bytes, bp->bytes);
  }

  TAILQ_INSERT_TAIL(&bin->head, bp, link);

  return bin;
}

fa_regexp_bin_t *fa_regexp_bin_create_value(uint32_t value, uint32_t bits) {
  return fa_regexp_bin_create(value, bits, 0);
}

fa_regexp_bin_t *fa_regexp_bin_create_wild(uint32_t bits) {
  return fa_regexp_bin_create(0, bits, 1);
}

void fa_regexp_bin_destroy(fa_regexp_bin_t *bin) {
  while (!TAILQ_EMPTY(&bin->head)) {
    fa_regexp_bin_part_t *bp = TAILQ_FIRST(&bin->head);
    TAILQ_REMOVE(&bin->head, bp, link);

    if (bp->buf)
      free(bp->buf);
    free(bp);
  }

  free(bin);
}

fa_regexp_bin_t *fa_regexp_bin_merge(fa_regexp_bin_t *a, fa_regexp_bin_t *b) {
  if (!a || !b) {
    if (a)
      fa_regexp_bin_destroy(a);
    if (b)
      fa_regexp_bin_destroy(b);

    return NULL;
  }

  while (!TAILQ_EMPTY(&b->head)) {
    fa_regexp_bin_part_t *bp = TAILQ_FIRST(&b->head);
    TAILQ_REMOVE(&b->head, bp, link);

    TAILQ_INSERT_TAIL(&a->head, bp, link);
  }

  fa_regexp_bin_destroy(b);

  return a;
}

static int permute_byte(uint8_t value, uint8_t mask, uint8_t *buf) {
  int i;

  // all bit permutations, might have dups
  for (i = 0; i < 256; i++)
    buf[i] = ((uint8_t)i & ~mask) | value;

  return fa_unique_array(buf, 256, sizeof(buf[0]));
}

static void bin_setbits(uint8_t *buf, int offset, uint8_t val, int len) {
  if (offset % 8 == 0 && len % 8 == 0) {
    buf[offset / 8] |= val;
  } else {
    int i;

    for (i = 0; i < len; i++) {
      if (val & (1 << (len - i - 1)))
        _BIT_SET(buf, offset + i);
    }
  }
}

int fa_regexp_bin_bitlen(fa_regexp_bin_t *bin) {
  fa_regexp_bin_part_t *bp;
  int n = 0;

  TAILQ_FOREACH(bp, &bin->head, link)
    n += bp->bits;

  return n;
}

static int fa_regexp_bin_flatten(fa_regexp_bin_t *bin,
                                 uint8_t **valuebuf, uint8_t **maskbuf) {
  fa_regexp_bin_part_t *bp;
  int buflen;
  int offset;

  buflen = fa_regexp_bin_bitlen(bin);
  if (buflen % 8 != 0) {
    return -1;
  }
  buflen /= 8;

  *valuebuf = calloc(1, buflen);
  *maskbuf = calloc(1, buflen);

  offset = 0;

  TAILQ_FOREACH(bp, &bin->head, link) {
    int i;

    for (i = 0; i < bp->bytes; i++) {
      int n;

      n = bp->bits - (bp->bytes - i - 1) * 8;
      if (n > 8)
        n = 8;

      if (bp->buf) {
        bin_setbits(*valuebuf, offset, bp->buf[i], n);
        bin_setbits(*maskbuf, offset, 0xff, n);
      }
      offset += n;
    }
  }

  return buflen;
}

fa_t *fa_regexp_bin_fa(fa_regexp_bin_t *bin) {
  uint8_t permbuf[256];
  uint8_t *valuebuf, *maskbuf;
  fa_t *fa;
  fa_state_t *fs, *prev;
  int i, j, n, m;

  n = fa_regexp_bin_flatten(bin, &valuebuf, &maskbuf);
  if (n == -1)
    return NULL;

  fa = fa_create();
  fa->start = fa_state_create(fa);

  prev = fa->start;
  for (i = 0; i < n; i++) {
    fs = fa_state_create(fa);

    m = permute_byte(valuebuf[i], maskbuf[i], permbuf);
    for (j = 0; j < m; j++)
      fa_trans_create(prev, permbuf[j], fs);

    prev = fs;
  }

  prev->flags |= FA_STATE_F_ACCEPTING;

  free(valuebuf);
  free(maskbuf);

  return fa;
}
