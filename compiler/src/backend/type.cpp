// Copyright (C) 1985-1998 by Symantec
// Copyright (C) 2000-2021 by The D Language Foundation, All Rights Reserved
// http://www.digitalmars.com
// Written by Walter Bright
/*
 * This source file is made available for personal use
 * only. The license is in backendlicense.txt
 * For any other uses, please contact Digital Mars.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cc.hpp"
#include "global.hpp"
#include "type.hpp"
#include "el.hpp"

#undef MEM_PH_MALLOC
#define MEM_PH_MALLOC mem_fmalloc

static char __file__[] = __FILE__;      /* for tassert.h                */
#include        "tassert.hpp"

static type *type_list = nullptr;          // free list of types
static param_t *param_list = nullptr;      // free list of params

static int type_num,type_max;   /* gather statistics on # of types      */

typep_t tstypes[TYMAX];
typep_t tsptr2types[TYMAX];

typep_t tstrace,tsclib,tsjlib,tsdlib,
        tslogical;
typep_t tspvoid,tspcvoid;
typep_t tsptrdiff, tssize;

/*******************************
 * Compute size of type in bytes.
 */

targ_size_t type_size(type *t)
{   targ_size_t s;
    tym_t tyb;

    type_debug(t);
    tyb = tybasic(t->Tty);
#ifdef DEBUG
    if (tyb >= TYMAX)
        /*type_print(t),*/
        dbg_printf("tyb = x%lx\n",(long)tyb);
#endif
    assert(tyb < TYMAX);
    s = tysize[tyb];
    if (s == (targ_size_t) -1)
    {
        switch (tyb)
        {
            // in case program plays games with function pointers
            case TYffunc:
            case TYfpfunc:
            case TYfsfunc:
            case TYf16func:
            case TYhfunc:
            case TYnfunc:       /* in case program plays games with function pointers */
            case TYnpfunc:
            case TYnsfunc:
            case TYifunc:
            case TYjfunc:
                s = 1;
                break;
            case TYarray:
            {
                if (t->Tflags & TFsizeunknown)
                {
                    t->Tflags &= ~TFsizeunknown;
                }
                if (t->Tflags & TFvla)
                {
                    s = tysize[pointertype];
                    break;
                }
                s = type_size(t->Tnext);
                unsigned long u = t->Tdim * (unsigned long) s;
                if (t->Tdim && ((u / t->Tdim) != s || (long)u < 0))
                    assert(0);          // overflow should have been detected in front end
                s = u;
                break;
            }
            case TYstruct:
                t = t->Ttag->Stype;     /* find main instance           */
                                        /* (for const struct X)         */
                if (t->Tflags & TFsizeunknown)
                {
                }
                assert(t->Ttag);
                s = t->Ttag->Sstruct->Sstructsize;
                break;
            case TYvoid:
                s = 1;
                break;

            case TYref:
                s = tysize(TYnptr);
                break;

            default:
                assert(0);
        }
    }
    return s;
}

/********************************
 * Return the size of a type for alignment purposes.
 */

unsigned type_alignsize(type *t)
{   targ_size_t sz;

L1:
    type_debug(t);

    sz = tyalignsize(t->Tty);
    if (sz == (targ_size_t)-1)
    {
        switch (tybasic(t->Tty))
        {
            case TYarray:
                if (t->Tflags & TFsizeunknown)
                    goto err1;
                t = t->Tnext;
                goto L1;
            case TYstruct:
                t = t->Ttag->Stype;         // find main instance
                                            // (for const struct X)
                if (t->Tflags & TFsizeunknown)
                    goto err1;
                sz = t->Ttag->Sstruct->Salignsize;
                if (sz > t->Ttag->Sstruct->Sstructalign + 1)
                    sz = t->Ttag->Sstruct->Sstructalign + 1;
                break;

            case TYldouble:
                assert(0);

            default:
            err1:                   // let type_size() handle error messages
                sz = type_size(t);
                break;
        }
    }

    //printf("type_alignsize() = %d\n", sz);
    return sz;
}

