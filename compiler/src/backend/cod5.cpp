// Copyright (C) 1995-1998 by Symantec
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
#include        <time.h>
#include        "cc.hpp"
#include        "el.hpp"
#include        "oper.hpp"
#include        "code.hpp"
#include        "global.hpp"
#include        "type.hpp"

static char __file__[] = __FILE__;      /* for tassert.h                */
#include        "tassert.hpp"

STATIC void pe_add(block *b);
STATIC int need_prolog(block *b);

/********************************************************
 * Determine which blocks get the function prolog and epilog
 * attached to them.
 */

void cod5_prol_epi()
{
    cod5_noprol();
}

/**********************************************
 * No prolog/epilog optimization.
 */

void cod5_noprol()
{
    block *b;

    //printf("no prolog optimization\n");
    startblock->Bflags |= BFLprolog;
    for (b = startblock; b; b = b->Bnext)
    {
        b->Bflags &= ~BFLoutsideprolog;
        switch (b->BC)
        {   case BCret:
            case BCretexp:
                b->Bflags |= BFLepilog;
                break;
            default:
                b->Bflags &= ~BFLepilog;
        }
    }
}

/*********************************************
 * Add block b, and its successors, to those blocks outside those requiring
 * the function prolog.
 */

STATIC void pe_add(block *b)
{   list_t bl;

    if (b->Bflags & BFLoutsideprolog ||
        need_prolog(b))
        return;

    b->Bflags |= BFLoutsideprolog;
    for (bl = b->Bsucc; bl; bl = list_next(bl))
        pe_add(list_block(bl));
}

/**********************************************
 * Determine if block needs the function prolog to be set up.
 */

STATIC int need_prolog(block *b)
{
    if (b->Bregcon.used & fregsaved)
        goto Lneed;

    // If block referenced a param in 16 bit code
    if (!I32 && b->Bflags & BFLrefparam)
        goto Lneed;

    // If block referenced a stack local
    if (b->Bflags & BFLreflocal)
        goto Lneed;

    return 0;

Lneed:
    return 1;
}
