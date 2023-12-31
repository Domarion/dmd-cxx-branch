// Copyright (C) 1984-1998 by Symantec
// Copyright (C) 2000-2021 by The D Language Foundation, All Rights Reserved
// http://www.digitalmars.com
// Written by Walter Bright
/*
 * This source file is made available for personal use
 * only. The license is in backendlicense.txt
 * For any other uses, please contact Digital Mars.
 */

#include        <stdio.h>
#include        <string.h>
#include        <time.h>

#include        "cc.h"
#include        "oper.h"
#include        "global.h"
#include        "type.h"
#include        "filespec.h"
#include        "code.h"
#include        "cgcv.h"
#include        "go.h"
#include        "dt.h"

static char __file__[] = __FILE__;      /* for tassert.h                */
#include        "tassert.h"

static  int addrparam;  /* see if any parameters get their address taken */

/***************************
 * Write out statically allocated data.
 * Input:
 *      s               symbol to be initialized
 */

void outdata(symbol *s)
{
    targ_size_t datasize,a;
    int seg;
    targ_size_t offset;
    int flags;
    tym_t ty;

    symbol_debug(s);
#ifdef DEBUG
    debugy && dbg_printf("outdata('%s')\n",s->Sident);
#endif
    //printf("outdata('%s', ty=x%x)\n",s->Sident,s->Stype->Tty);
    //symbol_print(s);

    // Data segment variables are always live on exit from a function
    s->Sflags |= SFLlivexit;

    dt_t *dtstart = s->Sdt;
    s->Sdt = NULL;                      // it will be free'd
    datasize = 0;
    ty = s->ty();
    if (ty & mTYexport && config.wflags & WFexpdef && s->Sclass != SCstatic)
        objmod->export_symbol(s,0);        // export data definition
    for (dt_t *dt = dtstart; dt; dt = dt->DTnext)
    {
        //printf("\tdt = %p, dt = %d\n",dt,dt->dt);
        switch (dt->dt)
        {   case DT_abytes:
            {   // Put out the data for the string, and
                // reserve a spot for a pointer to that string
                datasize += size(dt->Dty);      // reserve spot for pointer to string
#if TARGET_SEGMENTED
                if (tybasic(dt->Dty) == TYcptr)
                {   dt->DTseg = cseg;
                    dt->DTabytes += Coffset;
                    goto L1;
                }
                else if (tybasic(dt->Dty) == TYfptr &&
                         dt->DTnbytes > config.threshold)
                {
                    targ_size_t foffset;
                    dt->DTseg = objmod->fardata(s->Sident,dt->DTnbytes,&foffset);
                    dt->DTabytes += foffset;
                L1:
                    objmod->write_bytes(SegData[dt->DTseg],dt->DTnbytes,dt->DTpbytes);
                    break;
                }
                else
#endif
                {
                    dt->DTabytes += objmod->data_readonly(dt->DTpbytes,dt->DTnbytes,&dt->DTseg);
                }
                break;
            }
            case DT_ibytes:
                datasize += dt->DTn;
                break;
            case DT_nbytes:
                //printf("DT_nbytes %d\n", dt->DTnbytes);
                datasize += dt->DTnbytes;
                break;

            case DT_azeros:
                /* A block of zeros
                 */
                //printf("DT_azeros %d\n", dt->DTazeros);
            case_azeros:
                datasize += dt->DTazeros;
                if (dt == dtstart && !dt->DTnext && s->Sclass != SCcomdat &&
                    (s->Sseg == UNKNOWN || s->Sseg <= UDATA))
                {   /* first and only, so put in BSS segment
                     */
                    switch (ty & mTYLINK)
                    {
#if TARGET_SEGMENTED
                        case mTYfar:                    // if far data
                            s->Sseg = objmod->fardata(s->Sident,datasize,&s->Soffset);
                            s->Sfl = FLfardata;
                            break;

                        case mTYcs:
                            s->Sseg = cseg;
                            Coffset = align(datasize,Coffset);
                            s->Soffset = Coffset;
                            Coffset += datasize;
                            s->Sfl = FLcsdata;
                            break;
#endif
                        case mTYthreadData:
                            assert(config.objfmt == OBJ_MACH && I64);
                        case mTYthread:
                        {   seg_data *pseg = objmod->tlsseg_bss();
                            s->Sseg = pseg->SDseg;
                            objmod->data_start(s, datasize, pseg->SDseg);
                            if (config.objfmt == OBJ_OMF)
                                pseg->SDoffset += datasize;
                            else
                                objmod->lidata(pseg->SDseg, pseg->SDoffset, datasize);
                            s->Sfl = FLtlsdata;
                            break;
                        }
                        default:
                            s->Sseg = UDATA;
                            objmod->data_start(s,datasize,UDATA);
                            objmod->lidata(s->Sseg,s->Soffset,datasize);
                            s->Sfl = FLudata;           // uninitialized data
                            break;
                    }
                    assert(s->Sseg && s->Sseg != UNKNOWN);
                    if (s->Sclass == SCglobal || (s->Sclass == SCstatic && config.objfmt != OBJ_OMF)) // if a pubdef to be done
                        objmod->pubdefsize(s->Sseg,s,s->Soffset,datasize);   // do the definition
                    searchfixlist(s);
                    if (config.fulltypes &&
                        !(s->Sclass == SCstatic && funcsym_p)) // not local static
                        cv_outsym(s);
                    goto Lret;
                }
                break;
            case DT_common:
                assert(!dt->DTnext);
                outcommon(s,dt->DTazeros);
                goto Lret;

            case DT_xoff:
            {   symbol *sb = dt->DTsym;

                if (tyfunc(sb->ty()))
                    ;
                else if (sb->Sdt)               // if initializer for symbol
{ if (!s->Sseg) s->Sseg = DATA;
                    outdata(sb);                // write out data for symbol
}
            }
            case DT_coff:
                datasize += size(dt->Dty);
                break;
            default:
#ifdef DEBUG
                dbg_printf("dt = %p, dt = %d\n",dt,dt->dt);
#endif
                assert(0);
        }
    }

    if (s->Sclass == SCcomdat)          // if initialized common block
    {
        seg = objmod->comdatsize(s, datasize);
        switch (ty & mTYLINK)
        {
            case mTYfar:                // if far data
                s->Sfl = FLfardata;
                break;

            case mTYcs:
                s->Sfl = FLcsdata;
                break;

            case mTYnear:
            case 0:
                s->Sfl = FLdata;        // initialized data
                break;
            case mTYthread:
                s->Sfl = FLtlsdata;
                break;

            default:
                assert(0);
        }
    }
    else
    {
      switch (ty & mTYLINK)
      {
#if TARGET_SEGMENTED
        case mTYfar:                    // if far data
            seg = objmod->fardata(s->Sident,datasize,&s->Soffset);
            s->Sfl = FLfardata;
            break;

        case mTYcs:
            seg = cseg;
            Coffset = align(datasize,Coffset);
            s->Soffset = Coffset;
            s->Sfl = FLcsdata;
            break;
#endif
        case mTYthreadData:
        {
            assert(config.objfmt == OBJ_MACH && I64);

            seg_data *pseg = objmod->tlsseg_data();
            s->Sseg = pseg->SDseg;
            objmod->data_start(s, datasize, s->Sseg);
            seg = pseg->SDseg;
            s->Sfl = FLtlsdata;
            break;
        }
        case mTYthread:
        {
            seg_data *pseg = objmod->tlsseg();
            s->Sseg = pseg->SDseg;
            objmod->data_start(s, datasize, s->Sseg);
            seg = pseg->SDseg;
            s->Sfl = FLtlsdata;
            break;
        }
        case mTYnear:
        case 0:
            if (
                s->Sseg == 0 ||
                s->Sseg == UNKNOWN)
                s->Sseg = DATA;
            seg = objmod->data_start(s,datasize,DATA);
            s->Sfl = FLdata;            // initialized data
            break;
        default:
            assert(0);
      }
    }
    if (s->Sseg == UNKNOWN && (config.objfmt == OBJ_ELF || config.objfmt == OBJ_MACH))
        s->Sseg = seg;
    else if (config.objfmt == OBJ_OMF)
        s->Sseg = seg;
    else
        seg = s->Sseg;

    if (s->Sclass == SCglobal || (s->Sclass == SCstatic && config.objfmt != OBJ_OMF))
        objmod->pubdefsize(seg,s,s->Soffset,datasize);    /* do the definition            */

    assert(s->Sseg != UNKNOWN);
    if (config.fulltypes &&
        !(s->Sclass == SCstatic && funcsym_p)) // not local static
        cv_outsym(s);
    searchfixlist(s);

    /* Go back through list, now that we know its size, and send out    */
    /* the data.                                                        */

    offset = s->Soffset;

    for (dt_t *dt = dtstart; dt; dt = dt->DTnext)
    {
        switch (dt->dt)
        {   case DT_abytes:
                if (tyreg(dt->Dty))
                    flags = CFoff;
                else
                    flags = CFoff | CFseg;
                if (I64)
                    flags |= CFoffset64;
                if (tybasic(dt->Dty) == TYcptr)
                    objmod->reftocodeseg(seg,offset,dt->DTabytes);
                else
#if TARGET_LINUX
                    objmod->reftodatseg(seg,offset,dt->DTabytes,dt->DTseg,flags);
#else
                /*else*/ if (dt->DTseg == DATA)
                    objmod->reftodatseg(seg,offset,dt->DTabytes,DATA,flags);
#if MARS
                else if (dt->DTseg == CDATA)
                    objmod->reftodatseg(seg,offset,dt->DTabytes,CDATA,flags);
#endif
                else
                    objmod->reftofarseg(seg,offset,dt->DTabytes,dt->DTseg,flags);
#endif
                offset += size(dt->Dty);
                break;
            case DT_ibytes:
                objmod->bytes(seg,offset,dt->DTn,dt->DTdata);
                offset += dt->DTn;
                break;
            case DT_nbytes:
                objmod->bytes(seg,offset,dt->DTnbytes,dt->DTpbytes);
                offset += dt->DTnbytes;
                break;
            case DT_azeros:
                //printf("objmod->lidata(seg = %d, offset = %d, azeros = %d)\n", seg, offset, dt->DTazeros);
                if (0 && seg == cseg)
                {
                    objmod->lidata(seg,offset,dt->DTazeros);
                    offset += dt->DTazeros;
                }
                else
                {
                    SegData[seg]->SDoffset = offset;
                    objmod->lidata(seg,offset,dt->DTazeros);
                    offset = SegData[seg]->SDoffset;
                }
                break;
            case DT_xoff:
            {
                symbol *sb = dt->DTsym;          // get external symbol pointer
                a = dt->DToffset; // offset from it
                if (tyreg(dt->Dty))
                    flags = CFoff;
                else
                    flags = CFoff | CFseg;
                if (I64 && tysize(dt->Dty) == 8)
                    flags |= CFoffset64;
                offset += objmod->reftoident(seg,offset,sb,a,flags);
                break;
            }
            case DT_coff:
                objmod->reftocodeseg(seg,offset,dt->DToffset);
                offset += intsize;
                break;
            default:
#ifdef DEBUG
                dbg_printf("dt = %p, dt = %d\n",dt,dt->dt);
#endif
                assert(0);
        }
    }
    Offset(seg) = offset;
Lret:
    dt_free(dtstart);
}



