//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

#include <stdlib.h>
#include <sys/queue.h>
#include <ctype.h>
#include <assert.h>

#include "fa.h"
#include "fa_misc.h"
#include "fa_state_set.h"
#include "fa_state_set_hash.h"
#include "fa_state_group.h"

static fa_trans_t *fa_trans_create_range(fa_state_t *fs,
                                         fa_symbol_t symfrom,
                                         fa_symbol_t symto,
                                         fa_state_t *dest);

static void *fa_t_pool;
static void *fa_state_t_pool;
static void *fa_trans_t_pool;

fa_mempool_create_f *fa_mempool_create;
fa_mempool_alloc_f *fa_mempool_alloc;
fa_mempool_free_f *fa_mempool_free;


void fa_init(void) {
  if (!fa_mempool_create)
    fa_mempool_create = fa_libcmem_create;
  if (!fa_mempool_alloc)
    fa_mempool_alloc = fa_libcmem_alloc;
  if (!fa_mempool_free)
    fa_mempool_free = fa_libcmem_free;

  fa_t_pool = fa_mempool_create("fa_t", sizeof(fa_t));
  fa_state_t_pool = fa_mempool_create("fa_state_t", sizeof(fa_state_t));
  fa_trans_t_pool = fa_mempool_create("fa_trans_t", sizeof(fa_trans_t));

  fa_state_set_init();
  fa_state_group_init();
}

fa_t *fa_create(void) {
  fa_t *fa = fa_mempool_alloc(fa_t_pool);

  fa->start = NULL;
  fa->states_n = 0;
  LIST_INIT(&fa->states);
  fa->trans_n = 0;

  return fa;
}

void fa_destroy(fa_t *fa) {
  while (!LIST_EMPTY(&fa->states))
    fa_state_destroy(LIST_FIRST(&fa->states));

  fa_mempool_free(fa_t_pool, fa);
}

fa_t *fa_clone(fa_t *fa) {
  fa_t *cfa;
  fa_state_t *fs;
  fa_trans_t *ft;

  cfa = fa_create();

  LIST_FOREACH(fs, &fa->states, link)
    fs->opaque_temp = fa_state_create(cfa);

  LIST_FOREACH(fs, &fa->states, link) {
    fa_state_t *fsn = fs->opaque_temp;

    LIST_FOREACH(ft, &fs->trans, link)
      fa_trans_create_range(fsn, ft->symfrom, ft->symto,
                            ft->state->opaque_temp);

    fsn->flags = fs->flags;
  }

  cfa->start = fa->start->opaque_temp;

  return cfa;
}

void fa_move(fa_t *fa, fa_t *src) {
  fa_state_t *fs;

  while (!LIST_EMPTY(&src->states)) {
    fs = LIST_FIRST(&src->states);
    LIST_REMOVE(fs, link);
    LIST_INSERT_HEAD(&fa->states, fs, link);
    fs->fa = fa;
  }

  fa->states_n += src->states_n;
  fa->trans_n += src->trans_n;
  src->states_n = 0;
  src->trans_n = 0;
}

fa_state_t *fa_state_create(fa_t *fa) {
  fa_state_t *fs = fa_mempool_alloc(fa_state_t_pool);

  fs->fa = fa;
  fs->flags = 0;
  LIST_INIT(&fs->trans);
  fs->opaque = NULL;
  fs->opaque_temp = NULL;

  LIST_INSERT_HEAD(&fa->states, fs, link);
  fa->states_n++;

  return fs;
}

void fa_state_destroy(fa_state_t *fs) {
  while (!LIST_EMPTY(&fs->trans))
    fa_trans_destroy(LIST_FIRST(&fs->trans));

  fs->fa->states_n--;
  LIST_REMOVE(fs, link);

  fa_mempool_free(fa_state_t_pool, fs);
}

