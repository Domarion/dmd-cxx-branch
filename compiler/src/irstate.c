
/* Compiler implementation of the D programming language
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/D-Programming-Language/dmd/blob/master/src/irstate.c
 */

#include <stdio.h>

#include "mars.h"
#include "mtype.h"
#include "declaration.h"
#include "irstate.h"
#include "statement.h"
#include "aav.h"

/****
 * Access labels AA from C++ code.
 * Params:
 *  s = key
 * Returns:
 *  pointer to value if it's there, null if not
 */
Label **IRState::lookupLabel(Statement *s)
{
    Label **slot = (Label **)dmd_aaGet((AA **)labels, (void *)s);
    if (*slot)
        return slot;
    return nullptr;
}

/****
 * Access labels AA from C++ code.
 * Params:
 *  s = key
 *  label = value
 */
void IRState::insertLabel(Statement *s, Label *label)
{
    Label **slot = (Label **)dmd_aaGet((AA **)labels, (void *)s);
    *slot = label;
}

block *IRState::getBreakBlock(Identifier *ident)
{
    IRState *bc;
    if (ident)
    {
        Statement *related = nullptr;
        block *ret = nullptr;
        for (bc = this; bc; bc = bc->prev)
        {
            // The label for a breakBlock may actually be some levels up (e.g.
            // on a try/finally wrapping a loop). We'll see if this breakBlock
            // is the one to return once we reach that outer statement (which
            // in many cases will be this same statement).
            if (bc->breakBlock)
            {
                related = bc->statement->getRelatedLabeled();
                ret = bc->breakBlock;
            }
            if (bc->statement == related && bc->prev->ident == ident)
                return ret;
        }
    }
    else
    {
        for (bc = this; bc; bc = bc->prev)
        {
            if (bc->breakBlock)
                return bc->breakBlock;
        }
    }
    return nullptr;
}

block *IRState::getContBlock(Identifier *ident)
{
    IRState *bc;

    if (ident)
    {
        block *ret = nullptr;
        for (bc = this; bc; bc = bc->prev)
        {
            // The label for a contBlock may actually be some levels up (e.g.
            // on a try/finally wrapping a loop). We'll see if this contBlock
            // is the one to return once we reach that outer statement (which
            // in many cases will be this same statement).
            if (bc->contBlock)
            {
                ret = bc->contBlock;
            }
            if (bc->prev && bc->prev->ident == ident)
                return ret;
        }
    }
    else
    {
        for (bc = this; bc; bc = bc->prev)
        {
            if (bc->contBlock)
                return bc->contBlock;
        }
    }
    return nullptr;
}

block *IRState::getSwitchBlock()
{
    IRState *bc;

    for (bc = this; bc; bc = bc->prev)
    {
        if (bc->switchBlock)
            return bc->switchBlock;
    }
    return nullptr;
}

block *IRState::getDefaultBlock()
{
    IRState *bc;

    for (bc = this; bc; bc = bc->prev)
    {
        if (bc->defaultBlock)
            return bc->defaultBlock;
    }
    return nullptr;
}

block *IRState::getFinallyBlock()
{
    IRState *bc;

    for (bc = this; bc; bc = bc->prev)
    {
        if (bc->finallyBlock)
            return bc->finallyBlock;
    }
    return nullptr;
}

FuncDeclaration *IRState::getFunc()
{
    IRState *bc;

    for (bc = this; bc->prev; bc = bc->prev)
    {
    }
    return (FuncDeclaration *)(bc->symbol);
}


/**********************
 * Returns true if do array bounds checking for the current function
 */
bool IRState::arrayBoundsCheck()
{
    bool result;
    switch (global.params.useArrayBounds)
    {
        case CHECKENABLEoff:
            result = false;
            break;

        case CHECKENABLEon:
            result = true;
            break;

        case CHECKENABLEsafeonly:
        {
            result = false;
            FuncDeclaration *fd = getFunc();
            if (fd)
            {   Type *t = fd->type;
                if (t->ty == Tfunction && ((TypeFunction *)t)->trust == TRUSTsafe)
                    result = true;
            }
            break;
        }

        default:
            assert(0);
    }
    return result;
}
