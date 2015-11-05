//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

#ifndef __FA_H__
#define __FA_H__

#include <sys/queue.h>
#include <inttypes.h>

#define FA_SYMBOL_E -1 // epsilon
typedef int32_t fa_symbol_t;

typedef STAILQ_HEAD(fa_state_sqhead_s, fa_state_s) fa_state_sqhead_t;
typedef TAILQ_HEAD(fa_state_tqhead_s, fa_state_s) fa_state_tqhead_t;
typedef LIST_HEAD(fa_state_head_s, fa_state_s) fa_state_head_t;
typedef LIST_HEAD(fa_trans_head_s, fa_trans_s) fa_trans_head_t;

// determinize opaque priority callback, return prioritized opaque value
typedef void *(fa_state_pri_f)(void **opaques, int opaques_n);

// minimize distinguish state callback, 0 same 1 distinguish
typedef int (fa_state_cmp_f)(void *a, void *b);

// used by fa_foreach_accepting
typedef void (fa_foreach_accepting_f)(void *opaque);

typedef void *(fa_mempool_create_f)(char *name, size_t size);
typedef void *(fa_mempool_alloc_f)(void *pool);
typedef void (fa_mempool_free_f)(void *pool, void *ptr);

// memory allocation callbacks
// fa_init defaults them to malloc/free wrappers
extern fa_mempool_create_f *fa_mempool_create;
extern fa_mempool_alloc_f *fa_mempool_alloc;
extern fa_mempool_free_f *fa_mempool_free;


typedef struct fa_state_s {
  LIST_ENTRY(fa_state_s) link; // fa_s.states list
  TAILQ_ENTRY(fa_state_s) tempq; // fa_determinize, fa_minimize groups
  STAILQ_ENTRY(fa_state_s) tempsq; // fa_eclosure, fa_remove_unreachable

  struct fa_s *fa;
#define FA_STATE_F_ACCEPTING (1 << 0)
#define FA_STATE_F_MARKED    (1 << 1) // fa_remove_unreachable
  uint32_t flags;
  fa_trans_head_t trans;
  void *opaque_temp; // used internally for various temp extra state info
  void *opaque; // user opaque
} fa_state_t;

typedef struct fa_trans_s {
  LIST_ENTRY(fa_trans_s) link; // fa_state_s.trans list

  fa_symbol_t symfrom;
  fa_symbol_t symto;
  fa_state_t *state;
  fa_state_t *src;
} fa_trans_t;


typedef struct fa_s {
  fa_state_t *start;
  fa_state_head_t states;
  int states_n;
  int trans_n;
} fa_t;

typedef struct fa_limit_s {
  int states;
  int trans;
} fa_limit_t;


void fa_init(void);
fa_t *fa_create(void);
void fa_destroy(fa_t *fa);
fa_t *fa_clone(fa_t *fa);
void fa_move(fa_t *fa, fa_t *src);
fa_state_t *fa_state_create(fa_t *fa);
void fa_state_destroy(fa_state_t *fs);
void fa_set_accepting_opaque(fa_t *fa, void *opaque);
void fa_foreach_accepting(fa_t *fa, fa_foreach_accepting_f cb);
int fa_count_symtrans(fa_t *fa);
fa_trans_t *fa_trans_create(fa_state_t *fs, fa_symbol_t symbol,
                            fa_state_t *dest);
void fa_trans_destroy(fa_trans_t *ft);

// reuses input fa:s, no need to free them
fa_t *fa_string_ex(uint8_t *str, int len, int icase);
fa_t *fa_string(uint8_t *str, int len);
fa_t *fa_union_list(fa_t **fa, int n);
fa_t *fa_union(fa_t *a, fa_t *b);
fa_t *fa_concat_list(fa_t **fa, int n);
fa_t *fa_concat(fa_t *a, fa_t *b);
fa_t *fa_repeat(fa_t *fa, int min, int max);
fa_t *fa_kstar(fa_t *fa);
fa_t *fa_remove_accepting_trans(fa_t *fa);
fa_t *fa_remove_unreachable(fa_t *fa);

// returns new fa, input fa need to be freed
fa_t *fa_determinize(fa_t *fa);
fa_t *fa_determinize_ex(fa_t *fa, fa_state_pri_f pri_cb,
                        fa_limit_t *limit, int *timeout);
fa_t *fa_minimize(fa_t *fa);
fa_t *fa_minimize_ex(fa_t *fa, fa_state_cmp_f cmp_cb, int *timeout);

#endif
