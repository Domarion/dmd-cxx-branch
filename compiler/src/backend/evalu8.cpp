// Copyright (C) 1985-1998 by Symantec
// Copyright (C) 2000-2021 by The D Language Foundation, All Rights Reserved
// http://www.digitalmars.com
// Written by Walter Bright
/*
 * This source file is made available for personal use
 * only. The license is in backendlicense.txt
 * For any other uses, please contact Digital Mars.
 */

#include        <math.h>
#include        <stdlib.h>
#include        <stdio.h>
#include        <string.h>
#include        <float.h>
#include        <time.h>

#define HAVE_FENV_H 1

#if HAVE_FENV_H
#include        <fenv.h>
#endif


#include        "cc.hpp"
#include        "oper.hpp"                /* OPxxxx definitions           */
#include        "global.hpp"
#include        "el.hpp"
#include        "type.hpp"

static char __file__[] = __FILE__;      /* for tassert.h                */
#include        "tassert.hpp"

extern void error(const char *filename, unsigned linnum, unsigned charnum, const char *format, ...);

#if HAVE_FENV_H
    #define HAVE_FLOAT_EXCEPT 1

    static int testFE()
    {
        return fetestexcept(FE_ALL_EXCEPT);
    }

    static void clearFE()
    {
        feclearexcept(FE_ALL_EXCEPT);
    }
#else
    #define HAVE_FLOAT_EXCEPT 0
    static int  testFE() { return 1; }
    static void clearFE() { }
#endif


elem * evalu8(elem *, goal_t);

/* When this !=0, we do constant folding on floating point constants
 * even if they raise overflow, underflow, invalid, etc. exceptions.
 */

static int ignore_exceptions;

/* When this is !=0, we try to fold out OPsizeof expressions.
 */

static int resolve_sizeof;

/**********************
 * Return boolean result of constant elem.
 */

int boolres(elem *e)
{   int b;

    //printf("boolres()\n");
    //elem_print(e);
    elem_debug(e);
//    assert((_status87() & 0x3800) == 0);
    switch (e->Eoper)
    {
        case OPrelconst:
        case OPstring:
            return TRUE;
        case OPconst:
            switch (tybasic(typemask(e)))
            {   case TYchar:
                case TYuchar:
                case TYschar:
                case TYchar16:
                case TYshort:
                case TYushort:
                case TYint:
                case TYuint:
                case TYbool:
                case TYwchar_t:
                case TYenum:
                case TYlong:
                case TYulong:
                case TYdchar:
                case TYllong:
                case TYullong:
                case TYsptr:
                case TYcptr:
                case TYhptr:
                case TYfptr:
                case TYvptr:
                case TYnptr:
                    b = el_tolong(e) != 0;
                    break;
                case TYnref: // reference can't be converted to bool
                    assert(0);
                    break;
                case TYfloat:
                case TYifloat:
                case TYdouble:
                case TYidouble:
                case TYdouble_alias:
                case TYildouble:
                case TYldouble:
                {   targ_ldouble ld = el_toldouble(e);

                    if (isnan((double)ld))
                        b = 1;
                    else
                        b = (ld != 0);
                    break;
                }
                case TYcfloat:
                    if (isnan(e->EV.Vcfloat.re) || isnan(e->EV.Vcfloat.im))
                        b = 1;
                    else
                        b = e->EV.Vcfloat.re != 0 || e->EV.Vcfloat.im != 0;
                    break;
                case TYcdouble:
                case TYdouble2:
                    if (isnan(e->EV.Vcdouble.re) || isnan(e->EV.Vcdouble.im))
                        b = 1;
                    else
                        b = e->EV.Vcdouble.re != 0 || e->EV.Vcdouble.im != 0;
                    break;
                case TYcldouble:
                    if (isnan(e->EV.Vcldouble.re) || isnan(e->EV.Vcldouble.im))
                        b = 1;
                    else
                        b = e->EV.Vcldouble.re != 0 || e->EV.Vcldouble.im != 0;
                    break;
                case TYstruct:  // happens on syntax error of (struct x)0
                    assert(0);
                case TYvoid:    /* happens if we get syntax errors or
                                       on RHS of && || expressions */
                    b = 0;
                    break;

                case TYcent:
                case TYucent:
                case TYschar16:
                case TYuchar16:
                case TYshort8:
                case TYushort8:
                case TYlong4:
                case TYulong4:
                case TYllong2:
                case TYullong2:
                    b = e->EV.Vcent.lsw || e->EV.Vcent.msw;
                    break;

                case TYfloat4:
                {   b = 0;
                    for (size_t i = 0; i < 4; i++)
                    {
                        if (isnan(e->EV.Vfloat4[i]) || e->EV.Vfloat4[i] != 0)
                        {   b = 1;
                            break;
                        }
                    }
                    break;
                }

                default:
#ifdef DEBUG
                    WRTYxx(typemask(e));
#endif
                    assert(0);
            }
            break;
        default:
            assert(0);
    }
    return b;
}

/***************************
 * Return TRUE if expression will always evaluate to TRUE.
 */

int iftrue(elem *e)
{
  while (1)
  {
        assert(e);
        elem_debug(e);
        switch (e->Eoper)
        {       case OPcomma:
                case OPinfo:
                        e = e->E2;
                        break;
                case OPrelconst:
                case OPconst:
                case OPstring:
                        return boolres(e);
                default:
                        return FALSE;
        }
  }
}

/***************************
 * Return TRUE if expression will always evaluate to FALSE.
 */

int iffalse(elem *e)
{
        while (1)
        {       assert(e);
                elem_debug(e);
                switch (e->Eoper)
                {       case OPcomma:
                        case OPinfo:
                                e = e->E2;
                                break;
                        case OPconst:
                                return !boolres(e);
                        //case OPstring:
                        //case OPrelconst:
                        default:
                                return FALSE;
                }
        }
}

/******************************
 * Evaluate a node with only constants as leaves.
 * Return with the result.
 */

