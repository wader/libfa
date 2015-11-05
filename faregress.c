//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <getopt.h>
#include <sys/queue.h>
#include <dirent.h>
#include <signal.h>
#include <sys/time.h>
#include <ctype.h>

#include <pcre.h>

#include "fa.h"
#include "fa_regexp.h"
#include "fa_sim.h"
#include "fa_sim_bitcomp.h"


#define TEST_ERROR -1
#define TEST_REJECT -2
#define TEST_MORE -3

int tests;
int test_cases;
int test_cases_fail;
int sigalarm_trigger;
int simbitcomp_total_bytes = 0;
int sim_total_bytes = 0;

typedef struct test_case_s {
  LIST_ENTRY(test_case_s) link;

  int num;
  int len;
  char *text;
  int line;
} test_case_t;

typedef LIST_HEAD(test_case_head_s, test_case_s) test_case_head_t;

typedef struct test_regexp_s {
  LIST_ENTRY(test_regexp_s) link;

  int num;
  char *regexp;
  int line;
} test_regexp_t;

typedef LIST_HEAD(test_regexp_head_s, test_regexp_s) test_regexp_head_t;

typedef struct test_opt_s {
  LIST_ENTRY(test_opt_s) link;

  char *name;
  char *value;
} test_opt_t;

typedef LIST_HEAD(test_opt_head_s, test_opt_s) test_opt_head_t;

typedef struct test_s {
  LIST_ENTRY(test_s) link;

  char *file;
  int line;
  test_regexp_head_t regexps;
  test_case_head_t cases;
  test_opt_head_t opts;
} test_t;

typedef LIST_HEAD(test_head_s, test_s) test_head_t;


static void sigalarm_handler(int signal) {
  sigalarm_trigger = 1;
}

static test_case_t *test_case_create(int num, int len, char *text,
                                     int line) {
  test_case_t *tc = malloc(sizeof(*tc));

  tc->num = num;
  tc->len = len;
  tc->text = malloc(len);
  memcpy(tc->text, text, len);
  tc->line = line;

  return tc;
}

static void test_case_destroy(test_case_t *tc) {
  LIST_REMOVE(tc, link);

  free(tc->text);
  free(tc);
}

static test_regexp_t *test_regexp_create(int num, char *regexp, int line) {
  test_regexp_t *tr = malloc(sizeof(*tr));

  tr->num = num;
  tr->regexp = strdup(regexp);
  tr->line = line;

  return tr;
}

static void test_regexp_destroy(test_regexp_t *tr) {
  LIST_REMOVE(tr, link);

  free(tr->regexp);
  free(tr);
}

static char *test_opt_get(test_t *t, char *name, char *def) {
  test_opt_t *o;

  LIST_FOREACH(o, &t->opts, link)
    if (strcmp(name, o->name) == 0)
      return o->value;

  return def;
}

static int test_opt_get_int(test_t *t, char *name, int def) {
  char *s;

  s = test_opt_get(t, name, NULL);
  if (s == NULL)
    return def;

  return atoi(s);
}

static test_opt_t *test_opt_create(char *name, char *value) {
  test_opt_t *t = malloc(sizeof(*t));

  t->name = strdup(name);
  t->value = strdup(value);

  return t;
}

static void test_opt_destroy(test_opt_t *t) {
  LIST_REMOVE(t, link);

  free(t->name);
  free(t->value);
  free(t);
}

static test_t *test_create(char *file, int line) {
  test_t *t  = malloc(sizeof(*t));

  t->file = strdup(file);
  t->line = line;
  LIST_INIT(&t->regexps);
  LIST_INIT(&t->cases);
  LIST_INIT(&t->opts);

  return t;
}

static void test_destroy(test_t *t) {
  while (!LIST_EMPTY(&t->regexps))
    test_regexp_destroy(LIST_FIRST(&t->regexps));

  while (!LIST_EMPTY(&t->cases))
    test_case_destroy(LIST_FIRST(&t->cases));

  while (!LIST_EMPTY(&t->opts))
    test_opt_destroy(LIST_FIRST(&t->opts));

  free(t->file);
  free(t);
}