/******************************
 * Output n bytes of a common block, n > 0.
 */

void outcommon(symbol *s,targ_size_t n)
{
    //printf("outcommon('%s',%d)\n",s->Sident,n);
    if (n != 0)
    {
        assert(s->Sclass == SCglobal);
        if (s->ty() & mTYcs) // if store in code segment
        {
            /* COMDEFs not supported in code segment
             * so put them out as initialized 0s
             */
            DtBuilder dtb;
            dtb.nzeros(n);
            s->Sdt = dtb.finish();
            outdata(s);
        }
        else if (s->ty() & mTYthread) // if store in thread local segment
        {
            if (config.objfmt == OBJ_ELF)
            {
                s->Sclass = SCcomdef;
                objmod->common_block(s, 0, n, 1);
            }
            else
            {
                /* COMDEFs not supported in tls segment
                 * so put them out as COMDATs with initialized 0s
                 */
                s->Sclass = SCcomdat;
                DtBuilder dtb;
                dtb.nzeros(n);
                s->Sdt = dtb.finish();
                outdata(s);
            }
        }
        else
        {
            s->Sclass = SCcomdef;
            if (config.objfmt == OBJ_OMF)
            {
                s->Sxtrnnum = objmod->common_block(s,(s->ty() & mTYfar) == 0,n,1);
                if (s->ty() & mTYfar)
                    s->Sfl = FLfardata;
                else
                    s->Sfl = FLextern;
                s->Sseg = UNKNOWN;
                pstate.STflags |= PFLcomdef;
            }
            else
                objmod->common_block(s, 0, n, 1);
        }
        if (config.fulltypes)
            cv_outsym(s);
    }
}