elem * evalu8(elem *e, goal_t goal)
{   elem *e1,*e2;
    tym_t tym,tym2,uns;
    unsigned op;
    targ_int i1,i2;
    int i;
    targ_llong l1,l2;
    targ_ldouble d1,d2;
    elem esave;

//    assert((_status87() & 0x3800) == 0);
    assert(e && EOP(e));
    op = e->Eoper;
    elem_debug(e);
    e1 = e->E1;

    //printf("evalu8(): "); elem_print(e);
    elem_debug(e1);
    if (e1->Eoper == OPconst && !tyvector(e1->Ety))
    {
        tym2 = 0;
        e2 = nullptr;
        if (EBIN(e))
        {   e2 = e->E2;
            elem_debug(e2);
            if (e2->Eoper == OPconst && !tyvector(e2->Ety))
            {
                i2 = l2 = el_tolong(e2);
                d2 = el_toldouble(e2);
            }
            else
                return e;
            tym2 = tybasic(typemask(e2));
        }
        else
        {
            tym2 = 0;
            e2 = nullptr;
            i2 = 0;             // not used, but static analyzer complains
            l2 = 0;             // "
            d2 = 0;             // "
        }
        i1 = l1 = el_tolong(e1);
        d1 = el_toldouble(e1);
        tym = tybasic(typemask(e1));    /* type of op is type of left child */

        // Huge pointers are always evaluated at runtime
        if (tym == TYhptr && (l1 != 0 || l2 != 0))
            return e;

        esave = *e;
        clearFE();
    }
    else
        return e;

    /* if left or right leaf is unsigned, this is an unsigned operation */
    uns = tyuns(tym) | tyuns(tym2);

  /*elem_print(e);*/
  /*dbg_printf("x%lx ",l1); WROP(op); dbg_printf("x%lx = ",l2);*/
  i = 0;
  switch (op)
  {
    case OPadd:
        switch (tym)
        {
            case TYfloat:
                switch (tym2)
                {
                    case TYfloat:
                        e->EV.Vfloat = e1->EV.Vfloat + e2->EV.Vfloat;
                        break;
                    case TYifloat:
                        e->EV.Vcfloat.re = e1->EV.Vfloat;
                        e->EV.Vcfloat.im = e2->EV.Vfloat;
                        break;
                    case TYcfloat:
                        e->EV.Vcfloat.re = e1->EV.Vfloat + e2->EV.Vcfloat.re;
                        e->EV.Vcfloat.im = 0             + e2->EV.Vcfloat.im;
                        break;
                    default:
                        assert(0);
                }
                break;
            case TYdouble:
            case TYdouble_alias:
                switch (tym2)
                {
                    case TYdouble:
                    case TYdouble_alias:
                            e->EV.Vdouble = e1->EV.Vdouble + e2->EV.Vdouble;
                        break;
                    case TYidouble:
                        e->EV.Vcdouble.re = e1->EV.Vdouble;
                        e->EV.Vcdouble.im = e2->EV.Vdouble;
                        break;
                    case TYcdouble:
                        e->EV.Vcdouble.re = e1->EV.Vdouble + e2->EV.Vcdouble.re;
                        e->EV.Vcdouble.im = 0              + e2->EV.Vcdouble.im;
                        break;
                    default:
                        assert(0);
                }
                break;
            case TYldouble:
                switch (tym2)
                {
                    case TYldouble:
                        e->EV.Vldouble = d1 + d2;
                        break;
                    case TYildouble:
                        e->EV.Vcldouble.re = d1;
                        e->EV.Vcldouble.im = d2;
                        break;
                    case TYcldouble:
                        e->EV.Vcldouble.re = d1 + e2->EV.Vcldouble.re;
                        e->EV.Vcldouble.im = 0  + e2->EV.Vcldouble.im;
                        break;
                    default:
                        assert(0);
                }
                break;
            case TYifloat:
                switch (tym2)
                {
                    case TYfloat:
                        e->EV.Vcfloat.re = e2->EV.Vfloat;
                        e->EV.Vcfloat.im = e1->EV.Vfloat;
                        break;
                    case TYifloat:
                        e->EV.Vfloat = e1->EV.Vfloat + e2->EV.Vfloat;
                        break;
                    case TYcfloat:
                        e->EV.Vcfloat.re = 0             + e2->EV.Vcfloat.re;
                        e->EV.Vcfloat.im = e1->EV.Vfloat + e2->EV.Vcfloat.im;
                        break;
                    default:
                        assert(0);
                }
                break;
            case TYidouble:
                switch (tym2)
                {
                    case TYdouble:
                        e->EV.Vcdouble.re = e2->EV.Vdouble;
                        e->EV.Vcdouble.im = e1->EV.Vdouble;
                        break;
                    case TYidouble:
                        e->EV.Vdouble = e1->EV.Vdouble + e2->EV.Vdouble;
                        break;
                    case TYcdouble:
                        e->EV.Vcdouble.re = 0              + e2->EV.Vcdouble.re;
                        e->EV.Vcdouble.im = e1->EV.Vdouble + e2->EV.Vcdouble.im;
                        break;
                    default:
                        assert(0);
                }
                break;
            case TYildouble:
                switch (tym2)
                {
                    case TYldouble:
                        e->EV.Vcldouble.re = d2;
                        e->EV.Vcldouble.im = d1;
                        break;
                    case TYildouble:
                        e->EV.Vldouble = d1 + d2;
                        break;
                    case TYcldouble:
                        e->EV.Vcldouble.re = 0  + e2->EV.Vcldouble.re;
                        e->EV.Vcldouble.im = d1 + e2->EV.Vcldouble.im;
                        break;
                    default:
                        assert(0);
                }
                break;
            case TYcfloat:
                switch (tym2)
                {
                    case TYfloat:
                        e->EV.Vcfloat.re = e1->EV.Vcfloat.re + e2->EV.Vfloat;
                        e->EV.Vcfloat.im = e1->EV.Vcfloat.im;
                        break;
                    case TYifloat:
                        e->EV.Vcfloat.re = e1->EV.Vcfloat.re;
                        e->EV.Vcfloat.im = e1->EV.Vcfloat.im + e2->EV.Vfloat;
                        break;
                    case TYcfloat:
                        e->EV.Vcfloat.re = e1->EV.Vcfloat.re + e2->EV.Vcfloat.re;
                        e->EV.Vcfloat.im = e1->EV.Vcfloat.im + e2->EV.Vcfloat.im;
                        break;
                    default:
                        assert(0);
                }
                break;
            case TYcdouble:
                switch (tym2)
                {
                    case TYdouble:
                        e->EV.Vcdouble.re = e1->EV.Vcdouble.re + e2->EV.Vdouble;
                        e->EV.Vcdouble.im = e1->EV.Vcdouble.im;
                        break;
                    case TYidouble:
                        e->EV.Vcdouble.re = e1->EV.Vcdouble.re;
                        e->EV.Vcdouble.im = e1->EV.Vcdouble.im + e2->EV.Vdouble;
                        break;
                    case TYcdouble:
                        e->EV.Vcdouble.re = e1->EV.Vcdouble.re + e2->EV.Vcdouble.re;
                        e->EV.Vcdouble.im = e1->EV.Vcdouble.im + e2->EV.Vcdouble.im;
                        break;
                    default:
                        assert(0);
                }
                break;
            case TYcldouble:
                switch (tym2)
                {
                    case TYldouble:
                        e->EV.Vcldouble.re = e1->EV.Vcldouble.re + d2;
                        e->EV.Vcldouble.im = e1->EV.Vcldouble.im;
                        break;
                    case TYildouble:
                        e->EV.Vcldouble.re = e1->EV.Vcldouble.re;
                        e->EV.Vcldouble.im = e1->EV.Vcldouble.im + d2;
                        break;
                    case TYcldouble:
                        e->EV.Vcldouble.re = e1->EV.Vcldouble.re + e2->EV.Vcldouble.re;
                        e->EV.Vcldouble.im = e1->EV.Vcldouble.im + e2->EV.Vcldouble.im;
                        break;
                    default:
                        assert(0);
                }
                break;

            default:
                if (intsize == 2)
                {   if (tyfv(tym))
                        e->EV.Vlong = (l1 & 0xFFFF0000) |
                            (targ_ushort) ((targ_ushort) l1 + i2);
                    else if (tyfv(tym2))
                        e->EV.Vlong = (l2 & 0xFFFF0000) |
                            (targ_ushort) (i1 + (targ_ushort) l2);
                    else if (tyintegral(tym) || typtr(tym))
                        e->EV.Vllong = l1 + l2;
                    else
                        assert(0);
                }
                else if (tyintegral(tym) || typtr(tym))
                    e->EV.Vllong = l1 + l2;
                else
                    assert(0);
                break;
        }
        break;

    case OPmin:
        switch (tym)
        {
            case TYfloat:
                switch (tym2)
                {
                    case TYfloat:
                        e->EV.Vfloat = e1->EV.Vfloat - e2->EV.Vfloat;
                        break;
                    case TYifloat:
                        e->EV.Vcfloat.re =  e1->EV.Vfloat;
                        e->EV.Vcfloat.im = -e2->EV.Vfloat;
                        break;
                    case TYcfloat:
                        e->EV.Vcfloat.re = e1->EV.Vfloat - e2->EV.Vcfloat.re;
                        e->EV.Vcfloat.im = 0             - e2->EV.Vcfloat.im;
                        break;
                    default:
                        assert(0);
                }
                break;
            case TYdouble:
            case TYdouble_alias:
                switch (tym2)
                {
                    case TYdouble:
                    case TYdouble_alias:
                        e->EV.Vdouble = e1->EV.Vdouble - e2->EV.Vdouble;
                        break;
                    case TYidouble:
                        e->EV.Vcdouble.re =  e1->EV.Vdouble;
                        e->EV.Vcdouble.im = -e2->EV.Vdouble;
                        break;
                    case TYcdouble:
                        e->EV.Vcdouble.re = e1->EV.Vdouble - e2->EV.Vcdouble.re;
                        e->EV.Vcdouble.im = 0              - e2->EV.Vcdouble.im;
                        break;
                    default:
                        assert(0);
                }
                break;
            case TYldouble:
                switch (tym2)
                {
                    case TYldouble:
                        e->EV.Vldouble = d1 - d2;
                        break;
                    case TYildouble:
                        e->EV.Vcldouble.re =  d1;
                        e->EV.Vcldouble.im = -d2;
                        break;
                    case TYcldouble:
                        e->EV.Vcldouble.re = d1 - e2->EV.Vcldouble.re;
                        e->EV.Vcldouble.im = 0  - e2->EV.Vcldouble.im;
                        break;
                    default:
                        assert(0);
                }
                break;
            case TYifloat:
                switch (tym2)
                {
                    case TYfloat:
                        e->EV.Vcfloat.re = -e2->EV.Vfloat;
                        e->EV.Vcfloat.im =  e1->EV.Vfloat;
                        break;
                    case TYifloat:
                        e->EV.Vfloat = e1->EV.Vfloat - e2->EV.Vfloat;
                        break;
                    case TYcfloat:
                        e->EV.Vcfloat.re = 0             - e2->EV.Vcfloat.re;
                        e->EV.Vcfloat.im = e1->EV.Vfloat - e2->EV.Vcfloat.im;
                        break;
                    default:
                        assert(0);
                }
                break;
            case TYidouble:
                switch (tym2)
                {
                    case TYdouble:
                        e->EV.Vcdouble.re = -e2->EV.Vdouble;
                        e->EV.Vcdouble.im =  e1->EV.Vdouble;
                        break;
                    case TYidouble:
                        e->EV.Vdouble = e1->EV.Vdouble - e2->EV.Vdouble;
                        break;
                    case TYcdouble:
                        e->EV.Vcdouble.re = 0              - e2->EV.Vcdouble.re;
                        e->EV.Vcdouble.im = e1->EV.Vdouble - e2->EV.Vcdouble.im;
                        break;
                    default:
                        assert(0);
                }
                break;
            case TYildouble:
                switch (tym2)
                {
                    case TYldouble:
                        e->EV.Vcldouble.re = -d2;
                        e->EV.Vcldouble.im =  d1;
                        break;
                    case TYildouble:
                        e->EV.Vldouble = d1 - d2;
                        break;
                    case TYcldouble:
                        e->EV.Vcldouble.re = 0  - e2->EV.Vcldouble.re;
                        e->EV.Vcldouble.im = d1 - e2->EV.Vcldouble.im;
                        break;
                    default:
                        assert(0);
                }
                break;
            case TYcfloat:
                switch (tym2)
                {
                    case TYfloat:
                        e->EV.Vcfloat.re = e1->EV.Vcfloat.re - e2->EV.Vfloat;
                        e->EV.Vcfloat.im = e1->EV.Vcfloat.im;
                        break;
                    case TYifloat:
                        e->EV.Vcfloat.re = e1->EV.Vcfloat.re;
                        e->EV.Vcfloat.im = e1->EV.Vcfloat.im - e2->EV.Vfloat;
                        break;
                    case TYcfloat:
                        e->EV.Vcfloat.re = e1->EV.Vcfloat.re - e2->EV.Vcfloat.re;
                        e->EV.Vcfloat.im = e1->EV.Vcfloat.im - e2->EV.Vcfloat.im;
                        break;
                    default:
                        assert(0);
                }
                break;
            case TYcdouble:
                switch (tym2)
                {
                    case TYdouble:
                        e->EV.Vcdouble.re = e1->EV.Vcdouble.re - e2->EV.Vdouble;
                        e->EV.Vcdouble.im = e1->EV.Vcdouble.im;
                        break;
                    case TYidouble:
                        e->EV.Vcdouble.re = e1->EV.Vcdouble.re;
                        e->EV.Vcdouble.im = e1->EV.Vcdouble.im - e2->EV.Vdouble;
                        break;
                    case TYcdouble:
                        e->EV.Vcdouble.re = e1->EV.Vcdouble.re - e2->EV.Vcdouble.re;
                        e->EV.Vcdouble.im = e1->EV.Vcdouble.im - e2->EV.Vcdouble.im;
                        break;
                    default:
                        assert(0);
                }
                break;
            case TYcldouble:
                switch (tym2)
                {
                    case TYldouble:
                        e->EV.Vcldouble.re = e1->EV.Vcldouble.re - d2;
                        e->EV.Vcldouble.im = e1->EV.Vcldouble.im;
                        break;
                    case TYildouble:
                        e->EV.Vcldouble.re = e1->EV.Vcldouble.re;
                        e->EV.Vcldouble.im = e1->EV.Vcldouble.im - d2;
                        break;
                    case TYcldouble:
                        e->EV.Vcldouble.re = e1->EV.Vcldouble.re - e2->EV.Vcldouble.re;
                        e->EV.Vcldouble.im = e1->EV.Vcldouble.im - e2->EV.Vcldouble.im;
                        break;
                    default:
                        assert(0);
                }
                break;

            default:
                if (intsize == 2 &&
                    tyfv(tym) && tysize[tym2] == 2)
                    e->EV.Vllong = (l1 & 0xFFFF0000) |
                        (targ_ushort) ((targ_ushort) l1 - i2);
                else if (tyintegral(tym) || typtr(tym))
                    e->EV.Vllong = l1 - l2;
                else
                    assert(0);
                break;
        }
        break;
    case OPmul:
        if (tyintegral(tym) || typtr(tym))
            e->EV.Vllong = l1 * l2;
        else
        {   switch (tym)
            {
                case TYfloat:
                    switch (tym2)
                    {
                        case TYfloat:
                        case TYifloat:
                            e->EV.Vfloat = e1->EV.Vfloat * e2->EV.Vfloat;
                            break;
                        case TYcfloat:
                            e->EV.Vcfloat.re = e1->EV.Vfloat * e2->EV.Vcfloat.re;
                            e->EV.Vcfloat.im = e1->EV.Vfloat * e2->EV.Vcfloat.im;
                            break;
                        default:
                            assert(0);
                    }
                    break;
                case TYdouble:
                case TYdouble_alias:
                    switch (tym2)
                    {
                        case TYdouble:
                        case TYdouble_alias:
                        case TYidouble:
                            e->EV.Vdouble = e1->EV.Vdouble * e2->EV.Vdouble;
                            break;
                        case TYcdouble:
                            e->EV.Vcdouble.re = e1->EV.Vdouble * e2->EV.Vcdouble.re;
                            e->EV.Vcdouble.im = e1->EV.Vdouble * e2->EV.Vcdouble.im;
                            break;
                        default:
                            assert(0);
                    }
                    break;
                case TYldouble:
                    switch (tym2)
                    {
                        case TYldouble:
                        case TYildouble:
                            e->EV.Vldouble = d1 * d2;
                            break;
                        case TYcldouble:
                            e->EV.Vcldouble.re = d1 * e2->EV.Vcldouble.re;
                            e->EV.Vcldouble.im = d1 * e2->EV.Vcldouble.im;
                            break;
                        default:
                            assert(0);
                    }
                    break;
                case TYifloat:
                    switch (tym2)
                    {
                        case TYfloat:
                            e->EV.Vfloat = e1->EV.Vfloat * e2->EV.Vfloat;
                            break;
                        case TYifloat:
                            e->EV.Vfloat = -e1->EV.Vfloat * e2->EV.Vfloat;
                            break;
                        case TYcfloat:
                            e->EV.Vcfloat.re = -e1->EV.Vfloat * e2->EV.Vcfloat.im;
                            e->EV.Vcfloat.im =  e1->EV.Vfloat * e2->EV.Vcfloat.re;
                            break;
                        default:
                            assert(0);
                    }
                    break;
                case TYidouble:
                    switch (tym2)
                    {
                        case TYdouble:
                            e->EV.Vdouble = e1->EV.Vdouble * e2->EV.Vdouble;
                            break;
                        case TYidouble:
                            e->EV.Vdouble = -e1->EV.Vdouble * e2->EV.Vdouble;
                            break;
                        case TYcdouble:
                            e->EV.Vcdouble.re = -e1->EV.Vdouble * e2->EV.Vcdouble.im;
                            e->EV.Vcdouble.im =  e1->EV.Vdouble * e2->EV.Vcdouble.re;
                            break;
                        default:
                            assert(0);
                    }
                    break;
                case TYildouble:
                    switch (tym2)
                    {
                        case TYldouble:
                            e->EV.Vldouble = d1 * d2;
                            break;
                        case TYildouble:
                            e->EV.Vldouble = -d1 * d2;
                            break;
                        case TYcldouble:
                            e->EV.Vcldouble.re = -d1 * e2->EV.Vcldouble.im;
                            e->EV.Vcldouble.im =  d1 * e2->EV.Vcldouble.re;
                            break;
                        default:
                            assert(0);
                    }
                    break;
                case TYcfloat:
                    switch (tym2)
                    {
                        case TYfloat:
                            e->EV.Vcfloat.re = e1->EV.Vcfloat.re * e2->EV.Vfloat;
                            e->EV.Vcfloat.im = e1->EV.Vcfloat.im * e2->EV.Vfloat;
                            break;
                        case TYifloat:
                            e->EV.Vcfloat.re = -e1->EV.Vcfloat.im * e2->EV.Vfloat;
                            e->EV.Vcfloat.im =  e1->EV.Vcfloat.re * e2->EV.Vfloat;
                            break;
                        case TYcfloat:
                            e->EV.Vcfloat = Complex_f::mul(e1->EV.Vcfloat, e2->EV.Vcfloat);
                            break;
                        default:
                            assert(0);
                    }
                    break;
                case TYcdouble:
                    switch (tym2)
                    {
                        case TYdouble:
                            e->EV.Vcdouble.re = e1->EV.Vcdouble.re * e2->EV.Vdouble;
                            e->EV.Vcdouble.im = e1->EV.Vcdouble.im * e2->EV.Vdouble;
                            break;
                        case TYidouble:
                            e->EV.Vcdouble.re = -e1->EV.Vcdouble.im * e2->EV.Vdouble;
                            e->EV.Vcdouble.im =  e1->EV.Vcdouble.re * e2->EV.Vdouble;
                            break;
                        case TYcdouble:
                            e->EV.Vcdouble = Complex_d::mul(e1->EV.Vcdouble, e2->EV.Vcdouble);
                            break;
                        default:
                            assert(0);
                    }
                    break;
                case TYcldouble:
                    switch (tym2)
                    {
                        case TYldouble:
                            e->EV.Vcldouble.re = e1->EV.Vcldouble.re * d2;
                            e->EV.Vcldouble.im = e1->EV.Vcldouble.im * d2;
                            break;
                        case TYildouble:
                            e->EV.Vcldouble.re = -e1->EV.Vcldouble.im * d2;
                            e->EV.Vcldouble.im =  e1->EV.Vcldouble.re * d2;
                            break;
                        case TYcldouble:
                            e->EV.Vcldouble = Complex_ld::mul(e1->EV.Vcldouble, e2->EV.Vcldouble);
                            break;
                        default:
                            assert(0);
                    }
                    break;
                default:
                    assert(0);
            }
        }
        break;
    case OPdiv:
        if (!boolres(e2))                       // divide by 0
        {
            if (!tyfloating(tym))
                goto div0;
        }
        if (uns)
            e->EV.Vullong = ((targ_ullong) l1) / ((targ_ullong) l2);
        else
        {   switch (tym)
            {
                case TYfloat:
                    switch (tym2)
                    {
                        case TYfloat:
                            e->EV.Vfloat = e1->EV.Vfloat / e2->EV.Vfloat;
                            break;
                        case TYifloat:
                            e->EV.Vfloat = -e1->EV.Vfloat / e2->EV.Vfloat;
                            break;
                        case TYcfloat:
                            e->EV.Vcfloat.re = d1;
                            e->EV.Vcfloat.im = 0;
                            e->EV.Vcfloat = Complex_f::div(e->EV.Vcfloat, e2->EV.Vcfloat);
                            break;
                        default:
                            assert(0);
                    }
                    break;
                case TYdouble:
                case TYdouble_alias:
                    switch (tym2)
                    {
                        case TYdouble:
                        case TYdouble_alias:
                            e->EV.Vdouble = e1->EV.Vdouble / e2->EV.Vdouble;
                            break;
                        case TYidouble:
                            e->EV.Vdouble = -e1->EV.Vdouble / e2->EV.Vdouble;
                            break;
                        case TYcdouble:
                            e->EV.Vcdouble.re = d1;
                            e->EV.Vcdouble.im = 0;
                            e->EV.Vcdouble = Complex_d::div(e->EV.Vcdouble, e2->EV.Vcdouble);
                            break;
                        default:
                            assert(0);
                    }
                    break;
                case TYldouble:
                    switch (tym2)
                    {
                        case TYldouble:
                            e->EV.Vldouble = d1 / d2;
                            break;
                        case TYildouble:
                            e->EV.Vldouble = -d1 / d2;
                            break;
                        case TYcldouble:
                            e->EV.Vcldouble.re = d1;
                            e->EV.Vcldouble.im = 0;
                            e->EV.Vcldouble = Complex_ld::div(e->EV.Vcldouble, e2->EV.Vcldouble);
                            break;
                        default:
                            assert(0);
                    }
                    break;
                case TYifloat:
                    switch (tym2)
                    {
                        case TYfloat:
                        case TYifloat:
                            e->EV.Vfloat = e1->EV.Vfloat / e2->EV.Vfloat;
                            break;
                        case TYcfloat:
                            e->EV.Vcfloat.re = 0;
                            e->EV.Vcfloat.im = e1->EV.Vfloat;
                            e->EV.Vcfloat = Complex_f::div(e->EV.Vcfloat, e2->EV.Vcfloat);
                            break;
                        default:
                            assert(0);
                    }
                    break;
                case TYidouble:
                    switch (tym2)
                    {
                        case TYdouble:
                        case TYidouble:
                            e->EV.Vdouble = e1->EV.Vdouble / e2->EV.Vdouble;
                            break;
                        case TYcdouble:
                            e->EV.Vcdouble.re = 0;
                            e->EV.Vcdouble.im = e1->EV.Vdouble;
                            e->EV.Vcdouble = Complex_d::div(e->EV.Vcdouble, e2->EV.Vcdouble);
                            break;
                        default:
                            assert(0);
                    }
                    break;
                case TYildouble:
                    switch (tym2)
                    {
                        case TYldouble:
                        case TYildouble:
                            e->EV.Vldouble = d1 / d2;
                            break;
                        case TYcldouble:
                            e->EV.Vcldouble.re = 0;
                            e->EV.Vcldouble.im = d1;
                            e->EV.Vcldouble = Complex_ld::div(e->EV.Vcldouble, e2->EV.Vcldouble);
                            break;
                        default:
                            assert(0);
                    }
                    break;
                case TYcfloat:
                    switch (tym2)
                    {
                        case TYfloat:
                            e->EV.Vcfloat.re = e1->EV.Vcfloat.re / e2->EV.Vfloat;
                            e->EV.Vcfloat.im = e1->EV.Vcfloat.im / e2->EV.Vfloat;
                            break;
                        case TYifloat:
                            e->EV.Vcfloat.re =  e1->EV.Vcfloat.im / e2->EV.Vfloat;
                            e->EV.Vcfloat.im = -e1->EV.Vcfloat.re / e2->EV.Vfloat;
                            break;
                        case TYcfloat:
                            e->EV.Vcfloat = Complex_f::div(e1->EV.Vcfloat, e2->EV.Vcfloat);
                            break;
                        default:
                            assert(0);
                    }
                    break;
                case TYcdouble:
                    switch (tym2)
                    {
                        case TYdouble:
                            e->EV.Vcdouble.re = e1->EV.Vcdouble.re / e2->EV.Vdouble;
                            e->EV.Vcdouble.im = e1->EV.Vcdouble.im / e2->EV.Vdouble;
                            break;
                        case TYidouble:
                            e->EV.Vcdouble.re =  e1->EV.Vcdouble.im / e2->EV.Vdouble;
                            e->EV.Vcdouble.im = -e1->EV.Vcdouble.re / e2->EV.Vdouble;
                            break;
                        case TYcdouble:
                            e->EV.Vcdouble = Complex_d::div(e1->EV.Vcdouble, e2->EV.Vcdouble);
                            break;
                        default:
                            assert(0);
                    }
                    break;
                case TYcldouble:
                    switch (tym2)
                    {
                        case TYldouble:
                            e->EV.Vcldouble.re = e1->EV.Vcldouble.re / d2;
                            e->EV.Vcldouble.im = e1->EV.Vcldouble.im / d2;
                            break;
                        case TYildouble:
                            e->EV.Vcldouble.re =  e1->EV.Vcldouble.im / d2;
                            e->EV.Vcldouble.im = -e1->EV.Vcldouble.re / d2;
                            break;
                        case TYcldouble:
                            e->EV.Vcldouble = Complex_ld::div(e1->EV.Vcldouble, e2->EV.Vcldouble);
                            break;
                        default:
                            assert(0);
                    }
                    break;
                default:
                    e->EV.Vllong = l1 / l2;
                    break;
            }
        }
        break;
    case OPmod:

        if (!tyfloating(tym))
        {
            if (!boolres(e2))
            {
                div0:
                    error(e->Esrcpos.Sfilename, e->Esrcpos.Slinnum, e->Esrcpos.Scharnum, "divide by zero");
                    break;
            }
        }
        if (uns)
            e->EV.Vullong = ((targ_ullong) l1) % ((targ_ullong) l2);
        else
        {
            // BUG: what do we do for imaginary, complex?
            switch (tym)
            {   case TYdouble:
                case TYidouble:
                case TYdouble_alias:
                    e->EV.Vdouble = fmod(e1->EV.Vdouble,e2->EV.Vdouble);
                    break;
                case TYfloat:
                case TYifloat:
                    e->EV.Vfloat = fmodf(e1->EV.Vfloat,e2->EV.Vfloat);
                    break;
                case TYldouble:
                case TYildouble:
                    e->EV.Vldouble = fmodl(d1, d2);
                    break;
                case TYcfloat:
                    switch (tym2)
                    {
                        case TYfloat:
                        case TYifloat:
                            e->EV.Vcfloat.re = fmodf(e1->EV.Vcfloat.re, e2->EV.Vfloat);
                            e->EV.Vcfloat.im = fmodf(e1->EV.Vcfloat.im, e2->EV.Vfloat);
                            break;
                        default:
                            assert(0);
                    }
                    break;
                case TYcdouble:
                    switch (tym2)
                    {
                        case TYdouble:
                        case TYidouble:
                            e->EV.Vcdouble.re = fmod(e1->EV.Vcdouble.re, e2->EV.Vdouble);
                            e->EV.Vcdouble.im = fmod(e1->EV.Vcdouble.im, e2->EV.Vdouble);
                            break;
                        default:
                            assert(0);
                    }
                    break;
                case TYcldouble:
                    switch (tym2)
                    {
                        case TYldouble:
                        case TYildouble:
                            e->EV.Vcldouble.re = fmodl(e1->EV.Vcldouble.re, d2);
                            e->EV.Vcldouble.im = fmodl(e1->EV.Vcldouble.im, d2);
                            break;
                        default:
                            assert(0);
                    }
                    break;
                default:
                    e->EV.Vllong = l1 % l2;
                    break;
            }
        }
        break;
    case OPremquo:
    {
        targ_llong rem, quo;

        assert(!tyfloating(tym));
        if (!boolres(e2))
            goto div0;
        if (uns)
        {
            rem = ((targ_ullong) l1) % ((targ_ullong) l2);
            quo = ((targ_ullong) l1) / ((targ_ullong) l2);
        }
        else
        {
            rem = l1 % l2;
            quo = l1 / l2;
        }
        switch (tysize(tym))
        {
            case 2:
                e->EV.Vllong = (rem << 16) | (quo & 0xFFFF);
                break;
            case 4:
                e->EV.Vllong = (rem << 32) | (quo & 0xFFFFFFFF);
                break;
            case 8:
                e->EV.Vcent.lsw = quo;
                e->EV.Vcent.msw = rem;
                break;
            default:
                assert(0);
                break;
        }
        break;
    }
    case OPand:
        e->EV.Vllong = l1 & l2;
        break;
    case OPor:
        e->EV.Vllong = l1 | l2;
        break;
    case OPxor:
        e->EV.Vllong = l1 ^ l2;
        break;
    case OPnot:
        e->EV.Vint = boolres(e1) ^ TRUE;
        break;
    case OPcom:
        e->EV.Vllong = ~l1;
        break;
    case OPcomma:
        e->EV = e2->EV;
        break;
    case OPoror:
        e->EV.Vint = boolres(e1) || boolres(e2);
        break;
    case OPandand:
        e->EV.Vint = boolres(e1) && boolres(e2);
        break;
    case OPshl:
        if ((targ_ullong) i2 < sizeof(targ_ullong) * 8)
            e->EV.Vllong = l1 << i2;
        else
            e->EV.Vllong = 0;
        break;
    case OPshr:
        if ((targ_ullong) i2 > sizeof(targ_ullong) * 8)
            i2 = sizeof(targ_ullong) * 8;

        // Always unsigned
        e->EV.Vullong = ((targ_ullong) l1) >> i2;
        break;

    case OPbtst:
        if ((targ_ullong) i2 > sizeof(targ_ullong) * 8)
            i2 = sizeof(targ_ullong) * 8;
        e->EV.Vullong = (((targ_ullong) l1) >> i2) & 1;
        break;

    case OPashr:
        if ((targ_ullong) i2 > sizeof(targ_ullong) * 8)
            i2 = sizeof(targ_ullong) * 8;
        // Always signed
        e->EV.Vllong = l1 >> i2;
        break;

    case OPpair:
        switch (tysize[tym])
        {
            case 2:
                e->EV.Vlong = (i2 << 16) | (i1 & 0xFFFF);
                break;
            case 4:
                e->EV.Vllong = (l2 << 32) | (l1 & 0xFFFFFFFF);
                break;
            case 8:
                e->EV.Vcent.lsw = l1;
                e->EV.Vcent.msw = l2;
                break;
            default:
                assert(0);
        }
        break;

    case OPneg:
        // Avoid converting NANS to NAN
        memcpy(&e->EV.Vcldouble,&e1->EV.Vcldouble,sizeof(e->EV.Vcldouble));
        switch (tym)
        {   case TYdouble:
            case TYidouble:
            case TYdouble_alias:
                e->EV.Vdouble = -e->EV.Vdouble;
                break;
            case TYfloat:
            case TYifloat:
                e->EV.Vfloat = -e->EV.Vfloat;
                break;
            case TYldouble:
            case TYildouble:
                e->EV.Vldouble = -e->EV.Vldouble;
                break;
            case TYcfloat:
                e->EV.Vcfloat.re = -e->EV.Vcfloat.re;
                e->EV.Vcfloat.im = -e->EV.Vcfloat.im;
                break;
            case TYcdouble:
                e->EV.Vcdouble.re = -e->EV.Vcdouble.re;
                e->EV.Vcdouble.im = -e->EV.Vcdouble.im;
                break;
            case TYcldouble:
                e->EV.Vcldouble.re = -e->EV.Vcldouble.re;
                e->EV.Vcldouble.im = -e->EV.Vcldouble.im;
                break;
            default:
                e->EV.Vllong = -l1;
                break;
        }
        break;
    case OPabs:
        switch (tym)
        {
            case TYdouble:
            case TYidouble:
            case TYdouble_alias:
                e->EV.Vdouble = fabs(e1->EV.Vdouble);
                break;
            case TYfloat:
            case TYifloat:
                e->EV.Vfloat = fabs(e1->EV.Vfloat);
                break;
            case TYldouble:
            case TYildouble:
                e->EV.Vldouble = fabsl(d1);
                break;
            case TYcfloat:
                e->EV.Vfloat = Complex_f::abs(e1->EV.Vcfloat);
                break;
            case TYcdouble:
                e->EV.Vdouble = Complex_d::abs(e1->EV.Vcdouble);
                break;
            case TYcldouble:
                e->EV.Vldouble = Complex_ld::abs(e1->EV.Vcldouble);
                break;
            default:
                e->EV.Vllong = labs(l1);
                break;
        }
        break;
#if TX86
    case OPsqrt:
    case OPsin:
    case OPcos:
#endif
    case OPrndtol:
    case OPrint:
        return e;
    case OPngt:
        i++;
    case OPgt:
        if (!tyfloating(tym))
            goto Lnle;
        i ^= (int)(d1 > d2);
        e->EV.Vint = i;
        break;

    case OPnle:
    Lnle:
        i++;
    case OPle:
        if (uns)
        {
            i ^= (int)(((targ_ullong) l1) <= ((targ_ullong) l2));
        }
        else
        {
            if (tyfloating(tym))
                i ^= (int)(d1 <= d2);
            else
                i ^= (int)(l1 <= l2);
        }
        e->EV.Vint = i;
        break;

    case OPnge:
        i++;
    case OPge:
        if (!tyfloating(tym))
            goto Lnlt;
        i ^= (int)(d1 >= d2);
        e->EV.Vint = i;
        break;

    case OPnlt:
    Lnlt:
        i++;
    case OPlt:
        if (uns)
        {
            i ^= (int)(((targ_ullong) l1) < ((targ_ullong) l2));
        }
        else
        {
            if (tyfloating(tym))
                i ^= (int)(d1 < d2);
            else
                i ^= (int)(l1 < l2);
        }
        e->EV.Vint = i;
        break;

    case OPne:
        i++;
    case OPeqeq:
        if (tyfloating(tym))
        {
            switch (tybasic(tym))
            {
                case TYcfloat:
                    if (isnan(e1->EV.Vcfloat.re) || isnan(e1->EV.Vcfloat.im) ||
                        isnan(e2->EV.Vcfloat.re) || isnan(e2->EV.Vcfloat.im))
                        ;
                    else
                        i ^= (int)((e1->EV.Vcfloat.re == e2->EV.Vcfloat.re) &&
                                   (e1->EV.Vcfloat.im == e2->EV.Vcfloat.im));
                    break;
                case TYcdouble:
                    if (isnan(e1->EV.Vcdouble.re) || isnan(e1->EV.Vcdouble.im) ||
                        isnan(e2->EV.Vcdouble.re) || isnan(e2->EV.Vcdouble.im))
                        ;
                    else
                        i ^= (int)((e1->EV.Vcdouble.re == e2->EV.Vcdouble.re) &&
                                   (e1->EV.Vcdouble.im == e2->EV.Vcdouble.im));
                    break;
                case TYcldouble:
                    if (isnan(e1->EV.Vcldouble.re) || isnan(e1->EV.Vcldouble.im) ||
                        isnan(e2->EV.Vcldouble.re) || isnan(e2->EV.Vcldouble.im))
                        ;
                    else
                        i ^= (int)((e1->EV.Vcldouble.re == e2->EV.Vcldouble.re) &&
                                   (e1->EV.Vcldouble.im == e2->EV.Vcldouble.im));
                    break;
                default:
                    i ^= (int)(d1 == d2);
                    break;
            }
            //printf("%Lg + %Lgi, %Lg + %Lgi\n", e1->EV.Vcldouble.re, e1->EV.Vcldouble.im, e2->EV.Vcldouble.re, e2->EV.Vcldouble.im);
        }
        else
            i ^= (int)(l1 == l2);
        e->EV.Vint = i;
        break;

    case OPs16_32:
        e->EV.Vlong = (targ_short) i1;
        break;
    case OPnp_fp:
    case OPu16_32:
        e->EV.Vulong = (targ_ushort) i1;
        break;
    case OPd_u32:
        e->EV.Vulong = (targ_ulong)d1;
        //printf("OPd_u32: dbl = %g, ulng = x%lx\n",d1,e->EV.Vulong);
        break;
    case OPd_s32:
        e->EV.Vlong = (targ_long)d1;
        break;
    case OPu32_d:
        e->EV.Vdouble = (unsigned) l1;
        break;
    case OPs32_d:
        e->EV.Vdouble = (int) l1;
        break;
    case OPd_s16:
        e->EV.Vint = (targ_int)d1;
        break;
    case OPs16_d:
        e->EV.Vdouble = (targ_short) i1;
        break;
    case OPd_u16:
        e->EV.Vushort = (targ_ushort)d1;
        break;
    case OPu16_d:
        e->EV.Vdouble = (targ_ushort) i1;
        break;
    case OPd_s64:
        e->EV.Vllong = (targ_llong)d1;
        break;
    case OPd_u64:
    case OPld_u64:
        e->EV.Vullong = (targ_ullong)d1;
        break;
    case OPs64_d:
        e->EV.Vdouble = l1;
        break;
    case OPu64_d:
        e->EV.Vdouble = (targ_ullong) l1;
        break;
    case OPd_f:
        //assert((_status87() & 0x3800) == 0);
        e->EV.Vfloat = e1->EV.Vdouble;
        if (tycomplex(tym))
            e->EV.Vcfloat.im = e1->EV.Vcdouble.im;
        //assert((_status87() & 0x3800) == 0);
        break;
    case OPf_d:
        e->EV.Vdouble = e1->EV.Vfloat;
        if (tycomplex(tym))
            e->EV.Vcdouble.im = e1->EV.Vcfloat.im;
        break;
    case OPd_ld:
        e->EV.Vldouble = e1->EV.Vdouble;
        if (tycomplex(tym))
            e->EV.Vcldouble.im = e1->EV.Vcdouble.im;
        break;
    case OPld_d:
        e->EV.Vdouble = e1->EV.Vldouble;
        if (tycomplex(tym))
            e->EV.Vcdouble.im = e1->EV.Vcldouble.im;
        break;
    case OPc_r:
        e->EV = e1->EV;
        break;
    case OPc_i:
        switch (tym)
        {
            case TYcfloat:
                e->EV.Vfloat = e1->EV.Vcfloat.im;
                break;
            case TYcdouble:
                e->EV.Vdouble = e1->EV.Vcdouble.im;
                break;
            case TYcldouble:
                e->EV.Vldouble = e1->EV.Vcldouble.im;
                break;
            default:
                assert(0);
        }
        break;
    case OPs8_16:
        e->EV.Vint = (targ_schar) i1;
        break;
    case OPu8_16:
        e->EV.Vint = i1 & 0xFF;
        break;
    case OP16_8:
        e->EV.Vint = i1;
        break;
    case OPbool:
        e->EV.Vint = boolres(e1);
        break;
    case OP32_16:
    case OPoffset:
        e->EV.Vint = l1;
        break;

    case OP64_32:
        e->EV.Vlong = l1;
        break;
    case OPs32_64:
        e->EV.Vllong = (targ_long) l1;
        break;
    case OPu32_64:
        e->EV.Vllong = (targ_ulong) l1;
        break;

    case OP128_64:
        e->EV.Vllong = e1->EV.Vcent.lsw;
        break;
    case OPs64_128:
        e->EV.Vcent.lsw = e1->EV.Vllong;
        e->EV.Vcent.msw = 0;
        if ((targ_llong)e->EV.Vcent.lsw < 0)
            e->EV.Vcent.msw = ~(targ_ullong)0;
        break;
    case OPu64_128:
        e->EV.Vcent.lsw = e1->EV.Vullong;
        e->EV.Vcent.msw = 0;
        break;

    case OPmsw:
        switch (tysize(tym))
        {
            case 4:
                e->EV.Vllong = (l1 >> 16) & 0xFFFF;
                break;
            case 8:
                e->EV.Vllong = (l1 >> 32) & 0xFFFFFFFF;
                break;
            case 16:
                e->EV.Vllong = e1->EV.Vcent.msw;
                break;
            default:
                assert(0);
        }
        break;
    case OPb_8:
        e->EV.Vlong = i1 & 1;
        break;
    case OPbswap:
        if (tysize(tym) == 2)
        {
            e->EV.Vint = ((i1 >> 8) & 0x00FF) |
                         ((i1 << 8) & 0xFF00);
        }
        else if (tysize(tym) == 4)
        {
            e->EV.Vint = ((i1 >> 24) & 0x000000FF) |
                         ((i1 >>  8) & 0x0000FF00) |
                         ((i1 <<  8) & 0x00FF0000) |
                         ((i1 << 24) & 0xFF000000);
        }
        else
        {
            targ_llong n = l1;
            n = (n & 0x00000000FFFFFFFF) << 32 | (n & 0xFFFFFFFF00000000) >> 32;
            n = (n & 0x0000FFFF0000FFFF) << 16 | (n & 0xFFFF0000FFFF0000) >> 16;
            n = (n & 0x00FF00FF00FF00FF) << 8  | (n & 0xFF00FF00FF00FF00) >> 8;
            e->EV.Vllong = n;
        }
        break;

    case OPpopcnt:
    {
        // Eliminate any unwanted sign extension
        switch (tysize(tym))
        {
            case 1:     l1 &= 0xFF;       break;
            case 2:     l1 &= 0xFFFF;     break;
            case 4:     l1 &= 0xFFFFFFFF; break;
            case 8:     break;
            default:    assert(0);
        }

        int popcnt = 0;
        while (l1)
        {   // Not efficient, but don't need efficiency here
            popcnt += (l1 & 1);
            l1 = (targ_ullong)l1 >> 1;  // shift is unsigned
        }
        e->EV.Vllong = popcnt;
        break;
    }

    case OProl:
    case OPror:
    {   unsigned n = i2;
        if (op == OPror)
            n = -n;
        switch (tysize(tym))
        {
            case 1:
                n &= 7;
                e->EV.Vuchar = (unsigned char)((i1 << n) | ((i1 & 0xFF) >> (8 - n)));
                break;
            case 2:
                n &= 0xF;
                e->EV.Vushort = (targ_ushort)((i1 << n) | ((i1 & 0xFFFF) >> (16 - n)));
                break;
            case 4:
                n &= 0x1F;
                e->EV.Vulong = (targ_ulong)((i1 << n) | ((i1 & 0xFFFFFFFF) >> (32 - n)));
                break;
            case 8:
                n &= 0x3F;
                e->EV.Vullong = (targ_ullong)((l1 << n) | ((l1 & 0xFFFFFFFFFFFFFFFFLL) >> (64 - n)));
                break;
            //case 16:
            default:
                assert(0);
        }
        break;
    }
    case OPind:
        return e;

    case OPvecfill:
        switch (tybasic(e->Ety))
        {
            case TYfloat4:
                for (int i = 0; i < 4; ++i)
                    e->EV.Vfloat4[i] = e1->EV.Vfloat;
                break;
            case TYdouble2:
                for (int i = 0; i < 2; ++i)
                    e->EV.Vdouble2[i] = e1->EV.Vdouble;
                break;
            case TYschar16:
            case TYuchar16:
                for (int i = 0; i < 16; ++i)
                    ((targ_uchar *)&e->EV.Vcent)[i] = (targ_uchar)i1;
                break;
            case TYshort8:
            case TYushort8:
                for (int i = 0; i < 8; ++i)
                    ((targ_ushort *)&e->EV.Vcent)[i] = (targ_ushort)i1;
                break;
            case TYlong4:
            case TYulong4:
                for (int i = 0; i < 4; ++i)
                    ((targ_ulong *)&e->EV.Vcent)[i] = (targ_ulong)i1;
                break;
            case TYllong2:
            case TYullong2:
                for (int i = 0; i < 2; ++i)
                    ((targ_ullong *)&e->EV.Vcent)[i] = (targ_ullong)l1;
                break;
            default:
                assert(0);
        }
        break;

    default:
        return e;
  }
#if TX86
    int flags;

    if (!ignore_exceptions &&
        (config.flags4 & CFG4fastfloat) == 0 && testFE() &&
        (HAVE_FLOAT_EXCEPT || tyfloating(tym) || tyfloating(tybasic(typemask(e))))
       )
    {
        // Exceptions happened. Do not fold the constants.
        *e = esave;
        return e;
    }
#endif

  /*dbg_printf("result = x%lx\n",e->EV.Vlong);*/
  e->Eoper = OPconst;
  el_free(e1);
  if (e2)
        el_free(e2);
#if !__GNUC__
  //printf("2: %x\n", _status87());
  assert((_status87() & 0x3800) == 0);
#endif
  //printf("evalu8() returns: "); elem_print(e);
  return e;
}