void fa_set_accepting_opaque(fa_t *fa, void *opaque) {
  fa_state_t *fs;

  LIST_FOREACH(fs, &fa->states, link)
    if (fs->flags & FA_STATE_F_ACCEPTING)
      fs->opaque = opaque;
}

void fa_foreach_accepting(fa_t *fa, fa_foreach_accepting_f cb) {
  fa_state_t *fs;

  LIST_FOREACH(fs, &fa->states, link)
    if (fs->flags & FA_STATE_F_ACCEPTING)
      cb(fs->opaque);
}

int fa_count_symtrans(fa_t *fa) {
  fa_state_t *fs;
  fa_trans_t *ft;
  int n;

  n = 0;
  LIST_FOREACH(fs, &fa->states, link)
    LIST_FOREACH(ft, &fs->trans, link)
      n += ft->symto - ft->symfrom + 1;

  return n;
}

static int fa_trans_cmp(fa_trans_t *a, fa_trans_t *b) {
  return a->symfrom - b->symfrom;
}

static fa_trans_t *fa_trans_create_ex(fa_state_t *fs,
                                      fa_symbol_t symfrom,
                                      fa_symbol_t symto,
                                      fa_state_t *dest) {
  fa_trans_t *ft;

  ft = fa_mempool_alloc(fa_trans_t_pool);
  ft->src = fs;
  ft->symfrom = symfrom;
  ft->symto = symto;
  ft->state = dest;

  ft->src->fa->trans_n++;

  return ft;
}

// used by fa_clone
static fa_trans_t *fa_trans_create_range(fa_state_t *fs,
                                         fa_symbol_t symfrom,
                                         fa_symbol_t symto,
                                         fa_state_t *dest) {
  fa_trans_t *ft;

  ft = fa_trans_create_ex(fs, symfrom, symto, dest);
  LIST_INSERT_SORTED(&fs->trans, ft, link, fa_trans_cmp);

  return ft;
}

fa_trans_t *fa_trans_create(fa_state_t *fs,
                            fa_symbol_t symbol,
                            fa_state_t *dest) {
  fa_trans_t *ft;

  // don't include epsilon transition in ranges
  if (symbol == FA_SYMBOL_E) {
    LIST_FOREACH(ft, &fs->trans, link) {
      if (ft->symfrom != FA_SYMBOL_E)
        continue;

      if (ft->state == dest)
        return ft;
    }
  } else {
    LIST_FOREACH(ft, &fs->trans, link) {
      if (ft->state != dest)
        continue;

      // one above range
      if (symbol - ft->symto == 1) {
        fa_trans_t *mft;
        ft->symto++;

        // find next range and maybe merge them
        for (mft = LIST_NEXT(ft, link); mft; mft = LIST_NEXT(mft, link)) {
          if (mft->state != dest)
            continue;

          if (mft->symfrom - ft->symto != 1)
            break;

          ft->symto = mft->symto;
          fa_trans_destroy(mft);
          break;
        }

        return ft;
      }

      // one below range
      if (ft->symfrom - symbol == 1) {
        ft->symfrom--;
        return ft;
      }

      // already included
      if (ft->symfrom <= symbol && ft->symto >= symbol)
        return ft;
    }
  }

  ft = fa_trans_create_ex(fs, symbol, symbol, dest);
  LIST_INSERT_SORTED(&fs->trans, ft, link, fa_trans_cmp);

  return ft;
}

void fa_trans_destroy(fa_trans_t *ft) {
  LIST_REMOVE(ft, link);
  ft->src->fa->trans_n--;
  fa_mempool_free(fa_trans_t_pool, ft);
}

// convert string to FA
//
// "abc" becomes the FA:
// ->()-a->()-b->()-c->(o)
//
fa_t *fa_string(uint8_t *str, int len) {
  return fa_string_ex(str, len, 0);
}