/*****************************
 * Compute the size of parameters for function call.
 * Used for stdcall name mangling.
 * Note that hidden parameters do not contribute to size.
 */

targ_size_t type_paramsize(type *t)
{
    targ_size_t sz = 0;
    if (tyfunc(t->Tty))
    {
        for (param_t *p = t->Tparamtypes; p; p = p->Pnext)
        {
            size_t n = type_size(p->Ptype);
            n = align(REGSIZE,n);       // align to REGSIZE boundary
            sz += n;
        }
    }
    return sz;
}

/*****************************
 * Create a type & initialize it.
 * Input:
 *      ty = TYxxxx
 * Returns:
 *      pointer to newly created type.
 */

type *type_alloc(tym_t ty)
{   type *t;
    static type tzero;

    assert(tybasic(ty) != TYtemplate);
    if (type_list)
    {   t = type_list;
        type_list = t->Tnext;
    }
    else
        t = (type *) mem_fmalloc(sizeof(type));
    tzero.Tty = ty;
    *t = tzero;
#if SRCPOS_4TYPES
    if (PARSER && config.fulltypes)
        t->Tsrcpos = getlinnum();
#endif
#ifdef DEBUG
    t->id = IDtype;
    type_num++;
    if (type_num > type_max)
        type_max = type_num;
#endif
    //dbg_printf("type_alloc() = %p ",t); WRTYxx(t->Tty); dbg_printf("\n");
    //if (t == (type*)0xB6B744) *(char*)0=0;
    return t;
}

/*****************************
 * Fake a type & initialize it.
 * Input:
 *      ty = TYxxxx
 * Returns:
 *      pointer to newly created type.
 */

type *type_fake(tym_t ty)
{   type *t;

    assert(ty != TYstruct);

    t = type_alloc(ty);
    if (typtr(ty) || tyfunc(ty))
    {   t->Tnext = type_alloc(TYvoid);  /* fake with pointer to void    */
        t->Tnext->Tcount = 1;
    }
    return t;
}

/*****************************
 * Allocate a type of ty with a Tnext of tn.
 */

type *type_allocn(tym_t ty,type *tn)
{   type *t;

    //printf("type_allocn(ty = x%x, tn = %p)\n", ty, tn);
    assert(tn);
    type_debug(tn);
    t = type_alloc(ty);
    t->Tnext = tn;
    tn->Tcount++;
    //printf("\tt = %p\n", t);
    return t;
}

/******************************
 * Allocate a TYmemptr type.
 */

/********************************
 * Allocate a pointer type.
 * Returns:
 *      Tcount already incremented
 */

type *type_pointer(type *tnext)
{
    type *t = type_allocn(TYnptr, tnext);
    t->Tcount++;
    return t;
}

/********************************
 * Allocate a dynamic array type.
 * Returns:
 *      Tcount already incremented
 */

type *type_dyn_array(type *tnext)
{
    type *t = type_allocn(TYdarray, tnext);
    t->Tcount++;
    return t;
}

/********************************
 * Allocate a static array type.
 * Returns:
 *      Tcount already incremented
 */

type *type_static_array(targ_size_t dim, type *tnext)
{
    type *t = type_allocn(TYarray, tnext);
    t->Tdim = dim;
    t->Tcount++;
    return t;
}

/********************************
 * Allocate an associative array type,
 * which are key=value pairs
 * Returns:
 *      Tcount already incremented
 */

type *type_assoc_array(type *tkey, type *tvalue)
{
    type *t = type_allocn(TYaarray, tvalue);
    t->Tkey = tkey;
    tkey->Tcount++;
    t->Tcount++;
    return t;
}

/********************************
 * Allocate a delegate type.
 * Returns:
 *      Tcount already incremented
 */

type *type_delegate(type *tnext)
{
    type *t = type_allocn(TYdelegate, tnext);
    t->Tcount++;
    return t;
}

