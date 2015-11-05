//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

// TODO: fa_regexp_yacc.y, recursion depth a problem?
// TODO: a{3,0} not allowed..
// TODO: a{0,0} skip
// TODO: per union element anchoring end/start null flags? fa_regexp_ctx?

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "fa.h"
#include "fa_misc.h"
#include "fa_regexp.h"
#include "fa_regexp_bin.h"
#include "fa_regexp_class.h"
#include "fa_regexp_yacc.h"


static fa_regexp_node_t *fa_regexp_node(fa_regexp_type_t type, int pos) {
  fa_regexp_node_t *frn = calloc(1, sizeof(*frn));

  frn->type = type;
  frn->pos = pos;

  return frn;
}

fa_regexp_node_t *fa_regexp_node_sub(fa_regexp_node_t *sub, int pos) {
  fa_regexp_node_t *frn;

  if (sub == NULL)
    return NULL;

  frn = fa_regexp_node(RE_SUB, pos);
  frn->value.sub.flags = 0;
  frn->value.sub.sub = sub;

  return frn;
}

fa_regexp_node_t *fa_regexp_node_options(int neg, uint32_t flags,
                                         fa_regexp_node_t *sub, int pos) {
  fa_regexp_node_t *frn;

  if (sub == NULL)
    return NULL;

  frn = fa_regexp_node(RE_OPTIONS, pos);
  frn->value.options.neg = neg;
  frn->value.options.flags = flags;
  frn->value.options.sub = sub;

  return frn;
}

fa_regexp_node_t *fa_regexp_node_concat(fa_regexp_node_t *sub1,
                                        fa_regexp_node_t *sub2, int pos) {
  fa_regexp_node_t *frn;
  char *s;
  int l;

  if (sub1 == NULL || sub2 == NULL) {
    if (sub1)
      fa_regexp_node_free(sub1);
    if (sub2)
      fa_regexp_node_free(sub2);

    return NULL;
  }

  if (sub1 && sub2 &&
     sub1->type == RE_STRING &&
     sub2->type == RE_STRING) {

    // concat string with string
    l = sub1->value.string.len + sub2->value.string.len;
    s = malloc(l);
    memcpy(s, sub1->value.string.str, sub1->value.string.len);
    memcpy(s + sub1->value.string.len, sub2->value.string.str,
           sub2->value.string.len);
    frn = fa_regexp_node_string(s, l, pos);
    fa_regexp_node_free(sub1);
    fa_regexp_node_free(sub2);
    free(s);
  } else {
    frn = fa_regexp_node(RE_CONCAT, pos);
    frn->value.concat.sub1 = sub1;
    frn->value.concat.sub2 = sub2;
  }

  return frn;
}

fa_regexp_node_t *fa_regexp_node_union(fa_regexp_node_t *sub1,
                                       fa_regexp_node_t *sub2, int pos) {
  fa_regexp_node_t *frn;

  if (sub1 == NULL || sub2 == NULL) {
    if (sub1)
      fa_regexp_node_free(sub1);
    if (sub2)
      fa_regexp_node_free(sub2);

    return NULL;
  }

  frn = fa_regexp_node(RE_UNION, pos);
  frn->value.union_.sub1 = sub1;
  frn->value.union_.sub2 = sub2;

  return frn;
}

fa_regexp_node_t *fa_regexp_node_repeat(fa_regexp_node_t *sub, int onlymin,
                                        int min, int max, int pos) {
  fa_regexp_node_t *frn;

  if (sub == NULL)
    return NULL;

  frn = fa_regexp_node(RE_REPEAT, pos);
  frn->value.repeat.sub = sub;
  frn->value.repeat.onlymin = onlymin;
  frn->value.repeat.min = min;
  frn->value.repeat.max = max;

  return frn;
}

fa_regexp_node_t *fa_regexp_node_string(char *str, int len, int pos) {
  fa_regexp_node_t *frn = fa_regexp_node(RE_STRING, pos);

  frn->value.string.len = len;
  frn->value.string.str = malloc(len);
  memcpy(frn->value.string.str, str, len);

  return frn;
}

fa_regexp_node_t *fa_regexp_node_class(int neg, fa_regexp_class_t *class_,
                                       int pos) {
  fa_regexp_node_t *frn = fa_regexp_node(RE_CLASS, pos);

  frn->value.class_.neg = neg;
  frn->value.class_.class_ = class_;

  return frn;
}