fa_t *fa_string_ex(uint8_t *str, int len, int icase) {
  fa_t *fa = fa_create();
  fa_state_t *prev;
  int i;

  fa->start = fa_state_create(fa);

  prev = fa->start;
  for (i = 0; i < len; i++) {
    fa_state_t *cur;

    cur = fa_state_create(fa);
    fa_trans_create(prev, (int)str[i], cur);
    if (icase && isalpha(str[i])) {
      if (isupper(str[i]))
        fa_trans_create(prev, (int)tolower(str[i]), cur);
      else
        fa_trans_create(prev, (int)toupper(str[i]), cur);
    }
    prev = cur;
  }

  prev->flags |= FA_STATE_F_ACCEPTING;

  return fa;
}

static int fa_trans_has_only_symbol(fa_state_t *fa, fa_symbol_t symbol) {
  fa_trans_t *ft;

  LIST_FOREACH(ft, &fa->trans, link)
    if (ft->symto != symbol || ft->symfrom != symbol)
      return 0;

  return 1;
}

// build the union FA from list of FAs
//
// ->..1..             /->..1..
// ->..2..  becomes: ->()->..2..
// ->..n..             \->..n..
//
fa_t *fa_union_list(fa_t **fa, int n) {
  fa_t *ufa = fa_create();
  int i;

  assert(n > 0);

  // reuses a start state from one of the union fa:s if it only has
  // epsilon transitions
  for (i = 0; i < n; i++) {
    if (!fa_trans_has_only_symbol(fa[i]->start, FA_SYMBOL_E))
      continue;

    ufa->start = fa[i]->start;
    break;
  }

  if (!ufa->start)
    ufa->start = fa_state_create(ufa);

  for (i = 0; i < n; i++) {
    // don't add transition if we reused start state
    if (ufa->start != fa[i]->start)
      fa_trans_create(ufa->start, FA_SYMBOL_E, fa[i]->start);
    fa_move(ufa, fa[i]);
    fa_destroy(fa[i]);
  }

  return ufa;
}

fa_t *fa_union(fa_t *a, fa_t *b) {
  return fa_union_list((fa_t *[]){a, b}, 2);
}

// build the concatenation FA from list of FAs
//
// ->..1..
// ->..2..  becomes: ->..1..->()->..2..->()->..n..->(o)
// ->..b..
//
fa_t *fa_concat_list(fa_t **fa, int n) {
  fa_t *cfa;
  int i;

  assert(n > 0);

  if (n == 1)
    return fa[0];

  cfa = fa_create();
  cfa->start = fa[0]->start;

  for (i = 0; i < n - 1; i++) {
    fa_state_t *fs;

    LIST_FOREACH(fs, &fa[i]->states, link) {
      if (!(fs->flags & FA_STATE_F_ACCEPTING))
        continue;

      fa_trans_create(fs, FA_SYMBOL_E, fa[i+1]->start);
      fs->flags &= ~FA_STATE_F_ACCEPTING;
    }

    fa_move(cfa, fa[i]);
    fa_destroy(fa[i]);
  }

  fa_move(cfa, fa[n - 1]);
  fa_destroy(fa[n - 1]);

  return cfa;
}

fa_t *fa_concat(fa_t *a, fa_t *b) {
  return fa_concat_list((fa_t *[]){a, b}, 2);
}