static void *state_pri(void **opaques, int opaques_n) {
  test_regexp_t **l = (test_regexp_t**)opaques;
  test_regexp_t *w;
  int i;

  w = l[0];
  for (i = 1; i < opaques_n; i++)
    if (l[i]->num < w->num)
      w = l[i];

  return w;
}

static int state_cmp(void *a, void *b) {
  return a != b;
}

static int test_pcre_callout(pcre_callout_block *b) {
  int *num = b->callout_data;

  *num = b->callout_number;
  return 0;
}

static void dummy(void *opaque) {
}

static void test_do(test_t *t) {
  test_case_t *tc;
  test_regexp_t *tr;
  fa_t **fal;
  fa_t *fa, *tfa;
  fa_sim_t *sim;
  fa_sim_bitcomp_t *simbitcomp;
  char *errstr = NULL;
  int errpos;
  int i;
  fa_limit_t limit;
  fa_limit_t *plimit = NULL;
  pcre *pcre_comp = NULL;
  pcre_extra pcre_extra;
  int pcre_match_num;
  int pcre_len;
  char *pcre_s;

  struct itimerval itv;

  if (test_opt_get(t, "trans", NULL) ||
     test_opt_get(t, "states", NULL)) {
    limit.states = test_opt_get_int(t, "states", 0);
    limit.trans = test_opt_get_int(t, "trans", 0);
    plimit = &limit;
  }

  i = 0;
  pcre_len = 0;
  LIST_FOREACH(tr, &t->regexps, link) {
    i++;
    pcre_len += strlen(tr->regexp) + strlen("()(?C123)|");
  }

  fal = malloc(sizeof(fal[0]) * i);
  pcre_s = malloc(pcre_len + 1);
  pcre_s[0] = '\0';

  i = 0;
  LIST_FOREACH(tr, &t->regexps, link) {
    char b[256];

    fa_regexp_class_dot_all = test_opt_get_int(t, "dotall", 0);

    fa = fa_regexp_fa(tr->regexp, &errstr, &errpos, plimit);
    if (errstr) {
      LIST_FOREACH(tc, &t->cases, link) {
        char *s = malloc(tc->len+1);
        strncpy(s, tc->text, tc->len);
        s[tc->len] = '\0';
        if (tc->num == TEST_ERROR &&
           strstr(errstr, s) != NULL) {
          free(s);
          break;
        }
        free(s);
      }

      if (!tc) {
        fprintf(stderr, "%s:%d:%d: %s\n",
                t->file, tr->line, errpos, errstr);
        test_cases_fail++;
      }

      free(fal);
      free(pcre_s);
      return;
    }

    tfa = fa_determinize(fa);
    fa_destroy(fa);
    fa = tfa;

    tfa = fa_minimize(fa);
    fa_destroy(fa);
    fa = tfa;

    fa_set_accepting_opaque(fa, tr);

    // for coverage
    fa_count_symtrans(fa);
    fa_foreach_accepting(fa, dummy);

    fal[i++] = fa;

    strcat(pcre_s, "(");
    strcat(pcre_s, tr->regexp);
    strcat(pcre_s, ")");
    snprintf(b, sizeof(b), "(?C%d)|", tr->num);
    strcat(pcre_s, b);
  }

  pcre_s[strlen(pcre_s)-1] = '\0';

  fa = fa_union_list(fal, i);
  free(fal);

  sigalarm_trigger = 0;
  itv.it_interval.tv_sec = test_opt_get_int(t, "dtimeout", 0) / 1000;
  itv.it_interval.tv_usec = test_opt_get_int(t, "dtimeout", 0) % 1000;
  itv.it_value = itv.it_interval;
  setitimer(ITIMER_REAL, &itv, NULL);
  tfa = fa_determinize_ex(fa, state_pri, plimit, &sigalarm_trigger);
  itv.it_value.tv_sec = 0;
  itv.it_value.tv_usec = 0;
  setitimer(ITIMER_REAL, &itv, NULL);
  fa_destroy(fa);
  fa = tfa;

  if (fa) {
    sigalarm_trigger = 0;
    itv.it_interval.tv_sec = test_opt_get_int(t, "mtimeout", 0) / 1000;
    itv.it_interval.tv_usec = test_opt_get_int(t, "mtimeout", 0) % 1000;
    itv.it_value = itv.it_interval;
    setitimer(ITIMER_REAL, &itv, NULL);
    tfa = fa_minimize_ex(fa, state_cmp, &sigalarm_trigger);
    itv.it_value.tv_sec = 0;
    itv.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &itv, NULL);
    fa_destroy(fa);
    fa = tfa;

    if (!fa)
      errstr = "mtimeout";
  } else {
    if (sigalarm_trigger)
      errstr = "dtimeout";
    else
      errstr = "determinization generated too many states or transitions";
  }

  if (errstr) {
    LIST_FOREACH(tc, &t->cases, link) {
      char *s = malloc(tc->len+1);
      strncpy(s, tc->text, tc->len);
      s[tc->len] = '\0';
      if (tc->num == TEST_ERROR &&
         strstr(errstr, s) != NULL) {
        free(s);
        break;
      }
      free(s);
    }

    if (!tc) {
      fprintf(stderr, "%s:%d: %s\n",
              t->file, t->line, errstr);
      test_cases_fail++;
    }

    free(pcre_s);

    return;
  }

  if (test_opt_get(t, "removeacceptingtrans", NULL)) {
    fa = fa_remove_accepting_trans(fa);
    tfa = fa_minimize_ex(fa, state_cmp, NULL);
    fa_destroy(fa);
    fa = tfa;
  }

  pcre_extra.flags = PCRE_EXTRA_CALLOUT_DATA;
  pcre_extra.callout_data = &pcre_match_num;
  pcre_callout = test_pcre_callout;

  pcre_comp = pcre_compile(pcre_s,
                           // try to be a bit more like us
                           0
                           | PCRE_DOTALL
                           | PCRE_DOLLAR_ENDONLY
                           | PCRE_NO_AUTO_CAPTURE
                           ,
                           (const char**)&errstr, &errpos, NULL);
  if (errstr && !test_opt_get_int(t, "ignorepcre", 0)) {
    fprintf(stderr, "PCRE: %s:%d: %s: %s\n", t->file, t->line, errstr, pcre_s);
  }
  free(pcre_s);
  errstr = NULL;

  sim = fa_sim_create(fa);
  sim_total_bytes += sim->size;
  fa_destroy(fa);
  simbitcomp = fa_sim_bitcomp_create(sim);
  simbitcomp_total_bytes += simbitcomp->size;

  LIST_FOREACH(tc, &t->cases, link) {
    fa_sim_run_t run;
    test_case_t *otc;
    int fail;
    int r;

    fail = 0;

    fa_sim_run_init(sim, &run);
    r = fa_sim_run(sim, &run, (uint8_t *)tc->text, tc->len);
    otc = (test_case_t*)run.opaque;
    if ((r == FA_SIM_RUN_ACCEPT && otc->num != tc->num) ||
        (r == FA_SIM_RUN_MORE && tc->num != TEST_MORE) ||
        (r == FA_SIM_RUN_REJECT && tc->num != TEST_REJECT)) {
      fprintf(stderr, "SIM       : %s:%d: %.*s: ",
              t->file, tc->line, tc->len, tc->text);

      if (r == FA_SIM_RUN_ACCEPT)
        fprintf(stderr, "matched %d", otc->num);
      else if (r == FA_SIM_RUN_MORE)
        fprintf(stderr, "needs more input");
      else if (r == FA_SIM_RUN_REJECT)
        fprintf(stderr, "no match");
      fprintf(stderr, ", should ");
      if (tc->num == TEST_REJECT)
        fprintf(stderr, "not match");
      else if (tc->num == TEST_MORE)
        fprintf(stderr, "need more input");
      else
        fprintf(stderr, "match %d", tc->num);
      fprintf(stderr, "\n");
      fail++;
    }

    fa_sim_bitcomp_run_init(simbitcomp, &run);
    r = fa_sim_bitcomp_run(simbitcomp, &run, (uint8_t *)tc->text, tc->len);
    otc = (test_case_t*)run.opaque;
    if ((r == FA_SIM_RUN_ACCEPT && otc->num != tc->num) ||
        (r == FA_SIM_RUN_MORE && tc->num != TEST_MORE) ||
        (r == FA_SIM_RUN_REJECT && tc->num != TEST_REJECT)) {
      fprintf(stderr, "SIMBITCOMP: %s:%d: %.*s: ",
              t->file, tc->line, tc->len, tc->text);

      if (r == FA_SIM_RUN_ACCEPT)
        fprintf(stderr, "matched %d", otc->num);
      else if (r == FA_SIM_RUN_MORE)
        fprintf(stderr, "needs more input");
      else if (r == FA_SIM_RUN_REJECT)
        fprintf(stderr, "no match");
      fprintf(stderr, ", should ");
      if (tc->num == TEST_REJECT)
        fprintf(stderr, "not match");
      else if (tc->num == TEST_MORE)
        fprintf(stderr, "need more input");
      else
        fprintf(stderr, "match %d", tc->num);
      fprintf(stderr, "\n");
      fail++;
    }

    if (!test_opt_get_int(t, "ignorepcre", 0) &&
       pcre_comp) {
      int r;
      int ovector[10];
      int wspace[10000];

      pcre_match_num = TEST_REJECT;

      r = pcre_dfa_exec(pcre_comp, &pcre_extra, tc->text, tc->len,
                        0, 0, ovector, 10, wspace, 10000);

      if (r == -1) {
        if (tc->num != TEST_REJECT && tc->num != TEST_MORE) {
          fprintf(stderr, "PCRE: %s:%d: %.*s: no match should match %d\n",
                  t->file, tc->line, tc->len, tc->text, tc->num);
          fail++;
        }
      } else {
        if (pcre_match_num != tc->num) {
          fprintf(stderr, "PCRE: %s:%d: %.*s: matched %d should ",
                  t->file, tc->line, tc->len, tc->text, pcre_match_num);
          if (tc->num == TEST_REJECT)
            fprintf(stderr, "not match\n");
          else
            fprintf(stderr, "match %d\n", tc->num);

          fail++;
        }
      }
    }

    if (fail)
      test_cases_fail++;
  }

  if (pcre_comp)
    pcre_free(pcre_comp);

  fa_sim_destroy(sim);
  fa_sim_bitcomp_destroy(simbitcomp);
}