/***********************************
 * Allocation a function type.
 * Input:
 *      tyf             function type
 *      ptypes[nparams] types of the function parameters
 *      variadic        if ... function
 *      tret            return type
 * Returns:
 *      Tcount already incremented
 */
extern "C" // because of size_t on OSX 32
{
type *type_function(tym_t tyf, type **ptypes, size_t nparams, bool variadic, type *tret)
{
    param_t *paramtypes = nullptr;
    for (size_t i = 0; i < nparams; i++)
    {
        param_append_type(&paramtypes, ptypes[i]);
    }
    type *t = type_allocn(tyf, tret);
    t->Tflags |= TFprototype;
    if (!variadic)
        t->Tflags |= TFfixed;
    t->Tparamtypes = paramtypes;
    t->Tcount++;
    return t;
}
}

/***************************************
 * Create an enum type.
 * Input:
 *      name    name of enum
 *      tbase   "base" type of enum
 * Returns:
 *      Tcount already incremented
 */
type *type_enum(const char *name, type *tbase)
{
    Symbol *s = symbol_calloc(name);
    s->Sclass = SCenum;
    s->Senum = (enum_t *) MEM_PH_CALLOC(sizeof(enum_t));
    s->Senum->SEflags |= SENforward;        // forward reference

    type *t = type_allocn(TYenum, tbase);
    t->Ttag = (Classsym *)s;            // enum tag name
    t->Tcount++;
    s->Stype = t;
    t->Tcount++;
    return t;
}

/**************************************
 * Create a struct/union/class type.
 * Params:
 *      name = name of struct (this function makes its own copy of the string)
 * Returns:
 *      Tcount already incremented
 */
type *type_struct_class(const char *name, unsigned alignsize, unsigned structsize,
        type *arg1type, type *arg2type, bool isUnion, bool isClass, bool isPOD)
{
    Symbol *s = symbol_calloc(name);
    s->Sclass = SCstruct;
    s->Sstruct = struct_calloc();
    s->Sstruct->Salignsize = alignsize;
    s->Sstruct->Sstructalign = alignsize;
    s->Sstruct->Sstructsize = structsize;
    s->Sstruct->Sarg1type = arg1type;
    s->Sstruct->Sarg2type = arg2type;

    if (!isPOD)
        s->Sstruct->Sflags |= STRnotpod;
    if (isUnion)
        s->Sstruct->Sflags |= STRunion;
    if (isClass)
    {   s->Sstruct->Sflags |= STRclass;
        assert(!isUnion && isPOD);
    }

    type *t = type_alloc(TYstruct);
    t->Ttag = (Classsym *)s;            // structure tag name
    t->Tcount++;
    s->Stype = t;
    t->Tcount++;
    return t;
}

/*****************************
 * Free up data type.
 */

void type_free(type *t)
{   type *tn;
    tym_t ty;

    while (t)
    {
        //dbg_printf("type_free(%p, Tcount = %d)\n", t, t->Tcount);
        type_debug(t);
        assert((int)t->Tcount != -1);
        if (--t->Tcount)                /* if usage count doesn't go to 0 */
            break;
        ty = tybasic(t->Tty);
        if (tyfunc(ty))
        {   param_free(&t->Tparamtypes);
            list_free(&t->Texcspec, (list_free_fp)type_free);
        }
        else if (t->Tflags & TFvla && t->Tel)
            el_free(t->Tel);
        else if (t->Tkey && typtr(ty))
            type_free(t->Tkey);

        tn = t->Tnext;
        t->Tnext = type_list;
        type_list = t;                  /* link into free list          */
        t = tn;
    }
}

#ifdef STATS
/* count number of free types available on type list */
type_count_free()
    {
    type *t;
    int count;

    for(t=type_list;t;t=t->Tnext)
        count++;
    dbg_printf("types on free list %d with max of %d\n",count,type_max);
    }
#endif

/**********************************
 * Initialize type package.
 */

STATIC type * type_allocbasic(tym_t ty)
{   type *t;

    t = type_alloc(ty);
    t->Tmangle = mTYman_c;
    t->Tcount = 1;              /* so it is not inadvertantly free'd    */
    return t;
}