// repeated match of fa, match fa at least min times, but at most
// max times. if max is 0, match at least min or more times
//
// a*, (0, 0), zero or more
// a+, (1, 0), one or more
// a?, (0, 1), zero or one
//
fa_t *fa_repeat(fa_t *fa, int min, int max) {
  fa_t **fal;
  fa_t *famin, *famax;
  int i;
  int diff;

  diff = max - min;
  famin = NULL;

  if (min > 0) {
    fal = malloc(sizeof(fal[0]) * min);
    for (i = 0; i < min; i++)
      fal[i] = fa_clone(fa);
    famin = fa_concat_list(fal, min);
    free(fal);

    if (max != 0 && diff == 0) {
      fa_destroy(fa);
      return famin;
    }
  }

  if (max == 0) {
    famax = fa_kstar(fa);
  } else {
    fal = malloc(sizeof(fal[0]) * (diff + 1));
    fal[diff] = fa_create();
    fal[diff]->start = fa_state_create(fal[diff]);
    fal[diff]->start->flags |= FA_STATE_F_ACCEPTING;
    for (i = 0; i < diff; i++) {
      fal[i] = fa_clone(fa);
      fa_trans_create(fal[i]->start, FA_SYMBOL_E, fal[diff]->start);
    }
    famax = fa_concat_list(fal, diff + 1);
    free(fal);
    fa_destroy(fa);
  }

  if (famin)
    return fa_concat(famin, famax);
  else
    return famax;
}

// build kleene star FA, match zero or more times
fa_t *fa_kstar(fa_t *fa) {
  fa_state_t *fs;

  // match zero times
  fa->start->flags |= FA_STATE_F_ACCEPTING;

  LIST_FOREACH(fs, &fa->states, link) {
    if (fs == fa->start || !(fs->flags & FA_STATE_F_ACCEPTING))
      continue;

    fa_trans_create(fs, FA_SYMBOL_E, fa->start);
  }

  return fa;
}

// breadth-first traversal
fa_t *fa_remove_unreachable(fa_t *fa) {
  fa_state_sqhead_t unmarked = STAILQ_HEAD_INITIALIZER(unmarked);
  fa_state_t *fs, *next;

  STAILQ_INSERT_TAIL(&unmarked, fa->start, tempsq);
  fa->start->flags |= FA_STATE_F_MARKED;

  // visit all nodes and mark them
  while (!STAILQ_EMPTY(&unmarked)) {
    fa_trans_t *ft;

    fs = STAILQ_FIRST(&unmarked);
    STAILQ_REMOVE_HEAD(&unmarked, tempsq);

    LIST_FOREACH(ft, &fs->trans, link) {
      if (ft->state->flags & FA_STATE_F_MARKED)
        continue;

      ft->state->flags |= FA_STATE_F_MARKED;
      STAILQ_INSERT_TAIL(&unmarked, ft->state, tempsq);
    }
  }

  // unmark nodes and remove nodes that was not marked
  for (fs = LIST_FIRST(&fa->states); fs; fs = next) {
    next = LIST_NEXT(fs, link);

    if (!(fs->flags & FA_STATE_F_MARKED))
      fa_state_destroy(fs);
    else
      fs->flags &= ~FA_STATE_F_MARKED;
  }

  return fa;
}

// remove outgoing transitions for all accepting states
fa_t *fa_remove_accepting_trans(fa_t *fa) {
  fa_state_t *fs;

  LIST_FOREACH(fs, &fa->states, link) {
    if (!(fs->flags & FA_STATE_F_ACCEPTING))
      continue;

    while (!LIST_EMPTY(&fs->trans))
      fa_trans_destroy(LIST_FIRST(&fs->trans));
  }

  return fa_remove_unreachable(fa);
}

// set of states reachable from set using given symbol
static fa_state_set_t *fa_reachable(fa_state_set_t *start, fa_symbol_t symbol) {
  fa_state_set_t *reachable = fa_state_set_create();
  fa_trans_t *ft;
  int i;

  for (i = 0; i < start->states_n; i++) {
    LIST_FOREACH(ft, &start->states[i]->trans, link) {
      if (ft->symfrom > symbol || ft->symto < symbol)
        continue;

      fa_state_set_add(reachable, ft->state);
    }
  }

  // no sort, in fa_determinize_ex fa_eclosure always used on resulting set

  return reachable;
}

