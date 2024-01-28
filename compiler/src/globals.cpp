
/* Compiler implementation of the D programming language
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/D-Programming-Language/dmd/blob/master/src/mars.c
 */

#include "globals.hpp"

#include "root/filename.hpp"

Global global;

unsigned Global::startGagging()
{
    ++gag;
    gaggedWarnings = 0;
    return gaggedErrors;
}

bool Global::endGagging(unsigned oldGagged)
{
    bool anyErrs = (gaggedErrors != oldGagged);
    --gag;
    // Restore the original state of gagged errors; set total errors
    // to be original errors + new ungagged errors.
    errors -= (gaggedErrors - oldGagged);
    gaggedErrors = oldGagged;
    return anyErrs;
}

void Global::increaseErrorCount()
{
    if (gag)
        ++gaggedErrors;
    ++errors;
}


const char *Loc::toChars() const
{
    OutBuffer buf;

    if (filename)
    {
        buf.printf("%s", filename);
    }

    if (linnum)
    {
        buf.printf("(%d", linnum);
        if (global.params.showColumns && charnum)
            buf.printf(",%d", charnum);
        buf.writeByte(')');
    }
    return buf.extractChars();
}

Loc::Loc(const char *filename, unsigned linnum, unsigned charnum)
{
    this->linnum = linnum;
    this->charnum = charnum;
    this->filename = filename;
}

bool Loc::equals(const Loc& loc)
{
    return (!global.params.showColumns || charnum == loc.charnum) &&
        linnum == loc.linnum && FileName::equals(filename, loc.filename);
}
