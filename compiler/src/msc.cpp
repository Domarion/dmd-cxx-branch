
/* Compiler implementation of the D programming language
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/D-Programming-Language/dmd/blob/master/src/msc.c
 */

#include        <stdio.h>
#include        <string.h>
#include        <stddef.h>

#include        "mars.hpp"

#include        "cc.hpp"
#include        "global.hpp"
#include        "oper.hpp"
#include        "code.hpp"
#include        "type.hpp"
#include        "dt.hpp"

static char __file__[] = __FILE__;      /* for tassert.h                */
#include        "tassert.hpp"


extern Global global;

Config config;
Configv configv;

struct Environment;

void out_config_init(
        int model,      // 32: 32 bit code
                        // 64: 64 bit code
                        // Windows: bit 0 set to generate MS-COFF instead of OMF
        bool exe,       // true: exe file
                        // false: dll or shared library (generate PIC code)
        bool trace,     // add profiling code
        bool nofloat,   // do not pull in floating point code
        bool verbose,   // verbose compile
        bool optimize,  // optimize code
        int symdebug,   // add symbolic debug information
                        // 1: D
                        // 2: fake it with C symbolic debug info
        bool alwaysframe,       // always create standard function frame
        bool stackstomp         // add stack stomping code
        );

void out_config_debug(
        bool debugb,
        bool debugc,
        bool debugf,
        bool debugr,
        bool debugw,
        bool debugx,
        bool debugy
    );

/**************************************
 * Initialize config variables.
 */

void backend_init()
{
    //printf("out_config_init()\n");
    Param *params = &global.params;

    bool exe = params->pic == 0;

    out_config_init(
        (params->is64bit ? 64 : 32),
        exe,
        false, //params->trace,
        params->nofloat,
        params->verbose,
        params->optimize,
        params->symdebug,
        params->alwaysframe,
        params->stackstomp
    );

#ifdef DEBUG
    out_config_debug(
        params->debugb,
        params->debugc,
        params->debugf,
        params->debugr,
        false,
        params->debugx,
        params->debugy
    );
#endif
}


/***********************************
 * Return aligned 'offset' if it is of size 'size'.
 */

targ_size_t align(targ_size_t size, targ_size_t offset)
{
    switch (size)
    {
        case 1:
            break;
        case 2:
        case 4:
        case 8:
            offset = (offset + size - 1) & ~(size - 1);
            break;
        default:
            if (size >= 16)
                offset = (offset + 15) & ~15;
            else
                offset = (offset + REGSIZE - 1) & ~(REGSIZE - 1);
            break;
    }
    return offset;
}

/*******************************
 * Get size of ty
 */

targ_size_t size(tym_t ty)
{
    int sz = (tybasic(ty) == TYvoid) ? 1 : tysize(ty);
#ifdef DEBUG
    if (sz == -1)
        WRTYxx(ty);
#endif
    assert(sz!= -1);
    return sz;
}

/****************************
 * Generate symbol of type ty at DATA:offset
 */

symbol *symboldata(targ_size_t offset,tym_t ty)
{
    symbol *s = symbol_generate(SClocstat, type_fake(ty));
    s->Sfl = FLdata;
    s->Soffset = offset;
    s->Stype->Tmangle = mTYman_sys; // writes symbol unmodified in Obj::mangle
    symbol_keep(s);                 // keep around
    return s;
}

/**************************************
 */

void backend_term()
{
}