static int hex(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  else if (c >= 'a' && c <= 'f')
    return 10 + c - 'a';
  else if (c >= 'A' && c <= 'F')
    return 10 + c - 'A';
  else
    return 0;
}

static void test_case_unescape(char *s, char **us, int *len) {
  int i, j, l;

  j = 0;
  l = strlen(s);
  if (l == 0) {
    *us = malloc(1);
    *len = 0;
    return;
  }

  *us = malloc(l);

  for (i = 0; i < l; i++, j++) {
    if (s[i] == '\\' && l-(i+1) > 0) {
      i++;
      if (s[i] == 'x' && l-(i+1) > 1) {
        (*us)[j] = (hex(s[i+1]) << 4) | hex(s[i+2]);
        i += 2;
      } else if (s[i] == 'r')
        (*us)[j] = '\r';
      else if (s[i] == 'n')
        (*us)[j] = '\n';
      else if (s[i] == 't')
        (*us)[j] = '\t';
      else if (s[i] == 'v')
        (*us)[j] = '\v';
      else if (s[i] == 'f')
        (*us)[j] = '\f';
      else if (s[i] == 'e')
        (*us)[j] = '\e';
      else if (s[i] == 'a')
        (*us)[j] = '\a';
      else if (s[i] == 'b')
        (*us)[j] = '\b';
      else if (s[i] == '0')
        (*us)[j] = '\0';
      else
        (*us)[j] = s[i];
    } else
      (*us)[j] = s[i];
  }

  *len = j;
}