fa_regexp_node_t *fa_regexp_node_binary(fa_regexp_bin_t *bin, int pos) {
  fa_regexp_node_t *frn = fa_regexp_node(RE_BINARY, pos);

  frn->value.binary = bin;

  return frn;
}

void fa_regexp_node_free(fa_regexp_node_t *node) {
  switch (node->type) {
    case RE_SUB:
      if (node->value.sub.sub)
        fa_regexp_node_free(node->value.sub.sub);
      break;
    case RE_OPTIONS:
      if (node->value.options.sub)
        fa_regexp_node_free(node->value.options.sub);
      break;
    case RE_CONCAT:
      if (node->value.concat.sub1)
        fa_regexp_node_free(node->value.concat.sub1);
      if (node->value.concat.sub2)
        fa_regexp_node_free(node->value.concat.sub2);
      break;
    case RE_UNION:
      if (node->value.union_.sub1)
        fa_regexp_node_free(node->value.union_.sub1);
      if (node->value.union_.sub2)
        fa_regexp_node_free(node->value.union_.sub2);
      break;
    case RE_REPEAT:
      if (node->value.repeat.sub)
        fa_regexp_node_free(node->value.repeat.sub);
      break;
    case RE_STRING:
      if (node->value.string.str)
        free(node->value.string.str);
      break;
    case RE_CLASS:
      if (node->value.class_.class_)
        fa_regexp_class_destroy(node->value.class_.class_);
      break;
    case RE_BINARY:
      if (node->value.binary)
        fa_regexp_bin_destroy(node->value.binary);
      break;
    default:
      assert(0);
  }

  free(node);
}

#if 0
static char *fa_regexp_node_type(fa_regexp_type_t type) {
  switch (type) {
    case RE_SUB: return "sub"; break;
    case RE_OPTIONS: return "options"; break;
    case RE_CONCAT: return "concat"; break;
    case RE_UNION: return "union"; break;
    case RE_REPEAT: return "repeat"; break;
    case RE_STRING: return "string"; break;
    case RE_CLASS: return "class"; break;
    case RE_BINARY: return "binary"; break;
    default:
      assert(0);
  }

  return NULL;
}

static void fa_regexp_node_dump_hex(char *s, int len, char *esc) {
  int i;

  for (i = 0; i < len; i++)
    if (isprint(s[i]))
      fprintf(stderr, "%c", s[i]);
    else
      fprintf(stderr, "%sx%.2x", esc, (uint8_t)s[i]);
}

static void fa_regexp_node_dump_ex(int indent, fa_regexp_node_t *node) {
  int i;

  for (i = 0; i < indent; i++)
    fprintf(stderr, "  ");

  fprintf(stderr, "%s at %d\n", fa_regexp_node_type(node->type), node->pos);

  switch (node->type) {
    case RE_SUB:
      if (node->value.sub.sub)
        fa_regexp_node_dump_ex(indent+1, node->value.sub.sub);
      break;
    case RE_OPTIONS:
      for (i = 0; i < indent; i++)
        fprintf(stderr, "  ");
      fprintf(stderr, "  neg=%d options=%x\n",
              node->value.options.neg, node->value.options.flags);
      if (node->value.options.sub)
        fa_regexp_node_dump_ex(indent+1, node->value.options.sub);
      break;
    case RE_CONCAT:
      if (node->value.concat.sub1)
        fa_regexp_node_dump_ex(indent+1, node->value.concat.sub1);
      if (node->value.concat.sub2)
        fa_regexp_node_dump_ex(indent+1, node->value.concat.sub2);
      break;
    case RE_UNION:
      if (node->value.union_.sub1)
        fa_regexp_node_dump_ex(indent+1, node->value.union_.sub1);
      if (node->value.union_.sub2)
        fa_regexp_node_dump_ex(indent+1, node->value.union_.sub2);
      break;
    case RE_REPEAT:
      for (i = 0; i < indent; i++)
        fprintf(stderr, "  ");
      fprintf(stderr, "  onlymin=%d min=%d max=%d\n",
              node->value.repeat.onlymin, node->value.repeat.min,
              node->value.repeat.max);
      fa_regexp_node_dump_ex(indent+1, node->value.repeat.sub);
      break;
    case RE_STRING:
      for (i = 0; i < indent; i++)
        fprintf(stderr, "  ");
      fprintf(stderr, "  \"");
      fa_regexp_node_dump_hex(node->value.string.str,
                              node->value.string.len, "\\");
      fprintf(stderr, "\"\n");
      break;
    case RE_CLASS:
      for (i = 0; i < indent; i++)
        fprintf(stderr, "  ");
      fprintf(stderr, "  neg=%d\n", node->value.class_.neg);
      break;
    case RE_BINARY:
      break;
    default:
      fprintf(stderr, "unknown\n");
      break;
  }
}