void type_init()
{
    tsbool    = type_allocbasic(TYbool);
    tswchar_t = type_allocbasic(TYwchar_t);
    tsdchar   = type_allocbasic(TYdchar);
    tsvoid    = type_allocbasic(TYvoid);
    tsnullptr = type_allocbasic(TYnullptr);
    tschar16  = type_allocbasic(TYchar16);
    tsuchar   = type_allocbasic(TYuchar);
    tsschar   = type_allocbasic(TYschar);
    tschar    = type_allocbasic(TYchar);
    tsshort   = type_allocbasic(TYshort);
    tsushort  = type_allocbasic(TYushort);
    tsint     = type_allocbasic(TYint);
    tsuns     = type_allocbasic(TYuint);
    tslong    = type_allocbasic(TYlong);
    tsulong   = type_allocbasic(TYulong);
    tsllong   = type_allocbasic(TYllong);
    tsullong  = type_allocbasic(TYullong);
    tsfloat   = type_allocbasic(TYfloat);
    tsdouble  = type_allocbasic(TYdouble);
    tsreal64  = type_allocbasic(TYdouble_alias);
    tsldouble  = type_allocbasic(TYldouble);
    tsifloat   = type_allocbasic(TYifloat);
    tsidouble  = type_allocbasic(TYidouble);
    tsildouble  = type_allocbasic(TYildouble);
    tscfloat   = type_allocbasic(TYcfloat);
    tscdouble  = type_allocbasic(TYcdouble);
    tscldouble  = type_allocbasic(TYcldouble);

    if (I64)
    {
        TYptrdiff = TYllong;
        TYsize = TYullong;
        tsptrdiff = tsllong;
        tssize = tsullong;
    }
    else
    {
        TYptrdiff = TYint;
        TYsize = TYuint;
        tsptrdiff = tsint;
        tssize = tsuns;
    }

    // Type of trace function
    tstrace = type_fake(TYnfunc);
    tstrace->Tmangle = mTYman_c;
    tstrace->Tcount++;

    chartype = (config.flags3 & CFG3ju) ? tsuchar : tschar;

    // Type of far library function
    tsclib = type_fake(LARGECODE ? TYfpfunc : TYnpfunc);
    tsclib->Tmangle = mTYman_c;
    tsclib->Tcount++;

    tspvoid = type_allocn(pointertype,tsvoid);
    tspvoid->Tmangle = mTYman_c;
    tspvoid->Tcount++;

    // Type of far library function
    tsjlib =    type_fake(TYjfunc);
    tsjlib->Tmangle = mTYman_c;
    tsjlib->Tcount++;

    tsdlib = tsjlib;

    // Type of logical expression
    tslogical = (config.flags4 & CFG4bool) ? tsbool : tsint;

    for (int i = 0; i < TYMAX; i++)
    {
        if (tstypes[i])
        {   tsptr2types[i] = type_allocn(pointertype,tstypes[i]);
            tsptr2types[i]->Tcount++;
        }
    }
}

/**********************************
 * Free type_list.
 */

void type_term()
{
#if TERMCODE
    type *tn;
    param_t *pn;
    int i;

    for (i = 0; i < arraysize(tstypes); i++)
    {   type *t = tsptr2types[i];

        if (t)
        {   assert(!(t->Tty & (mTYconst | mTYvolatile | mTYimmutable | mTYshared)));
            assert(!(t->Tflags));
            assert(!(t->Tmangle));
            type_free(t);
        }
        type_free(tstypes[i]);
    }

    type_free(tsclib);
    type_free(tspvoid);
    type_free(tspcvoid);
    type_free(tsjlib);
    type_free(tstrace);

    while (type_list)
    {   tn = type_list->Tnext;
        mem_ffree(type_list);
        type_list = tn;
    }

    while (param_list)
    {   pn = param_list->Pnext;
        mem_ffree(param_list);
        param_list = pn;
    }

#ifdef DEBUG
    dbg_printf("Max # of types = %d\n",type_max);
    if (type_num != 0)
        dbg_printf("type_num = %d\n",type_num);
/*    assert(type_num == 0);*/
#endif
#endif // TERMCODE
}

