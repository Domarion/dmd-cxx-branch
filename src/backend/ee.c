// Copyright (C) 1995-1998 by Symantec
// Copyright (C) 2000-2021 by The D Language Foundation, All Rights Reserved
// http://www.digitalmars.com
// Written by Walter Bright
/*
 * This source file is made available for personal use
 * only. The license is in backendlicense.txt
 * For any other uses, please contact Digital Mars.
 */


/*
 * Code to handle debugger expression evaluation
 */

#include        <stdio.h>
#include        <string.h>
#include        <time.h>
#include        "cc.h"
#include        "token.h"
#include        "global.h"
#include        "type.h"
#include        "oper.h"
#include        "el.h"
#include        "exh.h"
#if TX86
#include        "cgcv.h"
#endif

#include        "iasm.h"

static char __file__[] = __FILE__;      /* for tassert.h                */
#include        "tassert.h"

#if MARS
EEcontext eecontext;
#endif

//////////////////////////////////////
// Convert any symbols generated for the debugger expression to SCstack
// storage class.

void eecontext_convs(unsigned marksi)
{   unsigned u;
    unsigned top;
    symtab_t *ps;

    // Change all generated SCauto's to SCstack's
    ps = cstate.CSpsymtab;

    top = ps->top;
    //printf("eecontext_convs(%d,%d)\n",marksi,top);
    for (u = marksi; u < top; u++)
    {   symbol *s;

        s = ps->tab[u];
        switch (s->Sclass)
        {
            case SCauto:
            case SCregister:
                s->Sclass = SCstack;
                s->Sfl = FLstack;
                break;
        }
    }
}