static void test_file(char *file) {
  FILE *s;
  char buf[4096];
  test_t *t = NULL;
  int line = 0;

  s = fopen(file, "rb");
  if (s == NULL)
    return;

  while (fgets(buf, sizeof(buf), s)) {
    char *s = buf;
    char *d;
    test_regexp_t *tr;
    test_case_t *tc;

    line++;

    // empty or comment
    while (*s == ' ')
      s++;
    if (*s == '#')
      continue;

    // remove \n
    s[strlen(s) - 1] = '\0';

    if (*s == '\0') {
      if (t != NULL) {
        test_do(t);
        test_destroy(t);
        t = NULL;
      }

      continue;
    }

    d = strchr(s, ':');
    if (d == NULL) {
      test_opt_t *o;

      d = strchr(s, '=');
      if (d == NULL)
        continue;

      *d++ = '\0';

      if (t == NULL) {
        t = test_create(file, line);
        tests++;
      }

      o = test_opt_create(s, d);
      LIST_INSERT_HEAD(&t->opts, o, link);

      continue;
    }
    *d++ = '\0';

    if (t == NULL) {
      t = test_create(file, line);
      tests++;
    }

    if (s == buf) {
      // no indent
      tr = test_regexp_create(atoi(s), d, line);
      LIST_INSERT_HEAD(&t->regexps, tr, link);
    } else {
      // indent
      char *us;
      int len;
      int num;

      if (strcmp(s, "e") == 0)
        num = TEST_ERROR;
      else if (strcmp(s, "!") == 0)
        num = TEST_REJECT;
      else if (strcmp(s, "m") == 0)
        num = TEST_MORE;
      else
        num = atoi(s);

      test_case_unescape(d, &us, &len);
      tc = test_case_create(num, len, us, line);
      free(us);
      test_cases++;
      LIST_INSERT_HEAD(&t->cases, tc, link);
    }
  }

  if (t) {
    test_do(t);
    test_destroy(t);
  }

  fclose(s);
}