/*******************************
 * Type type information.
 */

/**************************
 * Make copy of a type.
 */

type *type_copy(type *t)
{   type *tn;
    param_t *p;

    type_debug(t);
        tn = type_alloc(t->Tty);
    *tn = *t;
    switch (tybasic(tn->Tty))
    {
            case TYarray:
                if (tn->Tflags & TFvla)
                    tn->Tel = el_copytree(tn->Tel);
                break;

            default:
                if (tyfunc(tn->Tty))
                {
                L1:
                    tn->Tparamtypes = nullptr;
                    for (p = t->Tparamtypes; p; p = p->Pnext)
                    {   param_t *pn;

                        pn = param_append_type(&tn->Tparamtypes,p->Ptype);
                        if (p->Pident)
                        {
                            pn->Pident = (char *)MEM_PH_STRDUP(p->Pident);
                        }
                        assert(!p->Pelem);
                    }
                }
                else if (tn->Tkey && typtr(tn->Tty))
                    tn->Tkey->Tcount++;
                break;
    }
    if (tn->Tnext)
    {   type_debug(tn->Tnext);
        tn->Tnext->Tcount++;
    }
    tn->Tcount = 0;
    return tn;
}

/****************************
 * Modify the tym_t field of a type.
 */

type *type_setty(type **pt,long newty)
{   type *t;

    t = *pt;
    type_debug(t);
    if ((tym_t)newty != t->Tty)
    {   if (t->Tcount > 1)              /* if other people pointing at t */
        {   type *tn;

            tn = type_copy(t);
            tn->Tcount++;
            type_free(t);
            t = tn;
            *pt = t;
        }
        t->Tty = newty;
    }
    return t;
}

/******************************
 * Set type field of some object to t.
 */

type *type_settype(type **pt, type *t)
{
    if (t)
    {   type_debug(t);
        t->Tcount++;
    }
    type_free(*pt);
    return *pt = t;
}

/****************************
 * Modify the Tmangle field of a type.
 */

type *type_setmangle(type **pt,mangle_t mangle)
{   type *t;

    t = *pt;
    type_debug(t);
    if (mangle != type_mangle(t))
    {
        if (t->Tcount > 1)              // if other people pointing at t
        {   type *tn;

            tn = type_copy(t);
            tn->Tcount++;
            type_free(t);
            t = tn;
            *pt = t;
        }
        t->Tmangle = mangle;
    }
    return t;
}

/******************************
 * Set/clear const and volatile bits in *pt according to the settings
 * in cv.
 */

type *type_setcv(type **pt,tym_t cv)
{   unsigned long ty;

    type_debug(*pt);
    ty = (*pt)->Tty & ~(mTYconst | mTYvolatile | mTYimmutable | mTYshared);
    return type_setty(pt,ty | (cv & (mTYconst | mTYvolatile | mTYimmutable | mTYshared)));
}

/*****************************
 * Set dimension of array.
 */

type *type_setdim(type **pt,targ_size_t dim)
{   type *t = *pt;

    type_debug(t);
    if (t->Tcount > 1)                  /* if other people pointing at t */
    {   type *tn;

        tn = type_copy(t);
        tn->Tcount++;
        type_free(t);
        t = tn;
    }
    t->Tflags &= ~TFsizeunknown; /* we have determined its size */
    t->Tdim = dim;              /* index of array               */
    return *pt = t;
}


/*****************************
 * Create a 'dependent' version of type t.
 */

type *type_setdependent(type *t)
{
    type_debug(t);
    if (t->Tcount > 0 &&                        /* if other people pointing at t */
        !(t->Tflags & TFdependent))
    {
        t = type_copy(t);
    }
    t->Tflags |= TFdependent;
    return t;
}