void fa_regexp_node_dump(fa_regexp_node_t *node) {
  fa_regexp_node_dump_ex(0, node);
}

static void fa_regexp_node_dump_pgf_ex(int indent, fa_regexp_node_t *node) {
  int i;

  for (i = 0; i < indent; i++)
    fprintf(stderr, "  ");

  if (indent == 0)
    fprintf(stderr, "\\node {%s", fa_regexp_node_type(node->type));
  else
    fprintf(stderr, "child {node {%s", fa_regexp_node_type(node->type));

  switch (node->type) {
    case RE_SUB:
      fprintf(stderr, "}\n");
      if (node->value.sub.sub)
        fa_regexp_node_dump_pgf_ex(indent+1, node->value.sub.sub);
      break;
    case RE_OPTIONS:
      fprintf(stderr, "}\n");
      if (node->value.options.sub)
        fa_regexp_node_dump_pgf_ex(indent+1, node->value.options.sub);
      break;
    case RE_CONCAT:
      fprintf(stderr, "}\n");
      if (node->value.concat.sub1)
        fa_regexp_node_dump_pgf_ex(indent+1, node->value.concat.sub1);
      if (node->value.concat.sub2)
        fa_regexp_node_dump_pgf_ex(indent+1, node->value.concat.sub2);
      break;
    case RE_UNION:
      fprintf(stderr, "}\n");
      if (node->value.union_.sub1)
        fa_regexp_node_dump_pgf_ex(indent+1, node->value.union_.sub1);
      if (node->value.union_.sub2)
        fa_regexp_node_dump_pgf_ex(indent+1, node->value.union_.sub2);
      break;
    case RE_REPEAT:
      fprintf(stderr, " %d,%d}\n",
              node->value.repeat.min,
              node->value.repeat.max);
      fa_regexp_node_dump_pgf_ex(indent+1, node->value.repeat.sub);
      break;
    case RE_STRING:
      fprintf(stderr, " \"");
      fa_regexp_node_dump_hex(node->value.string.str,
                              node->value.string.len, "$\\backslash$");
      fprintf(stderr, "\"}\n");
      break;
    case RE_CLASS:
      fprintf(stderr, "}\n");
      break;
    case RE_BINARY:
      fprintf(stderr, "}\n");
      break;
    default:
      fprintf(stderr, "unknown\n");
      break;
  }

  if (indent != 0) {
    for (i = 0; i < indent; i++)
      fprintf(stderr, "  ");

    fprintf(stderr, "}\n");
  }
}

void fa_regexp_node_dump_pgf(fa_regexp_node_t *node) {
  fa_regexp_node_dump_pgf_ex(0, node);
  fprintf(stderr, ";\n");
}
#endif