static void test_dir(char *dir) {
  DIR *d;
  struct dirent *de;

  d = opendir(dir);
  if (d == NULL)
    return;

  while ((de = readdir(d))) {
    char buf[4096];

    snprintf(buf, sizeof(buf), "%s/%s", dir, de->d_name);
    test_file(buf);
  }

  closedir(d);
}

static void dumpstring(char *s, int len) {
  int i;

  for (i = 0; i < len; i++)
    if (isgraph(s[i]))
      fprintf(stderr, "%c", s[i]);
    else
      fprintf(stderr, "\\x%.2x", (uint8_t)s[i]);
  fprintf(stderr, "\n");
}

static void randstring(char *s, int len) {
  int i;
  char *v = ".||||||||||||||||||||*+?((())))[[[]]]{{{---}}}";
  int l = strlen(v);

  /*for (i = 0; i < len; i++)
    s[i] = (random() % 255) + 1;*/

  for (i = 0; i < len; i++)
    switch (random() % 100) {
      case 0 ... 30:
        s[i] = v[random() % l];
        break;
      default:
        s[i] = (random() % 255) + 1;
        break;
    }

  if (random() % 3 == 0)
    s[0] = '^';
  if (random() % 3 == 0)
    s[len-2] = '$';


  s[len-1] = 0;
}

static void test_regexp_fuzz(void) {
  char *s;
  int i = 0;
  int len = 70;
  char *errstr;
  int errpos;
  fa_t *fa, *tfa;
  int parsed = 0;

  s = malloc(len);

  for (;;) {
    len = (random() % 67) + 2;
    randstring(s, len);
    if (0)
      dumpstring(s, len);
    fa = fa_regexp_fa(s, &errstr, &errpos, NULL);
    if (!errstr) {
      parsed++;

      tfa = fa_determinize(fa);
      fa_destroy(fa);
      fa = tfa;

      tfa = fa_minimize(fa);
      fa_destroy(fa);
      fa = tfa;

      fa_destroy(fa);
    }

    i++;

    if (i % 100 == 0)
      fprintf(stderr, "i=%d parsed=%d\n", i, parsed);
  }

  free(s);
}