/************************************
 * Determine if type t is a dependent type.
 */

int type_isdependent(type *t)
{
    Symbol *stempl;
    type *tstart;

    //printf("type_isdependent(%p)\n", t);
    //type_print(t);
    for (tstart = t; t; t = t->Tnext)
    {
        type_debug(t);
        if (t->Tflags & TFdependent)
            goto Lisdependent;
        if (tyfunc(t->Tty)
                || tybasic(t->Tty) == TYtemplate
                )
        {
            for (param_t *p = t->Tparamtypes; p; p = p->Pnext)
            {
                if (p->Ptype && type_isdependent(p->Ptype))
                    goto Lisdependent;
                if (p->Pelem && el_isdependent(p->Pelem))
                    goto Lisdependent;
            }
        }
        else if (type_struct(t) &&
                 (stempl = t->Ttag->Sstruct->Stempsym) != nullptr)
        {
            for (param_t *p = t->Ttag->Sstruct->Sarglist; p; p = p->Pnext)
            {
                if (p->Ptype && type_isdependent(p->Ptype))
                    goto Lisdependent;
                if (p->Pelem && el_isdependent(p->Pelem))
                    goto Lisdependent;
            }
        }
    }
    //printf("\tis not dependent\n");
    return 0;

Lisdependent:
    //printf("\tis dependent\n");
    // Dependence on a dependent type makes this type dependent as well
    tstart->Tflags |= TFdependent;
    return 1;
}


/*******************************
 * Recursively check if type u is embedded in type t.
 * Returns:
 *      != 0 if embedded
 */

int type_embed(type *t,type *u)
{   param_t *p;

    for (; t; t = t->Tnext)
    {
        type_debug(t);
        if (t == u)
            return 1;
        if (tyfunc(t->Tty))
        {
            for (p = t->Tparamtypes; p; p = p->Pnext)
                if (type_embed(p->Ptype,u))
                    return 1;
        }
    }
    return 0;
}


/***********************************
 * Determine if type is a VLA.
 */

int type_isvla(type *t)
{
    while (t)
    {
        if (tybasic(t->Tty) != TYarray)
            break;
        if (t->Tflags & TFvla)
            return 1;
        t = t->Tnext;
    }
    return 0;
}


/**********************************
 * Pretty-print a type.
 */

void type_print(type *t)
{
  type_debug(t);
  dbg_printf("Tty="); WRTYxx(t->Tty);
  dbg_printf(" Tmangle=%d",t->Tmangle);
  dbg_printf(" Tflags=x%x",t->Tflags);
  dbg_printf(" Tcount=%d",t->Tcount);
  if (!(t->Tflags & TFsizeunknown) &&
        tybasic(t->Tty) != TYvoid &&
        tybasic(t->Tty) != TYident &&
        tybasic(t->Tty) != TYtemplate &&
        tybasic(t->Tty) != TYmfunc &&
        tybasic(t->Tty) != TYarray)
      dbg_printf(" Tsize=%lld",(long long)type_size(t));
  dbg_printf(" Tnext=%p",t->Tnext);
  switch (tybasic(t->Tty))
  {     case TYstruct:
        case TYmemptr:
            dbg_printf(" Ttag=%p,'%s'",t->Ttag,t->Ttag->Sident);
            //dbg_printf(" Sfldlst=%p",t->Ttag->Sstruct->Sfldlst);
            break;

        case TYarray:
            dbg_printf(" Tdim=%ld",(long)t->Tdim);
            break;

        case TYident:
            dbg_printf(" Tident='%s'",t->Tident);
            break;
        case TYtemplate:
            dbg_printf(" Tsym='%s'",((typetemp_t *)t)->Tsym->Sident);
            {   param_t *p;
                int i;

                i = 1;
                for (p = t->Tparamtypes; p; p = p->Pnext)
                {   dbg_printf("\nTP%d (%p): ",i++,p);
                    fflush(stdout);

dbg_printf("Pident=%p,Ptype=%p,Pelem=%p,Pnext=%p ",p->Pident,p->Ptype,p->Pelem,p->Pnext);
                    param_debug(p);
                    if (p->Pident)
                        printf("'%s' ", p->Pident);
                    if (p->Ptype)
                        type_print(p->Ptype);
                    if (p->Pelem)
                        elem_print(p->Pelem);
                }
            }
            break;

        default:
            if (tyfunc(t->Tty))
            {   param_t *p;
                int i;

                i = 1;
                for (p = t->Tparamtypes; p; p = p->Pnext)
                {   dbg_printf("\nP%d (%p): ",i++,p);
                    fflush(stdout);

dbg_printf("Pident=%p,Ptype=%p,Pelem=%p,Pnext=%p ",p->Pident,p->Ptype,p->Pelem,p->Pnext);
                    param_debug(p);
                    if (p->Pident)
                        printf("'%s' ", p->Pident);
                    type_print(p->Ptype);
                }
            }
            break;
  }
  dbg_printf("\n");
  if (t->Tnext) type_print(t->Tnext);
}

