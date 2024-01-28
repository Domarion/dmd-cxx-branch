/*
 * Copyright (c) 1994-1998 by Symantec
 * Copyright (C) 2000-2021 by The D Language Foundation, All Rights Reserved
 * http://www.digitalmars.com
 * Written by Walter Bright
 *
 * This source file is made available for personal use
 * only. The license is in backendlicense.txt
 * For any other uses, please contact Digital Mars.
 */

// Support for D exception handling

#include        <stdio.h>
#include        <string.h>
#include        <stdlib.h>
#include        <time.h>

#include        "cc.h"
#include        "el.h"
#include        "code.h"
#include        "oper.h"
#include        "global.h"
#include        "type.h"
#include        "dt.h"
#include        "exh.h"

static char __file__[] = __FILE__;      /* for tassert.h                */
#include        "tassert.h"

extern void error(const char *filename, unsigned linnum, unsigned charnum, const char *format, ...);

/****************************
 * Generate and output scope table.
 */

symbol *except_gentables()
{
    //printf("except_gentables()\n");
    return nullptr;
}

/**********************************************
 * Initializes the symbol s with the contents of the exception handler table.
 */

/* This is what the type should be on the target machine, not the host compiler
 *
 * struct Guard
 * {
 *    int last_index;             // previous index (enclosing guarded section)
 *    unsigned catchoffset;       // offset to catch block from symbol
 *    void *finally;              // finally code to execute
 * }
 */