/*************************************
 * Mark a symbol as going into a read-only segment.
 */

void out_readonly(symbol *s)
{
    // The default is DATA
    if (config.objfmt == OBJ_ELF || config.objfmt == OBJ_MACH)
    {
        /* Cannot have pointers in CDATA when compiling PIC code, because
         * they require dynamic relocations of the read-only segment.
         * Instead use the .data.rel.ro section. See Bugzilla 11171.
         */
        if (config.flags3 & CFG3pic && dtpointers(s->Sdt))
            s->Sseg = CDATAREL;
        else
            s->Sseg = CDATA;
    }
    else
    {
        // Haven't really worked out where immutable read-only data can go.
    }
}

/******************************
 * Walk expression tree, converting it from a PARSER tree to
 * a code generator tree.
 */

STATIC void outelem(elem *e)
{
    symbol *s;
    tym_t tym;
    elem *e1;

again:
    assert(e);
    elem_debug(e);

#ifdef DEBUG
    if (EBIN(e))
        assert(e->E1 && e->E2);
//    else if (EUNA(e))
//      assert(e->E1 && !e->E2);
#endif

    switch (e->Eoper)
    {
    default:
    Lop:
#if DEBUG
        //if (!EOP(e)) dbg_printf("e->Eoper = x%x\n",e->Eoper);
#endif
        if (EBIN(e))
        {   outelem(e->E1);
            e = e->E2;
        }
        else if (EUNA(e))
        {
            e = e->E1;
        }
        else
            break;
        goto again;                     /* iterate instead of recurse   */
    case OPaddr:
        e1 = e->E1;
        if (e1->Eoper == OPvar)
        {   // Fold into an OPrelconst
            tym = e->Ety;
            el_copy(e,e1);
            e->Ety = tym;
            e->Eoper = OPrelconst;
            el_free(e1);
            goto again;
        }
        goto Lop;

    case OPrelconst:
    case OPvar:
    L6:
        s = e->EV.sp.Vsym;
        assert(s);
        symbol_debug(s);
        switch (s->Sclass)
        {
            case SCregpar:
            case SCparameter:
                if (e->Eoper == OPrelconst)
                    addrparam = TRUE;   // taking addr of param list
                break;

            case SCstatic:
            case SClocstat:
            case SCextern:
            case SCglobal:
            case SCcomdat:
            case SCcomdef:
            case SCpseudo:
            case SCinline:
            case SCsinline:
            case SCeinline:
                s->Sflags |= SFLlivexit;
                /* FALL-THROUGH */
            case SCauto:
            case SCregister:
            case SCfastpar:
            case SCbprel:
                if (e->Eoper == OPrelconst)
                {
                    s->Sflags &= ~(SFLunambig | GTregcand);
                }
                else if (s->ty() & mTYfar)
                    e->Ety |= mTYfar;
                break;
        }
        break;
    case OPstring:
    case OPconst:
    case OPstrthis:
        break;
    case OPsizeof:
        assert(0);

        break;
    }
}