/*******************************
 * Pretty-print a param_t
 */

void param_t::print()
{
    dbg_printf("Pident=%p,Ptype=%p,Pelem=%p,Psym=%p,Pnext=%p\n",Pident,Ptype,Pelem,Psym,Pnext);
    if (Pident)
        dbg_printf("\tPident = '%s'\n", Pident);
    if (Ptype)
    {   dbg_printf("\tPtype =\n");
        type_print(Ptype);
    }
    if (Pelem)
    {   dbg_printf("\tPelem =\n");
        elem_print(Pelem);
    }
    if (Pdeftype)
    {   dbg_printf("\tPdeftype =\n");
        type_print(Pdeftype);
    }
    if (Psym)
    {   dbg_printf("\tPsym = '%s'\n", Psym->Sident);
    }
    if (Pptpl)
    {   dbg_printf("\tPptpl = %p\n", Pptpl);
    }
}

void param_t::print_list()
{
    for (param_t *p = this; p; p = p->Pnext)
        p->print();
}

/****************************
 * Allocate a param_t.
 */

param_t *param_calloc()
{
    static param_t pzero;
    param_t *p;

    if (param_list)
    {
        p = param_list;
        param_list = p->Pnext;
    }
    else
    {
        p = (param_t *) mem_fmalloc(sizeof(param_t));
    }
    *p = pzero;
#ifdef DEBUG
    p->id = IDparam;
#endif
    return p;
}

/***************************
 * Allocate a param_t of type t, and append it to parameter list.
 */

param_t *param_append_type(param_t **pp,type *t)
{   param_t *p;

    p = param_calloc();
    while (*pp)
    {   param_debug(*pp);
        pp = &((*pp)->Pnext);   /* find end of list     */
    }
    *pp = p;                    /* append p to list     */
    type_debug(t);
    p->Ptype = t;
    t->Tcount++;
    return p;
}

/************************
 * Version of param_free() suitable for list_free().
 */

void param_free_l(param_t *p)
{
    param_free(&p);
}

/***********************
 * Free parameter list.
 * Output:
 *      paramlst = nullptr
 */

void param_free(param_t **pparamlst)
{   param_t *p,*pn;

#if !TX86
    debug_assert(PARSER);
#endif
    for (p = *pparamlst; p; p = pn)
    {   param_debug(p);
        pn = p->Pnext;
        type_free(p->Ptype);
        mem_free(p->Pident);
        el_free(p->Pelem);
        type_free(p->Pdeftype);
        if (p->Pptpl)
            param_free(&p->Pptpl);
#ifdef DEBUG
        p->id = 0;
#endif
        p->Pnext = param_list;
        param_list = p;
    }
    *pparamlst = nullptr;
}

/***********************************
 * Compute number of parameters
 */

unsigned param_t::length()
{
    unsigned nparams = 0;
    param_t *p;

    for (p = this; p; p = p->Pnext)
        nparams++;
    return nparams;
}

