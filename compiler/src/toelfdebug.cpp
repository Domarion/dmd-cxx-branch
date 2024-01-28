
/* Compiler implementation of the D programming language
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/D-Programming-Language/dmd/blob/master/src/toelfdebug.c
 */

#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include <assert.h>

#include "mars.hpp"
#include "module.hpp"
#include "mtype.hpp"
#include "declaration.hpp"
#include "statement.hpp"
#include "enum.hpp"
#include "aggregate.hpp"
#include "init.hpp"
#include "attrib.hpp"
#include "id.hpp"
#include "import.hpp"
#include "template.hpp"

#include "rmem.hpp"
#include "cc.hpp"
#include "global.hpp"
#include "oper.hpp"
#include "code.hpp"
#include "type.hpp"
#include "dt.hpp"
#include "outbuf.hpp"
#include "irstate.hpp"

/****************************
 * Emit symbolic debug info in Dwarf2 format.
 */

void toDebug(EnumDeclaration *ed)
{
    //printf("EnumDeclaration::toDebug('%s')\n", ed->toChars());
}

void toDebug(StructDeclaration *sd)
{
}

void toDebug(ClassDeclaration *cd)
{
}
