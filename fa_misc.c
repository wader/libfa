//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

#if defined(__linux__)
// for qsort_r
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/param.h>

#include "fa_misc.h"

#if defined(__linux__)
static int unique_array_cmp(const void *a, const void *b, void *arg) {
  return memcmp(a, b, (uintptr_t)arg);
}
#elif defined(BSD)
static int unique_array_cmp(void *arg, const void *a, const void *b) {
  return memcmp(a, b, (uintptr_t)arg);
}
#else
#error unknown platform
#endif

int fa_unique_array(void *base, int nmemb, size_t size) {
  int i, n;
  uint8_t *p, *b = base;

  // sort elements
  #ifdef __linux__
  qsort_r(base, nmemb, size, unique_array_cmp, (void *)(uintptr_t)size);
  #else
  qsort_r(base, nmemb, size, (void *)(uintptr_t)size, unique_array_cmp);
  #endif

  // remove non-unique
  p = base;
  n = 1;
  for (i = 1; i < nmemb; i++) {
    if (memcmp(b+(i*size), p, size) != 0)
      memmove(b+(n++)*size, b+(i*size), size);
    p = b+(i*size);
  }

  return n;
}

void *fa_libcmem_create(char *name, size_t size) {
  return (void *)(intptr_t)size;
}

void *fa_libcmem_alloc(void *pool) {
  return calloc(1, (intptr_t)pool);
}

void fa_libcmem_free(void *pool, void *ptr) {
  free(ptr);
}