static void test_misc(void) {
  fa_t *fa;

    fa_state_t *a, *b;
    fa_trans_t *t;

  fa = fa_create();
  a = fa_state_create(fa);
  b = fa_state_create(fa);

  fa_trans_create(a, 'a', b);
  fa_trans_create(a, 'b', b);
  fa_trans_create(a, 'c', b);
  t = LIST_FIRST(&a->trans);
  if (t->symfrom != 'a' || t->symto != 'c')
    fprintf(stderr, "%c-%c\n", t->symfrom, t->symto);
  fa_trans_destroy(t);

  fa_trans_create(a, 'a', b);
  fa_trans_create(a, 'c', b);
  fa_trans_create(a, 'b', b);
  t = LIST_FIRST(&a->trans);
  if (t->symfrom != 'a' || t->symto != 'c')
    fprintf(stderr, "%c-%c\n", t->symfrom, t->symto);
  fa_trans_destroy(t);

  fa_trans_create(a, 'b', b);
  fa_trans_create(a, 'a', b);
  fa_trans_create(a, 'c', b);
  t = LIST_FIRST(&a->trans);
  if (t->symfrom != 'a' || t->symto != 'c')
    fprintf(stderr, "%c-%c\n", t->symfrom, t->symto);
  fa_trans_destroy(t);

  fa_trans_create(a, 'b', b);
  fa_trans_create(a, 'c', b);
  fa_trans_create(a, 'a', b);
  t = LIST_FIRST(&a->trans);
  if (t->symfrom != 'a' || t->symto != 'c')
    fprintf(stderr, "%c-%c\n", t->symfrom, t->symto);
  fa_trans_destroy(t);

  fa_trans_create(a, 'c', b);
  fa_trans_create(a, 'a', b);
  fa_trans_create(a, 'b', b);
  t = LIST_FIRST(&a->trans);
  if (t->symfrom != 'a' || t->symto != 'c')
    fprintf(stderr, "%c-%c\n", t->symfrom, t->symto);
  fa_trans_destroy(t);

  fa_trans_create(a, 'a', b);
  fa_trans_create(a, 'b', b);
  fa_trans_create(a, 'c', b);
  fa_trans_create(a, 'c', b);
  t = LIST_FIRST(&a->trans);
  if (t->symfrom != 'a' || t->symto != 'c')
    fprintf(stderr, "%c-%c\n", t->symfrom, t->symto);
  fa_trans_destroy(t);

  fa_trans_create(a, 'a', b);
  fa_trans_create(a, 'd', b);
  fa_trans_create(a, 'b', b);
  t = LIST_FIRST(&a->trans);
  fa_trans_destroy(t);

  fa_trans_create(a, FA_SYMBOL_E, b);
  fa_trans_create(a, 'a', a);
  fa_trans_create(a, FA_SYMBOL_E, a);
  fa_trans_create(a, FA_SYMBOL_E, a);
  t = LIST_FIRST(&a->trans);
  fa_trans_destroy(t);
  t = LIST_FIRST(&a->trans);
  fa_trans_destroy(t);
  t = LIST_FIRST(&a->trans);
  fa_trans_destroy(t);

  fa_trans_create(a, 'a', b);
  fa_trans_create(a, 'b', a);
  t = LIST_FIRST(&a->trans);
  fa_trans_destroy(t);
  t = LIST_FIRST(&a->trans);
  fa_trans_destroy(t);

  fa_destroy(fa);
}

int main(int argc, char **argv) {
  char *dir = NULL;

  signal(SIGALRM, sigalarm_handler);

  fa_init();

  test_misc();

  while (1) {
    int c;
    int index;
    struct option options[] = {
      // name, has_arg, flag, val
      {"dir", 1, NULL, 'd'},
      {"fuzz", 0, NULL, 'f'},
      {0, 0, 0, 0}
    };

    c = getopt_long(argc, argv, "d:f", options, &index);
    if (c == -1)
      break;

    switch (c) {
      case 0:
        break; // flag is set
      case 'd':
        dir = optarg;
        break;
      case 'f':
        test_regexp_fuzz();
        return 0;
        break;
      case '?':
        break;
      default:
        fprintf(stderr, "unknown option %d\n", c);
        exit(1);
        break;
    }
  }

  if (!dir) {
    fprintf(stderr, "please specify --dir\n");
    exit(1);
  }

  tests = 0;
  test_cases = 0;
  test_cases_fail = 0;

  test_dir(dir);

  fprintf(stderr, "sim: %d bytes, simbitcomp: %d bytes (%d%%)\n",
          sim_total_bytes, simbitcomp_total_bytes,
          simbitcomp_total_bytes * 100 / sim_total_bytes);
  fprintf(stderr, "%d tests, %d cases, %d failed\n",
          tests, test_cases, test_cases_fail);

  return 0;
}