static fa_t *fa_regexp_node_fa(fa_regexp_node_t *node,
                               char **errstr, int *errpos,
                               uint32_t *flags,
                               fa_limit_t *limit) {
  fa_t *sub1;
  fa_t *sub2;
  int n;

  switch (node->type) {
    case RE_SUB:
      node->value.sub.flags = *flags;
      return fa_regexp_node_fa(node->value.sub.sub, errstr, errpos,
                               &node->value.sub.flags, limit);
      break;
    case RE_OPTIONS:
      *flags =
        (*flags & ~node->value.options.flags) |
        (node->value.options.neg ? 0 : node->value.options.flags);
      return fa_regexp_node_fa(node->value.options.sub, errstr, errpos,
                               flags, limit);
      break;
    case RE_CONCAT:
      sub1 = fa_regexp_node_fa(node->value.concat.sub1, errstr, errpos,
                               flags, limit);
      if (!sub1)
        return NULL;
      sub2 = fa_regexp_node_fa(node->value.concat.sub2, errstr, errpos,
                               flags, limit);
      if (!sub2) {
        fa_destroy(sub1);
        return NULL;
      }
      return fa_concat(sub1, sub2);
      break;
    case RE_UNION:
      sub1 = fa_regexp_node_fa(node->value.union_.sub1, errstr, errpos,
                               flags, limit);
      if (!sub1)
        return NULL;
      sub2 = fa_regexp_node_fa(node->value.union_.sub2, errstr, errpos,
                               flags, limit);
      if (!sub2) {
        fa_destroy(sub1);
        return NULL;
      }
      return fa_union(sub1, sub2);
      break;
    case RE_REPEAT:
      // a{0} case
      if (node->value.repeat.onlymin &&
         node->value.repeat.min == 0) {
        return fa_string((uint8_t*)"", 0);
      }

      if (node->value.repeat.max != 0 &&
         node->value.repeat.min > node->value.repeat.max) {
        *errpos = node->pos;
        *errstr = "min repeat must be less or equal to max repeat";
        return NULL;
      }

      sub1 = fa_regexp_node_fa(node->value.repeat.sub, errstr, errpos,
                               flags, limit);
      if (!sub1)
        return NULL;

      n = MMAX(node->value.repeat.min, node->value.repeat.max);
      if (limit && (sub1->states_n * n > limit->states ||
                   sub1->trans_n * n > limit->trans)) {
        fa_destroy(sub1);
        *errpos = node->pos;
        *errstr = "repeat will generates too many states or transitions";
        return NULL;
      }

      return fa_repeat(sub1, node->value.repeat.min, node->value.repeat.max);
      break;
    case RE_STRING:
      return fa_string_ex((uint8_t*)node->value.string.str,
                          node->value.string.len,
                          *flags & FA_REGEXP_F_ICASE);
      break;
    case RE_CLASS:
      sub1 = fa_regexp_class_fa(node->value.class_.class_,
                                node->value.class_.neg,
                                *flags & FA_REGEXP_F_ICASE);
      // [^\x00-\xff] matches nothing
      if (sub1 == NULL) {
        *errstr = "character class does not match any characters";
        *errpos = node->pos;
        return NULL;
      }

      return sub1;
      break;
    case RE_BINARY:
      if (fa_regexp_bin_bitlen(node->value.binary) % 8 != 0) {
        *errstr = "binary is not byte aligned";
        *errpos = node->pos;
        return NULL;
      }

      return fa_regexp_bin_fa(node->value.binary);
      break;
    default:
      assert(0);
  }

  return NULL;
}

static void fa_regexp_state_any(fa_state_t *fs) {
  int i;

  for (i = 0; i < 256; i++)
    fa_trans_create(fs, i, fs);
}

static fa_t *fa_regexp_start_unanchor(fa_t *fa) {
  fa_state_t *any;

  any = fa_state_create(fa);
  fa_regexp_state_any(any);

  fa_trans_create(any, FA_SYMBOL_E, fa->start);
  fa->start = any;

  return fa;
}

static fa_t *fa_regexp_end_unanchor(fa_t *fa) {
  fa_state_t *fs;
  fa_state_t *any;

  any = fa_state_create(fa);
  any->flags |= FA_STATE_F_ACCEPTING;
  fa_regexp_state_any(any);

  LIST_FOREACH(fs, &fa->states, link) {
    if (fs == any || !(fs->flags & FA_STATE_F_ACCEPTING))
      continue;

    fs->flags &= ~FA_STATE_F_ACCEPTING;
    fa_trans_create(fs, FA_SYMBOL_E, any);
  }

  return fa;
}

fa_t *fa_regexp_fa(char *str, char **errstr, int *errpos, fa_limit_t *limit) {
  fa_regexp_node_t *root;
  fa_t *fa;
  char *s;
  int len;
  int start_anchor = 0;
  int end_anchor = 0;

  *errstr = NULL;
  *errpos = 0;
  s = str;
  len = strlen(str);

  if (s[0] == '^') {
    start_anchor = 1;
    s++;
    len--;
  }

  // if ends with "$" and its not escaped
  if (s[len-1] == '$' && (len == 1 || (len > 1 && s[len-2] != '\\'))) {
    end_anchor = 1;
    len--;
  }

  root = fa_regexp_yacc_parse(s, len, errstr, errpos);
  if (!*errstr) {
    uint32_t flags = 0;

    fa = fa_regexp_node_fa(root, errstr, errpos, &flags, limit);
    fa_regexp_node_free(root);
    if (!*errstr) {
      if (!start_anchor)
        fa = fa_regexp_start_unanchor(fa);
      if (!end_anchor)
        fa = fa_regexp_end_unanchor(fa);

      return fa;
    }
  }

  if (*errpos > 0 && start_anchor)
    (*errpos)++; // ^ was removed

  return NULL;
}
