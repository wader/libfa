//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

#ifndef LIST_INSERT_SORTED
#define LIST_INSERT_SORTED(head, elm, field, cmpfunc) do { \
        if (LIST_EMPTY(head)) {                            \
           LIST_INSERT_HEAD(head, elm, field);             \
        } else {                                           \
           typeof(elm) _tmp;                               \
           LIST_FOREACH(_tmp,head,field) {                 \
              if (cmpfunc(elm,_tmp) <= 0) {                \
                LIST_INSERT_BEFORE(_tmp,elm,field);        \
                break;                                     \
              }                                            \
              if (!LIST_NEXT(_tmp,field)) {                \
                 LIST_INSERT_AFTER(_tmp,elm,field);        \
                 break;                                    \
              }                                            \
           }                                               \
        }                                                  \
} while (0)
#endif

#ifndef ARRAYSIZEOF
#define ARRAYSIZEOF(a) (sizeof(a) / sizeof(*a))
#endif

#ifndef BITFIELD_TEST
#define BITFIELD_TEST(f,b) ((f)[(b)/8] & 1<<((b)&7))
#endif

#ifndef BITFIELD_SET
#define BITFIELD_SET(f,b) ((f)[(b)/8] |= 1<<((b)&7))
#endif

#ifndef MMAX
#define MMAX(a,b) (a) > (b) ? (a) : (b)
#endif

#ifndef MMIN
#define MMIN(a,b) (a) < (b) ? (a) : (b)
#endif


int fa_unique_array(void *base, int nmemb, size_t size);
void *fa_libcmem_create(char *name, size_t size);
void *fa_libcmem_alloc(void *pool);
void fa_libcmem_free(void *pool, void *ptr);