void except_fillInEHTable(symbol *s)
{
    unsigned fsize = NPTRSIZE;             // target size of function pointer
    DtBuilder dtb;

    /*
        void*           pointer to start of function (Windows)
        unsigned        offset of ESP from EBP
        unsigned        offset from start of function to return code
        unsigned nguards;       // dimension of guard[] (Linux)
        Guard guard[];          // sorted such that the enclosing guarded sections come first
      catchoffset:
        unsigned ncatches;      // number of catch blocks
        {   void *type;         // symbol representing type
            unsigned bpoffset;  // EBP offset of catch variable
            void *handler;      // catch handler code
        } catch[];
     */

/* Be careful of this, as we need the sizeof Guard on the target, not
 * in the compiler.
 */
    unsigned GUARD_SIZE;
    assert(0);

    int sz = 0;

    //printf("ehtables: func = %s, offset = x%x, startblock->Boffset = x%x\n", funcsym_p->Sident, funcsym_p->Soffset, startblock->Boffset);

    // Get offset of ESP from EBP
    long spoff = cod3_spoff();
    dtb.dword(spoff);
    sz += 4;

    // Offset from start of function to return code
    dtb.dword(retoffset);
    sz += 4;

    // First, calculate starting catch offset
    int guarddim = 0;                               // max dimension of guard[]
    int ndctors = 0;                                // number of ESCdctor's
    for (block *b = startblock; b; b = b->Bnext)
    {
        if (b->BC == BC_try && b->Bscope_index >= guarddim)
            guarddim = b->Bscope_index + 1;
//      printf("b->BC = %2d, Bscope_index = %2d, last_index = %2d, offset = x%x\n",
//              b->BC, b->Bscope_index, b->Blast_index, b->Boffset);
        if (usednteh & EHcleanup)
            for (code *c = b->Bcode; c; c = code_next(c))
            {
                if (c->Iop == (ESCAPE | ESCddtor))
                    ndctors++;
            }
    }
    //printf("guarddim = %d, ndctors = %d\n", guarddim, ndctors);

    unsigned catchoffset = sz + (guarddim + ndctors) * GUARD_SIZE;

    // Generate guard[]
    int i = 0;
    for (block *b = startblock; b; b = b->Bnext)
    {
        //printf("b = %p, b->Btry = %p, b->offset = %x\n", b, b->Btry, b->Boffset);
        if (b->BC == BC_try)
        {
            assert(b->Bscope_index >= i);
            if (i < b->Bscope_index)
            {   int fillsize = (b->Bscope_index - i) * GUARD_SIZE;
                dtb.nzeros( fillsize);
                sz += fillsize;
            }
            i = b->Bscope_index + 1;

            int nsucc = b->numSucc();

            dtb.dword(b->Blast_index);          // parent index

            if (b->jcatchvar)                           // if try-catch
            {
                assert(catchoffset);
                dtb.dword(catchoffset);
                dtb.size(0);                  // no finally handler

                catchoffset += NPTRSIZE + (nsucc - 1) * (3 * NPTRSIZE);
            }
            else                                        // else try-finally
            {
                assert(nsucc == 2);
                dtb.dword(0);           // no catch offset
                block *bhandler = b->nthSucc(1);
                assert(bhandler->BC == BC_finally);
                // To successor of BC_finally block
                bhandler = bhandler->nthSucc(0);
                // finally handler address
                dtb.coff(bhandler->Boffset);
            }
            sz += GUARD_SIZE;
        }
    }

    /* Append to guard[] the guard blocks for temporaries that are created and destroyed
     * within a single expression. These are marked by the special instruction pairs
     * (ESCAPE | ESCdctor) and (ESCAPE | ESCddtor).
     */
    if (usednteh & EHcleanup)
    {
        #define STACKINC 16
        int stackbuf[STACKINC];
        int *stack = stackbuf;
        int stackmax = STACKINC;

    int scopeindex = guarddim;
    for (block *b = startblock; b; b = b->Bnext)
    {
        /* Set up stack of scope indices
         */
        stack[0] = b->Btry ? b->Btry->Bscope_index : -1;
        int stacki = 1;

        unsigned boffset = b->Boffset;
        for (code *c = b->Bcode; c; c = code_next(c))
        {
            if (c->Iop == (ESCAPE | ESCdctor))
            {
                code *c2 = code_next(c);
                // Find corresponding ddtor instruction
                int n = 0;
                unsigned eoffset = boffset;
                unsigned foffset;
                for (; 1; c2 = code_next(c2))
                {
                    // Bugzilla 13720: optimizer might elide the corresponding ddtor
                    if (!c2)
                        goto Lnodtor;

                    if (c2->Iop == (ESCAPE | ESCddtor))
                    {
                        if (n)
                            n--;
                        else
                        {
                            foffset = eoffset;
                            code *cf = code_next(c2);
                            foffset += calccodsize(cf);
                            while (!cf->isJumpOP())
                            {
                                cf = code_next(cf);
                                foffset += calccodsize(cf);
                            }
                            // issue 9438
                            //cf = code_next(cf);
                            //foffset += calccodsize(cf);
                            break;
                        }
                    }
                    else if (c2->Iop == (ESCAPE | ESCdctor))
                    {
                        n++;
                    }
                    else
                        eoffset += calccodsize(c2);
                }
                //printf("boffset = %x, eoffset = %x, foffset = %x\n", boffset, eoffset, foffset);
                dtb.dword(stack[stacki - 1]);   // parent index
                dtb.dword(0);           // no catch offset
                dtb.coff(foffset);  // finally handler address
                if (stacki == stackmax)
                {   // stack[] is out of space; enlarge it
                    int *pi = (int *)malloc((stackmax + STACKINC) * sizeof(int));
                    assert(pi);
                    memcpy(pi, stack, stackmax * sizeof(int));
                    if (stack != stackbuf)
                        free(stack);
                    stack = pi;
                    stackmax += STACKINC;
                }
                stack[stacki++] = scopeindex;
                ++scopeindex;
                sz += GUARD_SIZE;
            }
            else if (c->Iop == (ESCAPE | ESCddtor))
            {
                stacki--;
                assert(stacki != 0);
            }
        Lnodtor:
            boffset += calccodsize(c);
        }
    }
        if (stack != stackbuf)
            free(stack);
    }

    // Generate catch[]
    for (block *b = startblock; b; b = b->Bnext)
    {
        if (b->BC == BC_try && b->jcatchvar)         // if try-catch
        {
            int nsucc = b->numSucc();
            dtb.size(nsucc - 1);           // # of catch blocks
            sz += NPTRSIZE;

            for (int i = 1; i < nsucc; ++i)
            {
                block *bcatch = b->nthSucc(i);

                dtb.xoff(bcatch->Bcatchtype,0,TYnptr);

                dtb.size(cod3_bpoffset(b->jcatchvar));     // EBP offset

                // catch handler address
                dtb.coff(bcatch->Boffset);

                sz += 3 * NPTRSIZE;
            }
        }
    }
    assert(sz != 0);
    s->Sdt = dtb.finish();
}