// set of states reachable from set using epsilon transitions
static fa_state_set_t *fa_eclosure(fa_state_set_t *start) {
  fa_state_set_t *reachable = fa_state_set_create();
  fa_trans_t *ft;
  fa_state_sqhead_t stack = STAILQ_HEAD_INITIALIZER(stack);
  int i;

  for (i = 0; i < start->states_n; i++) {
    STAILQ_INSERT_HEAD(&stack, start->states[i], tempsq);
    fa_state_set_add(reachable, start->states[i]);
  }

  while (!STAILQ_EMPTY(&stack)) {
    fa_state_t *s = STAILQ_FIRST(&stack);

    STAILQ_REMOVE_HEAD(&stack, tempsq);

    LIST_FOREACH(ft, &s->trans, link)
      if (ft->symfrom == FA_SYMBOL_E &&
         fa_state_set_add(reachable, ft->state))
        STAILQ_INSERT_HEAD(&stack, ft->state, tempsq);
  }

  // fa_state_set_cmp/fa_state_set_hash_find relies on sorted states
  fa_state_set_sort(reachable);

  return reachable;
}

#define FA_DETERMINIZE_HASH_SIZE 199

// determinize fa using power set construction algorithm
//
// keeps a stack of state sets, pop a set of the stack and and for each symbol
// in set determine what set of states can be reached using the symbol and a
// eclosure. create set if it has not been seen before and creates a transition
// to it
//
// if given pri_cb use it assign opaque in new dfa
//
// stores state set in state opaque_temp
//
fa_t *fa_determinize(fa_t *fa) {
  return fa_determinize_ex(fa, NULL, NULL, NULL);
}

fa_t *fa_determinize_ex(fa_t *fa, fa_state_pri_f pri_cb,
                        fa_limit_t *limit, int *timeout) {
  fa_t *dfa;
  fa_state_set_t *eclosure, *start;
  fa_state_tqhead_t unmarked = TAILQ_HEAD_INITIALIZER(unmarked);
  fa_state_t *fs;
  fa_state_set_hash_t *fssh;
  int i;
  int cancel;

  cancel = 0;
  fssh = fa_state_set_hash_create(FA_DETERMINIZE_HASH_SIZE);
  dfa = fa_create();

  // build initial set of states reachable with epsilon transition
  // from start state
  start = fa_state_set_create();
  fa_state_set_add(start, fa->start);
  eclosure = fa_eclosure(start);
  fa_state_set_destroy(start);
  dfa->start = fa_state_create(dfa);
  dfa->start->opaque_temp = eclosure;
  TAILQ_INSERT_TAIL(&unmarked, dfa->start, tempq);

  while (!cancel && !TAILQ_EMPTY(&unmarked)) {
    fa_state_t *t = TAILQ_FIRST(&unmarked);
    fa_state_set_t *ts = t->opaque_temp;

    TAILQ_REMOVE(&unmarked, t, tempq);

    fa_state_set_syms(ts);

    for (i = 0; i < 256; i++) {
      fa_state_t *u;
      fa_state_set_t *reachable;

      if (!BITFIELD_TEST(ts->syms->map, i))
        continue;

      // build set of states reachable with symbol followed by epsilon
      // transition from current state set
      reachable = fa_reachable(ts, i);
      eclosure = fa_eclosure(reachable);
      fa_state_set_destroy(reachable);

      // state for state set already exist?
      u = fa_state_set_hash_find(fssh, eclosure);
      if (u) {
        fa_state_set_destroy(eclosure);
      } else {
        u = fa_state_create(dfa);
        fa_state_set_hash_add(fssh, eclosure, u);
        u->opaque_temp = eclosure;
        TAILQ_INSERT_TAIL(&unmarked, u, tempq);
      }

      fa_trans_create(t, i, u);
    }

    if ((timeout && *timeout) ||
       (limit && (fa->states_n > limit->states ||
                  fa->trans_n > limit->trans)))
      cancel = 1;
  }

  // if we have a state priority callback, use it to get opaque to use
  if (!cancel && pri_cb)
    LIST_FOREACH(fs, &dfa->states, link) {
      fa_state_set_t *s = fs->opaque_temp;
      int n, one;

      if (!(s->flags & FA_STATE_F_ACCEPTING))
        continue;

      n = 0;
      one = 1;
      for (i = 0; i < s->states_n; i++) {
        if (s->states[i]->flags & FA_STATE_F_ACCEPTING) {
          // take first if not set, will be used if no others are found
          if (n == 0)
            fs->opaque = s->states[i]->opaque;

          // found more than one unique opaque
          if (fs->opaque != s->states[i]->opaque)
            one = 0;

          n++;
        }
      }

      if (one) {
        // fs->opaque already set above
      } else {
        // more than one unique opaque, ask callback
        void **opaques;

        opaques = malloc(sizeof(opaques[0]) * n);
        n = 0;
        for (i = 0; i < s->states_n; i++)
          if (s->states[i]->flags & FA_STATE_F_ACCEPTING)
            opaques[n++] = s->states[i]->opaque;

        n = fa_unique_array(opaques, n, sizeof(opaques[0]));
        fs->opaque = pri_cb(opaques, n);
        free(opaques);
      }
    }

  // cleanup and set accepting
  LIST_FOREACH(fs, &dfa->states, link) {
    fa_state_set_t *s = fs->opaque_temp;

    // some original state in set was accepting
    if (s->flags & FA_STATE_F_ACCEPTING)
      fs->flags |= FA_STATE_F_ACCEPTING;

    fa_state_set_destroy(s);
  }

  fa_state_set_hash_destroy(fssh);

  if (cancel) {
    fa_destroy(dfa);
    dfa = NULL;
  }

  return dfa;
}

