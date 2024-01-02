
/* Compiler implementation of the D programming language
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/dlang/dmd/blob/master/src/dmd/mars.h
 */

#pragma once

/*
It is very important to use version control macros correctly - the
idea is that host and target are independent. If these are done
correctly, cross compilers can be built.
The host compiler and host operating system are also different,
and are predefined by the host compiler. The ones used in
dmd are:

Macros defined by the compiler, not the code:

    Compiler:
        __GNUC__        Gnu compiler
        __clang__       Clang compiler

    Host operating system:
        __linux__       Linux

For the target systems, there are the target operating system and
the target object file format:

    Target operating system:
        TARGET_LINUX    Covers 32 and 64 bit linux

    It is expected that the compiler for each platform will be able
    to generate 32 and 64 bit code from the same compiler binary.

    There are currently no macros for byte endianness order.
 */


#include "root/dsystem.h"

void unittests();

struct OutBuffer;

#include "globals.h"

#include "root/ctfloat.h"

#include "complex_t.h"

#include "errors.h"

class Dsymbol;
class Library;
struct File;
void obj_start(char *srcfile);
void obj_end(Library *library, File *objfile);
void obj_append(Dsymbol *s);
void obj_write_deferred(Library *library);

/// Utility functions used by both main and frontend.
void readFile(Loc loc, File *f);
void writeFile(Loc loc, File *f);
void ensurePathToNameExists(Loc loc, const char *name);

const char *importHint(const char *s);
/// Little helper function for writing out deps.
void escapePath(OutBuffer *buf, const char *fname);