/*************************************
 * Determine register candidates.
 */

STATIC void out_regcand_walk(elem *e);

void out_regcand(symtab_t *psymtab)
{
    block *b;
    SYMIDX si;
    int ifunc;

    //printf("out_regcand()\n");
    ifunc = (tybasic(funcsym_p->ty()) == TYifunc);
    for (si = 0; si < psymtab->top; si++)
    {   symbol *s = psymtab->tab[si];

        symbol_debug(s);
        //assert(sytab[s->Sclass] & SCSS);      // only stack variables
        s->Ssymnum = si;                        // Ssymnum trashed by cpp_inlineexpand
        if (!(s->ty() & mTYvolatile) &&
            !(ifunc && (s->Sclass == SCparameter || s->Sclass == SCregpar)) &&
            s->Sclass != SCstatic)
            s->Sflags |= (GTregcand | SFLunambig);      // assume register candidate
        else
            s->Sflags &= ~(GTregcand | SFLunambig);
    }

    addrparam = FALSE;                  // haven't taken addr of param yet
    for (b = startblock; b; b = b->Bnext)
    {
        if (b->Belem)
            out_regcand_walk(b->Belem);

        // Any assembler blocks make everything ambiguous
        if (b->BC == BCasm)
            for (si = 0; si < psymtab->top; si++)
                psymtab->tab[si]->Sflags &= ~(SFLunambig | GTregcand);
    }

    // If we took the address of one parameter, assume we took the
    // address of all non-register parameters.
    if (addrparam)                      // if took address of a parameter
    {
        for (si = 0; si < psymtab->top; si++)
            if (psymtab->tab[si]->Sclass == SCparameter || psymtab->tab[si]->Sclass == SCshadowreg)
                psymtab->tab[si]->Sflags &= ~(SFLunambig | GTregcand);
    }

}