// distinguishable if diff on accepting/non-accepting or not all transition
// goes to same state group or cmp_cb says so
static int fa_minimize_distinguishable_state(fa_state_t *a, fa_state_t *b,
                                             fa_state_cmp_f cmp_cb) {
  fa_trans_t *fta;
  fa_trans_t *ftb;
  fa_trans_t tempa = {{0}};
  fa_trans_t tempb = {{0}};

  // one is accepting
  if ((a->flags & FA_STATE_F_ACCEPTING) != (b->flags & FA_STATE_F_ACCEPTING))
    return 1;

  fta = LIST_FIRST(&a->trans);
  if (fta)
    tempa = *fta;
  ftb = LIST_FIRST(&b->trans);
  if (ftb)
    tempb = *ftb;

  while (fta && ftb) {
    // range difference
    if (tempa.symfrom != tempb.symfrom)
      return 1;

    // goes to different state group
    if (fta->state->opaque_temp != ftb->state->opaque_temp)
      return 1;

    // go to next sym range for one or both
    if (tempa.symto < tempb.symto) {
      tempb.symfrom = tempa.symto + 1;
      fta = LIST_NEXT(fta, link);
      if (fta)
        tempa = *fta;
    } else if (tempa.symto > tempb.symto) {
      tempa.symfrom = tempb.symto + 1;
      ftb = LIST_NEXT(ftb, link);
      if (ftb)
        tempb = *ftb;
    } else {
      fta = LIST_NEXT(fta, link);
      if (fta)
        tempa = *fta;
      ftb = LIST_NEXT(ftb, link);
      if (ftb)
        tempb = *ftb;
    }
  }

  // one has transitions left
  if (fta || ftb)
    return 1;

  // callback say they are different
  if (cmp_cb && cmp_cb(a->opaque, b->opaque))
    return 1;

  return 0;
}

