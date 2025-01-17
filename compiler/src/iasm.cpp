
/* Compiler implementation of the D programming language
 * Copyright (C) 2018-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/D-Programming-Language/dmd/blob/master/src/iasm.c
 */

/* Inline assembler for the D programming language compiler
 */

#include "scope.hpp"
#include "declaration.hpp"
#include "statement.hpp"

Statement *inlineAsmSemantic(InlineAsmStatement *s, Scope *sc);

Statement *asmSemantic(AsmStatement *s, Scope *sc)
{
    //printf("AsmStatement::semantic()\n");

    FuncDeclaration *fd = sc->parent->isFuncDeclaration();
    assert(fd);

    if (!s->tokens)
        return nullptr;

    // Assume assembler code takes care of setting the return value
    sc->func->hasReturnExp |= 8;

    InlineAsmStatement *ias = new InlineAsmStatement(s->loc, s->tokens);
    return inlineAsmSemantic(ias, sc);
}