/*************************************
 * Create template-argument-list blank from
 * template-parameter-list
 * Input:
 *      ptali   initial template-argument-list
 */

param_t *param_t::createTal(param_t *ptali)
{
    param_t *ptal = nullptr;
    param_t **pp = &ptal;
    param_t *p;

    for (p = this; p; p = p->Pnext)
    {
        *pp = param_calloc();
        if (p->Pident)
        {
            // Should find a way to just point rather than dup
            (*pp)->Pident = (char *)MEM_PH_STRDUP(p->Pident);
        }
        if (ptali)
        {
            if (ptali->Ptype)
            {   (*pp)->Ptype = ptali->Ptype;
                (*pp)->Ptype->Tcount++;
            }
            if (ptali->Pelem)
            {
                elem *e = el_copytree(ptali->Pelem);
                (*pp)->Pelem = e;
            }
            (*pp)->Psym = ptali->Psym;
            (*pp)->Pflags = ptali->Pflags;
            assert(!ptali->Pptpl);
            ptali = ptali->Pnext;
        }
        pp = &(*pp)->Pnext;
    }
    return ptal;
}

/**********************************
 * Look for Pident matching id
 */

param_t *param_t::search(char *id)
{   param_t *p;

    for (p = this; p; p = p->Pnext)
    {
        if (p->Pident && strcmp(p->Pident, id) == 0)
            break;
    }
    return p;
}

/**********************************
 * Look for Pident matching id
 */

int param_t::searchn(char *id)
{   param_t *p;
    int n = 0;

    for (p = this; p; p = p->Pnext)
    {
        if (p->Pident && strcmp(p->Pident, id) == 0)
            return n;
        n++;
    }
    return -1;
}

/*************************************
 * Search for member, create symbol as needed.
 * Used for symbol tables for VLA's such as:
 *      void func(int n, int a[n]);
 */

symbol *param_search(const char *name, param_t **pp)
{   symbol *s = nullptr;
    param_t *p;

    p = (*pp)->search((char *)name);
    if (p)
    {
        s = p->Psym;
        if (!s)
        {
            s = symbol_calloc(p->Pident);
            s->Sclass = SCparameter;
            s->Stype = p->Ptype;
            s->Stype->Tcount++;
            p->Psym = s;
        }
    }
    return s;
}

int typematch(type *t1, type *t2, int relax);

// Return TRUE if type lists match.
static int paramlstmatch(param_t *p1,param_t *p2)
{
        return p1 == p2 ||
            p1 && p2 && typematch(p1->Ptype,p2->Ptype,0) &&
            paramlstmatch(p1->Pnext,p2->Pnext)
            ;
}

/*************************************************
 * A cheap version of exp2.typematch() and exp2.paramlstmatch(),
 * so that we can get cpp_mangle() to work for MARS.
 * It's less complex because it doesn't do templates and
 * can rely on strict typechecking.
 * Returns:
 *      !=0 if types match.
 */

int typematch(type *t1,type *t2,int relax)
{ tym_t t1ty, t2ty;
  tym_t tym;

  tym = ~(mTYimport | mTYnaked);

  return t1 == t2 ||
            t1 && t2 &&

            (
                /* ignore name mangling */
                (t1ty = (t1->Tty & tym)) == (t2ty = (t2->Tty & tym))
            )
                 &&

            (tybasic(t1ty) != TYarray || t1->Tdim == t2->Tdim ||
             t1->Tflags & TFsizeunknown || t2->Tflags & TFsizeunknown)
                 &&

            (tybasic(t1ty) != TYstruct
                && tybasic(t1ty) != TYenum
                && tybasic(t1ty) != TYmemptr
             || t1->Ttag == t2->Ttag)
                 &&

            typematch(t1->Tnext,t2->Tnext, 0)
                 &&

            (!tyfunc(t1ty) ||
             ((t1->Tflags & TFfixed) == (t2->Tflags & TFfixed) &&
                 paramlstmatch(t1->Tparamtypes,t2->Tparamtypes) ))
         ;
}

