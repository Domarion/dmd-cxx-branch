// Copyright (C) 1985-1998 by Symantec
// Copyright (C) 2000-2021 by The D Language Foundation, All Rights Reserved
// http://www.digitalmars.com
// Written by Walter Bright
/*
 * This source file is made available for personal use
 * only. The license is in backendlicense.txt
 * For any other uses, please contact Digital Mars.
 */

/* Global variables for PARSER  */

#include        <stdio.h>
#include        <time.h>

#include        "cc.hpp"
#include        "global.hpp"
#include        "oper.hpp"
#include        "type.hpp"
#include        "go.hpp"
#include        "ty.hpp"
#include        "code.hpp"

#include        "optab.cpp"
#include        "tytab.cpp"

/* Global flags:
 */

char PARSER;                    // indicate we're in the parser
char OPTIMIZER;                 // indicate we're in the optimizer
int structalign;                /* alignment for members of structures  */
char dbcs;                      // current double byte character set

int TYptrdiff = TYint;
int TYsize = TYuint;
int TYsize_t = TYuint;

char debuga,debugb,debugc,debugd,debuge,debugf,debugr,debugs,debugt,debugu,debugw,debugx,debugy;

/* File variables: */

char *argv0;                    // argv[0] (program name)
FILE *fdep = nullptr;              // dependency file stream pointer
FILE *flst = nullptr;              // list file stream pointer
FILE *fin = nullptr;               // input file
char     *foutdir = nullptr,       // directory to place output files in
         *finname = nullptr,
        *foutname = nullptr,
        *fsymname = nullptr,
        *fphreadname = nullptr,
        *ftdbname = nullptr,
        *fdepname = nullptr,
        *flstname = nullptr;       /* the filename strings                 */

int pathsysi;                   // -isystem= index
list_t headers;                 /* pre-include files                    */

/* Data from lexical analyzer: */

unsigned idhash = 0;    // hash value of identifier
int xc = ' ';           // character last read

/* Data for pragma processor:
 */

int colnumber = 0;              /* current column number                */

/* Other variables: */

int level = 0;                  /* declaration level                    */
                                /* 0: top level                         */
                                /* 1: function parameter declarations   */
                                /* 2: function local declarations       */
                                /* 3+: compound statement decls         */

param_t *paramlst = nullptr;       /* function parameter list              */
tym_t pointertype = TYnptr;     /* default data pointer type            */

/************************
 * Bit masks
 */

const unsigned mask[32] =
        {1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,0x8000,
         0x10000,0x20000,0x40000,0x80000,0x100000,0x200000,0x400000,0x800000,
         0x1000000,0x2000000,0x4000000,0x8000000,
         0x10000000,0x20000000,0x40000000,0x80000000};

/* From util.c */

/*****************************
 * SCxxxx types.
 */

char sytab[SCMAX] =
{
    #define X(a,b)      b,
        ENUMSCMAC
    #undef X
};

volatile int controlc_saw;              /* a control C was seen         */
symtab_t globsym;               /* global symbol table                  */
Pstate pstate;                  // parser state
Cstate cstate;                  // compiler state

unsigned
         maxblks = 0,   /* array max for all block stuff                */
                        /* dfoblks <= numblks <= maxblks                */
         numcse;        /* number of common subexpressions              */

struct Go go;

/* From debug.c */
#if DEBUG
const char *regstring[32] = {"AX","CX","DX","BX","SP","BP","SI","DI",
                             "R8","R9","R10","R11","R12","R13","R14","R15",
                             "XMM0","XMM1","XMM2","XMM3","XMM4","XMM5","XMM6","XMM7",
                             "ES","PSW","STACK","ST0","ST01","NOREG","RMload","RMstore"};
#endif

/* From nwc.c */

type *chartype;                 /* default 'char' type                  */

Obj *objmod = nullptr;
