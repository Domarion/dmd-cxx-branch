
/* Compiler implementation of the D programming language
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 */

#include "root/dsystem.hpp"
#include "mars.hpp"
#include "globals.hpp"
#include "root/file.hpp"
#include "root/filename.hpp"
#include "root/outbuffer.hpp"
#include "root/rmem.hpp"

/**
 * Reads a file, terminate the program on error
 *
 * Params:
 *   loc = The line number information from where the call originates
 *   f = a `ddmd.root.file.File` handle to read
 */
void readFile(Loc loc, File *f)
{
    if (f->read())
    {
        error(loc, "Error reading file '%s'", f->name->toChars());
        fatal();
    }
}

/**
 * Writes a file, terminate the program on error
 *
 * Params:
 *   loc = The line number information from where the call originates
 *   f = a `ddmd.root.file.File` handle to write
 */
void writeFile(Loc loc, File *f)
{
    if (f->write())
    {
        error(loc, "Error writing file '%s'", f->name->toChars());
        fatal();
    }
}

/**
 * Ensure the root path (the path minus the name) of the provided path
 * exists, and terminate the process if it doesn't.
 *
 * Params:
 *   loc = The line number information from where the call originates
 *   name = a path to check (the name is stripped)
 */
void ensurePathToNameExists(Loc loc, const char *name)
{
    const char *pt = FileName::path(name);
    if (*pt)
    {
        if (FileName::ensurePathExists(pt))
        {
            error(loc, "cannot create directory %s", pt);
            fatal();
        }
    }
    FileName::free(pt);
}

/**
 * Takes a path, and escapes '(', ')' and backslashes
 *
 * Params:
 *   buf = Buffer to write the escaped path to
 *   fname = Path to escape
 */
void escapePath(OutBuffer *buf, const char *fname)
{
    while (1)
    {
        switch (*fname)
        {
            case 0:
                return;
            case '(':
            case ')':
            case '\\':
                buf->writeByte('\\');
                /* fall through */
            default:
                buf->writeByte(*fname);
                break;
        }
        fname++;
    }
}
