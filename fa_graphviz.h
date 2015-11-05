//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

#ifndef __FA_GRAPHVIZ_H__
#define __FA_GRAPHVIZ_H__

#include <stdio.h>

#include "fa.h"

typedef char *(fa_graphviz_state_name_f)(fa_state_t *state);

int fa_graphviz_output(fa_t *fa, char *arg, char *label);
void fa_graphviz_set_state_name_cb(fa_graphviz_state_name_f cb);

#endif
