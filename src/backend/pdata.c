// Compiler implementation of the D programming language
// Copyright (C) 2012-2021 by The D Language Foundation, All Rights Reserved
// written by Walter Bright
// http://www.digitalmars.com
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// https://github.com/D-Programming-Language/dmd/blob/master/src/backend/pdata.c

// This module generates the .pdata and .xdata sections for Win64

#include        <stdio.h>
#include        <string.h>
#include        <stdlib.h>
#include        <time.h>

#include        "cc.h"
#include        "el.h"
#include        "code.h"
#include        "oper.h"
#include        "global.h"
#include        "type.h"
#include        "dt.h"
#include        "exh.h"
#include        "obj.h"

static char __file__[] = __FILE__;      /* for tassert.h                */
#include        "tassert.h"