// check if group includes equal states. if not, move all states
// distinguishable from the first state into a new group
static int fa_minimize_distinguish_group(fa_state_group_t *group,
                                         fa_state_group_t **new,
                                         fa_state_cmp_f cmp_cb) {
  int diff;
  fa_state_t *first, *fs, *next;

  first = TAILQ_FIRST(&group->states);

  // less than two states, not distinguishable
  if (!first || !TAILQ_NEXT(first, tempq))
    return 0;

  diff = 0;
  for (fs = TAILQ_NEXT(first, tempq); fs; fs = TAILQ_NEXT(fs, tempq))
    if (fa_minimize_distinguishable_state(first, fs, cmp_cb)) {
      diff = 1;
      break;
    }

  if (!diff)
    return 0;

  *new = fa_state_group_create();

  for (; fs; fs = next) {
    next = TAILQ_NEXT(fs, tempq);
    if (fa_minimize_distinguishable_state(first, fs, cmp_cb))
      fa_state_group_change(group, *new, fs);
  }

  // put states into new state group
  TAILQ_FOREACH(fs, &(*new)->states, tempq)
    fs->opaque_temp = *new;

  return 1;
}

// minimize dfa using hopcrofts algorithm
//
// iterating over a list of group of states and split group if
// distinguishable states are found
//
// when distinguishing, opaque_temp is used to store which group
// a state belong to
//
// when building the new dfa, opaque_temp is used to store which
// new group state the state belong to
//
// if given cmp_cb use it for indistinguishable to force them to be
// seen as distinguishable
//
fa_t *fa_minimize(fa_t *fa) {
  return fa_minimize_ex(fa, NULL, NULL);
}

fa_t *fa_minimize_ex(fa_t *fa, fa_state_cmp_f cmp_cb, int *timeout) {
  fa_state_group_head_t partition = LIST_HEAD_INITIALIZER(partition);
  fa_state_group_t *group, *next;
  fa_state_t *fs;
  fa_t *mdfa;
  int i;
  int change;
  int cancel;

  cancel = 0;

  // create initial group with all states
  group = fa_state_group_create();
  LIST_FOREACH(fs, &fa->states, link) {
    fa_state_group_assign(group, fs);
    // state belongs to the initial group
    fs->opaque_temp = group;
  }
  LIST_INSERT_HEAD(&partition, group, link);

  // while one or more groups are distinguishable
  change = 1;
  while (!cancel && change) {
    change = 0;

    // uses safe iteration, list is changed while iterating
    for (group = LIST_FIRST(&partition); group; group = next) {
      fa_state_group_t *new;
      next = LIST_NEXT(group, link);

      if (!fa_minimize_distinguish_group(group, &new, cmp_cb))
        continue;

      // group was splitted, insert new group
      LIST_INSERT_AFTER(group, new, link);
      change = 1;
    }

    if (timeout && *timeout)
      cancel = 1;
  }

  if (cancel) {
    mdfa = NULL;
  } else {
    mdfa = fa_create();

    // create a new state for each group
    LIST_FOREACH(group, &partition, link) {
      fa_state_t *mfs;

      mfs = fa_state_create(mdfa);
      TAILQ_FOREACH(fs, &group->states, tempq)
        fs->opaque_temp = mfs;
    }

    // create transitions for each group state, all states are
    // equal in each group
    LIST_FOREACH(group, &partition, link) {
      fa_state_t *c;
      fa_trans_t *ft;

      // use first state as candidate
      c = TAILQ_FIRST(&group->states);
      fs = c->opaque_temp;

      // create transitions between groups
      LIST_FOREACH(ft, &c->trans, link)
        for (i = ft->symfrom; i <= ft->symto; i++)
          fa_trans_create(fs, i, ft->state->opaque_temp);

      // group has original start state
      if (fa_state_group_has_state(group, fa->start))
        mdfa->start = fs;

      // candidate is accepting
      if (c->flags & FA_STATE_F_ACCEPTING)
        fs->flags |= FA_STATE_F_ACCEPTING;

      fs->opaque = c->opaque;
    }
  }

  // cleanup
  for (group = LIST_FIRST(&partition); group; group = next) {
    next = LIST_NEXT(group, link);
    fa_state_group_destroy(group);
  }

  return mdfa;
}
