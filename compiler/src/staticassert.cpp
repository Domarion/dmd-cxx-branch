
/* Compiler implementation of the D programming language
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/D-Programming-Language/dmd/blob/master/src/staticassert.c
 */

#include "root/dsystem.hpp"

#include "mars.hpp"
#include "dsymbol.hpp"
#include "staticassert.hpp"
#include "expression.hpp"
#include "id.hpp"
#include "scope.hpp"
#include "template.hpp"
#include "declaration.hpp"

bool evalStaticCondition(Scope *sc, Expression *exp, Expression *e, bool &errors);

/********************************* AttribDeclaration ****************************/

StaticAssert::StaticAssert(Loc loc, Expression *exp, Expression *msg)
        : Dsymbol(Id::empty)
{
    this->loc = loc;
    this->exp = exp;
    this->msg = msg;
}

Dsymbol *StaticAssert::syntaxCopy(Dsymbol *s)
{
    assert(!s);
    return new StaticAssert(loc, exp->syntaxCopy(), msg ? msg->syntaxCopy() : nullptr);
}

void StaticAssert::addMember(Scope *, ScopeDsymbol *)
{
    // we didn't add anything
}

bool StaticAssert::oneMember(Dsymbol **ps, Identifier *)
{
    //printf("StaticAssert::oneMember())\n");
    *ps = nullptr;
    return true;
}

const char *StaticAssert::kind() const
{
    return "static assert";
}
