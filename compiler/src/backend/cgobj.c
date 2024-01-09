// Copyright (C) 1984-1998 by Symantec
// Copyright (C) 2000-2021 by The D Language Foundation, All Rights Reserved
// http://www.digitalmars.com
// Written by Walter Bright
/*
 * This source file is made available for personal use
 * only. The license is in backendlicense.txt
 * For any other uses, please contact Digital Mars.
 */

#include        <stdio.h>
#include        <string.h>
#include        <stdlib.h>
#include        <malloc.h>
#include        <ctype.h>
#include        <direct.h>


#include        "cc.h"
#include        "global.h"
#include        "cgcv.h"
#include        "code.h"
#include        "type.h"
#include        "outbuf.h"

#include        "md5.h"

struct Loc
{
    char *filename;
    unsigned linnum;
    unsigned charnum;

    Loc(int y, int x)
    {
        linnum = y;
        charnum = x;
        filename = NULL;
    }
};

void error(Loc loc, const char *format, ...);


// C++ name mangling is handled by front end
#define cpp_mangle(s) ((s)->Sident)
