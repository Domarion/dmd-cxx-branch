
/* Compiler implementation of the D programming language
 * Copyright (C) 2010-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/D-Programming-Language/dmd/blob/master/src/imphint.c
 */


#include "root/dsystem.hpp"

#include "mars.hpp"

/******************************************
 * Looks for undefined identifier s to see
 * if it might be undefined because an import
 * was not specified.
 * Not meant to be a comprehensive list of names in each module,
 * just the most common ones.
 */

const char *importHint(const char *s)
{
    static const char *modules[] =
    {   "core.stdc.stdio",
        "std.stdio",
        "std.math",
        nullptr
    };
    static const char *names[] =
    {
        "printf", nullptr,
        "writeln", nullptr,
        "sin", "cos", "sqrt", "fabs", nullptr,
    };
    int m = 0;
    for (int n = 0; modules[m]; n++)
    {
        const char *p = names[n];
        if (p == nullptr)
        {
            m++;
            continue;
        }
        assert(modules[m]);
        if (strcmp(s, p) == 0)
            return modules[m];
    }
    return nullptr;        // didn't find it
}
