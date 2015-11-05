//
// Copyright (c) 2015 Waystream AB
// All rights reserved.
//
// This software may be modified and distributed under the terms
// of the NetBSD license.  See the LICENSE file for details.
//

#ifndef __FA_TEXT_H__
#define __FA_TEXT_H__

#include <stdio.h>

#include "fa.h"

fa_t *fa_text_input(char *arg);
int fa_text_output(fa_t *fa, char *arg, char *label);

#endif