STATIC void out_regcand_walk(elem *e)
{   symbol *s;

    while (1)
    {   elem_debug(e);

        if (EBIN(e))
        {   if (e->Eoper == OPstreq)
            {   if (e->E1->Eoper == OPvar)
                {   s = e->E1->EV.sp.Vsym;
                    s->Sflags &= ~(SFLunambig | GTregcand);
                }
                if (e->E2->Eoper == OPvar)
                {   s = e->E2->EV.sp.Vsym;
                    s->Sflags &= ~(SFLunambig | GTregcand);
                }
            }
            out_regcand_walk(e->E1);
            e = e->E2;
        }
        else if (EUNA(e))
        {
            // Don't put 'this' pointers in registers if we need
            // them for EH stack cleanup.
            if (e->Eoper == OPctor)
            {   elem *e1 = e->E1;

                if (e1->Eoper == OPadd)
                    e1 = e1->E1;
                if (e1->Eoper == OPvar)
                    e1->EV.sp.Vsym->Sflags &= ~GTregcand;
            }
            e = e->E1;
        }
        else
        {   if (e->Eoper == OPrelconst)
            {
                s = e->EV.sp.Vsym;
                assert(s);
                symbol_debug(s);
                switch (s->Sclass)
                {
                    case SCregpar:
                    case SCparameter:
                    case SCshadowreg:
                        addrparam = TRUE;       // taking addr of param list
                        break;
                    case SCauto:
                    case SCregister:
                    case SCfastpar:
                    case SCbprel:
                        s->Sflags &= ~(SFLunambig | GTregcand);
                        break;
                }
            }
            else if (e->Eoper == OPvar)
            {
                if (e->EV.sp.Voffset)
                {   if (!(e->EV.sp.Voffset == 1 && tybyte(e->Ety)))
                        e->EV.sp.Vsym->Sflags &= ~GTregcand;
                }
            }
            break;
        }
    }
}

/**************************
 * Optimize function,
 * generate code for it,
 * and write it out.
 */

STATIC void writefunc2(symbol *sfunc);

