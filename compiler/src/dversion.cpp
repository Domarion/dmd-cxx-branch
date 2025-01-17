
/* Compiler implementation of the D programming language
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/D-Programming-Language/dmd/blob/master/src/version.c
 */

#include "root/dsystem.hpp"
#include "root/root.hpp"

#include "identifier.hpp"
#include "dsymbol.hpp"
#include "cond.hpp"
#include "version.hpp"
#include "module.hpp"

void checkReserved(Loc loc, const char *ident);

/* ================================================== */

/* DebugSymbol's happen for statements like:
 *      debug = identifier;
 */

DebugSymbol::DebugSymbol(Loc loc, Identifier *ident)
    : Dsymbol(ident)
{
    this->loc = loc;
}

const char *DebugSymbol::toChars()
{
    if (ident)
        return ident->toChars();

    return "";
}

Dsymbol *DebugSymbol::syntaxCopy(Dsymbol *s)
{
    assert(!s);
    DebugSymbol *ds = new DebugSymbol(loc, ident);
    return ds;
}

void DebugSymbol::addMember(Scope *, ScopeDsymbol *sds)
{
    Module *m = sds->isModule();

    // Do not add the member to the symbol table,
    // just make sure subsequent debug declarations work.
    if (ident)
    {
        if (!m)
        {
            error("declaration must be at module level");
            errors = true;
        }
        else
        {
            if (findCondition(m->debugidsNot, ident))
            {
                error("defined after use");
                errors = true;
            }
            if (!m->debugids)
                m->debugids = new Identifiers();
            m->debugids->push(ident);
        }
    }
    else
    {
        if (!m)
        {
            error("level declaration must be at module level");
            errors = true;
        }
    }
}

const char *DebugSymbol::kind() const
{
    return "debug";
}

/* ================================================== */

/* VersionSymbol's happen for statements like:
 *      version = identifier;
 */

VersionSymbol::VersionSymbol(Loc loc, Identifier *ident)
    : Dsymbol(ident)
{
    this->loc = loc;
}

const char *VersionSymbol::toChars()
{
    if (ident)
        return ident->toChars();

    return "";
}

Dsymbol *VersionSymbol::syntaxCopy(Dsymbol *s)
{
    assert(!s);
    VersionSymbol *ds = ident ? new VersionSymbol(loc, ident)
                              : nullptr;
    return ds;
}

void VersionSymbol::addMember(Scope *, ScopeDsymbol *sds)
{
    //printf("VersionSymbol::addMember('%s') %s\n", sds->toChars(), toChars());
    Module *m = sds->isModule();

    // Do not add the member to the symbol table,
    // just make sure subsequent debug declarations work.
    if (ident)
    {
        checkReserved(loc, ident->toChars());
        if (!m)
        {
            error("declaration must be at module level");
            errors = true;
        }
        else
        {
            if (findCondition(m->versionidsNot, ident))
            {
                error("defined after use");
                errors = true;
            }
            if (!m->versionids)
                m->versionids = new Identifiers();
            m->versionids->push(ident);
        }
    }
    else
    {
        if (!m)
        {
            error("level declaration must be at module level");
            errors = true;
        }
    }
}

const char *VersionSymbol::kind() const
{
    return "version";
}