void writefunc(symbol *sfunc)
{
    cstate.CSpsymtab = &globsym;
    writefunc2(sfunc);
    cstate.CSpsymtab = NULL;
}

STATIC void writefunc2(symbol *sfunc)
{
    block *b;
    unsigned nsymbols;
    SYMIDX si;
    int anyasm;
    int csegsave;                       // for OMF
    func_t *f = sfunc->Sfunc;
    tym_t tyf;

    //printf("writefunc(%s)\n",sfunc->Sident);
    debug(debugy && dbg_printf("writefunc(%s)\n",sfunc->Sident));

    /* Signify that function has been output                    */
    /* (before inline_do() to prevent infinite recursion!)      */
    f->Fflags &= ~Fpending;
    f->Fflags |= Foutput;

    if (
        (eecontext.EEcompile && eecontext.EEfunc != sfunc))
        return;

    /* Copy local symbol table onto main one, making sure       */
    /* that the symbol numbers are adjusted accordingly */
    //dbg_printf("f->Flocsym.top = %d\n",f->Flocsym.top);
    nsymbols = f->Flocsym.top;
    if (nsymbols > globsym.symmax)
    {   /* Reallocate globsym.tab[]     */
        globsym.symmax = nsymbols;
        globsym.tab = symtab_realloc(globsym.tab, globsym.symmax);
    }
    debug(debugy && dbg_printf("appending symbols to symtab...\n"));
    assert(globsym.top == 0);
    memcpy(&globsym.tab[0],&f->Flocsym.tab[0],nsymbols * sizeof(symbol *));
    globsym.top = nsymbols;

    assert(startblock == NULL);
    if (f->Fflags & Finline)            // if keep function around
    {   // Generate copy of function
        block *bf;
        block **pb;

        pb = &startblock;
        for (bf = f->Fstartblock; bf; bf = bf->Bnext)
        {
            b = block_calloc();
            *pb = b;
            pb = &b->Bnext;

            *b = *bf;
            assert(b->numSucc() == 0);
            assert(!b->Bpred);
            b->Belem = el_copytree(b->Belem);
        }
    }
    else
    {   startblock = sfunc->Sfunc->Fstartblock;
        sfunc->Sfunc->Fstartblock = NULL;
    }
    assert(startblock);

    /* Do any in-line expansion of function calls inside sfunc  */
    assert(funcsym_p == NULL);
    funcsym_p = sfunc;
    tyf = tybasic(sfunc->ty());

    // TX86 computes parameter offsets in stackoffsets()
    //printf("globsym.top = %d\n", globsym.top);

    for (si = 0; si < globsym.top; si++)
    {   symbol *s = globsym.tab[si];

        symbol_debug(s);
        //printf("symbol %d '%s'\n",si,s->Sident);

        type_size(s->Stype);    // do any forward template instantiations

        s->Ssymnum = si;        // Ssymnum trashed by cpp_inlineexpand
        s->Sflags &= ~(SFLunambig | GTregcand);
        switch (s->Sclass)
        {
            case SCbprel:
                s->Sfl = FLbprel;
                goto L3;
            case SCauto:
            case SCregister:
                s->Sfl = FLauto;
                goto L3;
            case SCfastpar:
                s->Sfl = FLfast;
                goto L3;
            case SCregpar:
            case SCparameter:
            case SCshadowreg:
                s->Sfl = FLpara;
                if (tyf == TYifunc)
                {   s->Sflags |= SFLlivexit;
                    break;
                }
            L3:
                if (!(s->ty() & mTYvolatile))
                    s->Sflags |= GTregcand | SFLunambig; // assume register candidate   */
                break;
            case SCpseudo:
                s->Sfl = FLpseudo;
                break;
            case SCstatic:
                break;                  // already taken care of by datadef()
            case SCstack:
                s->Sfl = FLstack;
                break;
            default:
                symbol_print(s);
                assert(0);
        }
    }

    addrparam = FALSE;                  // haven't taken addr of param yet
    anyasm = 0;
    numblks = 0;
    for (b = startblock; b; b = b->Bnext)
    {
        numblks++;                              // redo count
        memset(&b->_BLU,0,sizeof(b->_BLU));
        if (b->Belem)
        {   outelem(b->Belem);
#if MARS
            if (b->Belem->Eoper == OPhalt)
            {   b->BC = BCexit;
                list_free(&b->Bsucc,FPNULL);
            }
#endif
        }
        if (b->BC == BCasm)
            anyasm = 1;
        if (sfunc->Sflags & SFLexit && (b->BC == BCret || b->BC == BCretexp))
        {   b->BC = BCexit;
            list_free(&b->Bsucc,FPNULL);
        }
        assert(b != b->Bnext);
    }
    PARSER = 0;
    if (eecontext.EEelem)
    {   unsigned marksi = globsym.top;

        eecontext.EEin++;
        outelem(eecontext.EEelem);
        eecontext.EEelem = doptelem(eecontext.EEelem,TRUE);
        eecontext.EEin--;
        eecontext_convs(marksi);
    }
    maxblks = 3 * numblks;              // allow for increase in # of blocks
    // If we took the address of one parameter, assume we took the
    // address of all non-register parameters.
    if (addrparam | anyasm)             // if took address of a parameter
    {
        for (si = 0; si < globsym.top; si++)
            if (anyasm || globsym.tab[si]->Sclass == SCparameter)
                globsym.tab[si]->Sflags &= ~(SFLunambig | GTregcand);
    }

    block_pred();                       // compute predecessors to blocks
    block_compbcount();                 // eliminate unreachable blocks
    if (go.mfoptim)
    {   OPTIMIZER = 1;
        optfunc();                      /* optimize function            */
        assert(dfo);
        OPTIMIZER = 0;
    }
    else
    {
        //dbg_printf("blockopt()\n");
        blockopt(0);                    /* optimize                     */
    }

    assert(funcsym_p == sfunc);
    if (eecontext.EEcompile != 1)
    {
        if (symbol_iscomdat(sfunc))
        {
            csegsave = cseg;
            objmod->comdat(sfunc);
        }
        else
            if (config.flags & CFGsegs) // if user set switch for this
            {
                objmod->codeseg(funcsym_p->Sident, 1);
                                        // generate new code segment
            }
        cod3_align();                   // align start of function
        objmod->func_start(sfunc);
        searchfixlist(sfunc);           // backpatch any refs to this function
    }

    //dbg_printf("codgen()\n");
        codgen();                               // generate code
    //dbg_printf("after codgen for %s Coffset %x\n",sfunc->Sident,Coffset);
    blocklist_free(&startblock);
    objmod->func_term(sfunc);
    if (eecontext.EEcompile == 1)
        goto Ldone;
    if (sfunc->Sclass == SCglobal)
    {
        if ((config.objfmt == OBJ_OMF || config.objfmt == OBJ_MSCOFF) && !(config.flags4 & CFG4allcomdat))
        {
            assert(sfunc->Sseg == cseg);
            objmod->pubdef(sfunc->Sseg,sfunc,sfunc->Soffset);       // make a public definition
        }
    }
    if (config.wflags & WFexpdef &&
        sfunc->Sclass != SCstatic &&
        sfunc->Sclass != SCsinline &&
        !(sfunc->Sclass == SCinline && !(config.flags2 & CFG2comdat)) &&
        sfunc->ty() & mTYexport)
        objmod->export_symbol(sfunc,Para.offset);      // export function definition

    if (config.fulltypes && config.fulltypes != CV8)
        cv_func(sfunc);                 // debug info for function

#if MARS
    /* This is to make uplevel references to SCfastpar variables
     * from nested functions work.
     */
    for (si = 0; si < globsym.top; si++)
    {
        Symbol *s = globsym.tab[si];

        switch (s->Sclass)
        {   case SCfastpar:
                s->Sclass = SCauto;
                break;
        }
    }
    /* After codgen() and writing debug info for the locals,
     * readjust the offsets of all stack variables so they
     * are relative to the frame pointer.
     * Necessary for nested function access to lexically enclosing frames.
     */
     cod3_adjSymOffsets();
#endif

    if ((config.objfmt == OBJ_OMF || config.objfmt == OBJ_MSCOFF) &&
        symbol_iscomdat(sfunc))         // if generated a COMDAT
        objmod->setcodeseg(csegsave);       // reset to real code seg

    /* Check if function is a constructor or destructor, by     */
    /* seeing if the function name starts with _STI or _STD     */
    {
#if _M_I86
        short *p = (short *) sfunc->Sident;
        if (p[0] == 'S_' && (p[1] == 'IT' || p[1] == 'DT'))
#else
        char *p = sfunc->Sident;
        if (p[0] == '_' && p[1] == 'S' && p[2] == 'T' &&
            (p[3] == 'I' || p[3] == 'D'))
#endif
            objmod->funcptr(sfunc);
    }

Ldone:
    funcsym_p = NULL;
    globsym.top = 0;

    //dbg_printf("done with writefunc()\n");
    util_free(dfo);
    dfo = NULL;
}

/*************************
 * Align segment offset.
 * Input:
 *      seg             segment to be aligned
 *      datasize        size in bytes of object to be aligned
 */

void alignOffset(int seg,targ_size_t datasize)
{
    targ_size_t alignbytes;

    alignbytes = align(datasize,Offset(seg)) - Offset(seg);
    //dbg_printf("seg %d datasize = x%x, Offset(seg) = x%x, alignbytes = x%x\n",
      //seg,datasize,Offset(seg),alignbytes);
    if (alignbytes)
        objmod->lidata(seg,Offset(seg),alignbytes);
}


/***************************************
 * Write data into read-only data segment.
 * Return symbol for it.
 */

#define ROMAX 32
struct Readonly
{
    symbol *sym;
    size_t length;
    unsigned char p[ROMAX];
};

#define RMAX 16
static Readonly readonly[RMAX];
static size_t readonly_length;
static size_t readonly_i;

void out_reset()
{
    readonly_length = 0;
    readonly_i = 0;
}

symbol *out_readonly_sym(tym_t ty, void *p, int len)
{
#if 0
    printf("out_readonly_sym(ty = x%x)\n", ty);
    for (int i = 0; i < len; i++)
        printf(" [%d] = %02x\n", i, ((unsigned char*)p)[i]);
#endif
    // Look for previous symbol we can reuse
    for (int i = 0; i < readonly_length; i++)
    {
        Readonly *r = &readonly[i];
        if (r->length == len && memcmp(p, r->p, len) == 0)
            return r->sym;
    }

    symbol *s;

    if (config.objfmt == OBJ_ELF ||
        (MARS && (config.objfmt == OBJ_OMF || config.objfmt == OBJ_MSCOFF)))
    {
        s = objmod->sym_cdata(ty, (char *)p, len);
    }
    else
    {
        unsigned sz = tysize(ty);

        alignOffset(DATA, sz);
        s = symboldata(Doffset,ty | mTYconst);
        s->Sseg = DATA;
        objmod->write_bytes(SegData[DATA], len, p);
        //printf("s->Sseg = %d:x%x\n", s->Sseg, s->Soffset);
    }

    if (len <= ROMAX)
    {   Readonly *r;

        if (readonly_length < RMAX)
        {
            r = &readonly[readonly_length];
            readonly_length++;
        }
        else
        {   r = &readonly[readonly_i];
            readonly_i++;
            if (readonly_i >= RMAX)
                readonly_i = 0;
        }
        r->length = len;
        r->sym = s;
        memcpy(r->p, p, len);
    }
    return s;
}

void Srcpos::print(const char *func)
{
    printf("%s(", func);
#if MARS
    printf("Sfilename = %s", Sfilename ? Sfilename : "null");
#else
    Sfile *sf = Sfilptr ? *Sfilptr : NULL;
    printf("Sfilptr = %p (filename = %s)", sf, sf ? sf->SFname : "null");
#endif
    printf(", Slinnum = %u", Slinnum);
    printf(")\n");
}
