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
#include        <stdlib.h>
#include        <time.h>

#include        "cc.h"
#include        "global.h"
#include        "type.h"
#include        "dt.h"

#include        "el.h"
#include        "oper.h"                /* for OPMAX            */
#include        "token.h"

#include        "code.h"

static char __file__[] = __FILE__;      /* for tassert.h                */
#include        "tassert.h"

//STATIC void symbol_undef(symbol *s);
STATIC void symbol_freemember(symbol *s);
STATIC void mptr_hydrate(mptr_t **);
STATIC void mptr_dehydrate(mptr_t **);
STATIC void baseclass_hydrate(baseclass_t **);
STATIC void baseclass_dehydrate(baseclass_t **);

/*********************************
 * Allocate/free symbol table.
 */

symbol **symtab_realloc(symbol **tab, size_t symmax)
{   symbol **newtab;

    if (config.flags2 & (CFG2phgen | CFG2phuse | CFG2phauto | CFG2phautoy))
    {
        newtab = (symbol **) MEM_PH_REALLOC(tab, symmax * sizeof(symbol *));
    }
    else
    {
        newtab = (symbol **) realloc(tab, symmax * sizeof(symbol *));
        if (!newtab)
            err_nomem();
    }
    return newtab;
}

symbol **symtab_malloc(size_t symmax)
{   symbol **newtab;

    if (config.flags2 & (CFG2phgen | CFG2phuse | CFG2phauto | CFG2phautoy))
    {
        newtab = (symbol **) MEM_PH_MALLOC(symmax * sizeof(symbol *));
    }
    else
    {
        newtab = (symbol **) malloc(symmax * sizeof(symbol *));
        if (!newtab)
            err_nomem();
    }
    return newtab;
}

symbol **symtab_calloc(size_t symmax)
{   symbol **newtab;

    if (config.flags2 & (CFG2phgen | CFG2phuse | CFG2phauto | CFG2phautoy))
    {
        newtab = (symbol **) MEM_PH_CALLOC(symmax * sizeof(symbol *));
    }
    else
    {
        newtab = (symbol **) calloc(symmax, sizeof(symbol *));
        if (!newtab)
            err_nomem();
    }
    return newtab;
}

void symtab_free(symbol **tab)
{
    if (config.flags2 & (CFG2phgen | CFG2phuse | CFG2phauto | CFG2phautoy))
        MEM_PH_FREE(tab);
    else if (tab)
        free(tab);
}

/*******************************
 * Type out symbol information.
 */

void symbol_print(symbol *)
{
}


/*********************************
 * Terminate use of symbol table.
 */

static symbol *keep;

void symbol_term()
{
    symbol_free(keep);
}

/****************************************
 * Keep symbol around until symbol_term().
 */

#if TERMCODE

void symbol_keep(symbol *s)
{
    symbol_debug(s);
    s->Sr = keep;       // use Sr so symbol_free() doesn't nest
    keep = s;
}

#endif

/****************************************
 * Return alignment of symbol.
 */
int Symbol::Salignsize()
{
    if (Salignment > 0)
        return Salignment;
    int alignsize = type_alignsize(Stype);

    /* Reduce alignment faults when SIMD vectors
     * are reinterpreted cast to other types with less alignment.
     */
    if (config.fpxmmregs && alignsize < 16 &&
        Sclass == SCauto &&
        type_size(Stype) == 16)
    {
        alignsize = 16;
    }

    return alignsize;
}

/****************************************
 * Return if symbol is dead.
 */

bool Symbol::Sisdead(bool anyiasm)
{
    return Sflags & SFLdead ||
           /* SFLdead means the optimizer found no references to it.
            * The rest deals with variables that the compiler never needed
            * to read from memory because they were cached in registers,
            * and so no memory needs to be allocated for them.
            * Code that does write those variables to memory gets NOPed out
            * during address assignment.
            */
           (!anyiasm && !(Sflags & SFLread) && Sflags & SFLunambig &&
            // mTYvolatile means this variable has been reference by a nested function
            !(Stype->Tty & mTYvolatile) &&
            (config.flags4 & CFG4optimized || !config.fulltypes));
}

/***********************************
 * Get user name of symbol.
 */

char *symbol_ident(symbol *s)
{
    return s->Sident;
}

/****************************************
 * Create a new symbol.
 */

symbol * symbol_calloc(const char *id)
{   symbol *s;
    int len;

    len = strlen(id);
    //printf("sizeof(symbol)=%d, sizeof(s->Sident)=%d, len=%d\n",sizeof(symbol),sizeof(s->Sident),len);
    s = (symbol *) mem_fmalloc(sizeof(symbol) - sizeof(s->Sident) + len + 1 + 5);
    memset(s,0,sizeof(symbol) - sizeof(s->Sident));
#ifdef DEBUG
    if (debugy)
        dbg_printf("symbol_calloc('%s') = %p\n",id,s);
    s->id = IDsymbol;
#endif
    memcpy(s->Sident,id,len + 1);
    s->Ssymnum = -1;
    return s;
}

/****************************************
 * Create a symbol, given a name and type.
 */

symbol * symbol_name(const char *name,int sclass,type *t)
{
    type_debug(t);
    symbol *s = symbol_calloc(name);
    s->Sclass = (enum SC) sclass;
    s->Stype = t;
    s->Stype->Tcount++;

    if (tyfunc(t->Tty))
        symbol_func(s);
    return s;
}

/****************************************
 * Create a symbol that is an alias to another function symbol.
 */

Funcsym *symbol_funcalias(Funcsym *sf)
{
    Funcsym *s;

    symbol_debug(sf);
    assert(tyfunc(sf->Stype->Tty));
    if (sf->Sclass == SCfuncalias)
        sf = sf->Sfunc->Falias;
    s = (Funcsym *)symbol_name(sf->Sident,SCfuncalias,sf->Stype);
    s->Sfunc->Falias = sf;
    return s;
}

/****************************************
 * Create a symbol, give it a name, storage class and type.
 */

symbol * symbol_generate(int sclass,type *t)
{
    static int tmpnum;
    char name[4 + sizeof(tmpnum) * 3 + 1];

    //printf("symbol_generate(_TMP%d)\n", tmpnum);
    sprintf(name,"_TMP%d",tmpnum++);
    symbol *s = symbol_name(name,sclass,t);
    //symbol_print(s);
    s->Sflags |= SFLnodebug | SFLartifical;
    return s;
}

/****************************************
 * Generate an auto symbol, and add it to the symbol table.
 */

symbol * symbol_genauto(type *t)
{   symbol *s;

    s = symbol_generate(SCauto,t);
    s->Sflags |= SFLfree;
    symbol_add(s);
    return s;
}

/******************************************
 * Generate symbol into which we can copy the contents of expression e.
 */

Symbol *symbol_genauto(elem *e)
{
    return symbol_genauto(type_fake(e->Ety));
}

/******************************************
 * Generate symbol into which we can copy the contents of expression e.
 */

Symbol *symbol_genauto(tym_t ty)
{
    return symbol_genauto(type_fake(ty));
}

/****************************************
 * Add in the variants for a function symbol.
 */

void symbol_func(symbol *s)
{
    //printf("symbol_func(%s, x%x)\n", s->Sident, fregsaved);
    symbol_debug(s);
    s->Sfl = FLfunc;
    // Interrupt functions modify all registers
    // BUG: do interrupt functions really save BP?
    // Note that fregsaved may not be set yet
    s->Sregsaved = (s->Stype && tybasic(s->Stype->Tty) == TYifunc) ? mBP : fregsaved;
    s->Sseg = UNKNOWN;          // don't know what segment it is in
    if (!s->Sfunc)
        s->Sfunc = func_calloc();
}

/***************************************
 * Add a field to a struct s.
 * Input:
 *      s       the struct symbol
 *      name    field name
 *      t       the type of the field
 *      offset  offset of the field
 */

void symbol_struct_addField(Symbol *s, const char *name, type *t, unsigned offset)
{
    Symbol *s2 = symbol_name(name, SCmember, t);
    s2->Smemoff = offset;
    list_append(&s->Sstruct->Sfldlst, s2);
}

/********************************
 * Check integrity of symbol data structure.
 */

#ifdef DEBUG

void symbol_check(symbol *s)
{
    //dbg_printf("symbol_check('%s',%p)\n",s->Sident,s);
    symbol_debug(s);
    if (s->Stype) type_debug(s->Stype);
    assert((unsigned)s->Sclass < (unsigned)SCMAX);
}

void symbol_tree_check(symbol *s)
{
    while (s)
    {   symbol_check(s);
        symbol_tree_check(s->Sl);
        s = s->Sr;
    }
}

#endif

/*************************************
 * Search for symbol in multiple symbol tables,
 * starting with most recently nested one.
 * Input:
 *      p ->    identifier string
 * Returns:
 *      pointer to symbol
 *      NULL if couldn't find it
 */

/*********************************
 * Delete symbol from symbol table, taking care to delete
 * all children of a symbol.
 * Make sure there are no more forward references (labels, tags).
 * Input:
 *      pointer to a symbol
 */

void meminit_free(meminit_t *m)         /* helper for symbol_free()     */
{
    list_free(&m->MIelemlist,(list_free_fp)el_free);
    MEM_PARF_FREE(m);
}

void symbol_free(symbol *s)
{
    while (s)                           /* if symbol exists             */
    {   symbol *sr;

#ifdef DEBUG
        if (debugy)
            dbg_printf("symbol_free('%s',%p)\n",s->Sident,s);
        symbol_debug(s);
        assert(/*s->Sclass != SCunde &&*/ (int) s->Sclass < (int) SCMAX);
#endif
        {   type *t = s->Stype;

            if (t)
                type_debug(t);
            if (t && tyfunc(t->Tty) && s->Sfunc)
            {
                func_t *f = s->Sfunc;

                debug(assert(f));
                blocklist_free(&f->Fstartblock);
                freesymtab(f->Flocsym.tab,0,f->Flocsym.top);

                symtab_free(f->Flocsym.tab);
                list_free(&f->Fsymtree,(list_free_fp)symbol_free);
                free(f->typesTable);
                func_free(f);
            }
            switch (s->Sclass)
            {
                case SCstruct:
                  {
#ifdef DEBUG
                    if (debugy)
                        dbg_printf("freeing members %p\n",s->Sstruct->Sfldlst);
#endif
                    list_free(&s->Sstruct->Sfldlst,FPNULL);
                    symbol_free(s->Sstruct->Sroot);
                    struct_free(s->Sstruct);
                  }
                    break;
                case SCenum:
                    /* The actual member symbols are either in a local  */
                    /* table or on the member list of a class, so we    */
                    /* don't free them here.                            */
                    assert(s->Senum);
                    list_free(&s->Senumlist,FPNULL);
                    MEM_PH_FREE(s->Senum);
                    s->Senum = NULL;
                    break;

                case SCparameter:
                case SCregpar:
                case SCfastpar:
                case SCshadowreg:
                case SCregister:
                case SCauto:
                    vec_free(s->Srange);
                    /* FALL-THROUGH */
                    break;
                default:
                    break;
            }
            if (s->Sflags & (SFLvalue | SFLdtorexp))
                el_free(s->Svalue);
            if (s->Sdt)
                dt_free(s->Sdt);
            type_free(t);
            symbol_free(s->Sl);
            sr = s->Sr;
#ifdef DEBUG
            s->id = 0;
#endif
            mem_ffree(s);
        }
        s = sr;
    }
}

/*****************************
 * Add symbol to current symbol array.
 */

SYMIDX symbol_add(symbol *s)
{   SYMIDX sitop;

    //printf("symbol_add('%s')\n", s->Sident);
#ifdef DEBUG
    if (!s || !s->Sident[0])
    {   dbg_printf("bad symbol\n");
        assert(0);
    }
#endif
    symbol_debug(s);
    if (pstate.STinsizeof)
    {   symbol_keep(s);
        return -1;
    }
    debug(assert(cstate.CSpsymtab));
    sitop = cstate.CSpsymtab->top;
    assert(sitop <= cstate.CSpsymtab->symmax);
    if (sitop == cstate.CSpsymtab->symmax)
    {
#ifdef DEBUG
#define SYMINC  1                       /* flush out reallocation bugs  */
#else
#define SYMINC  99
#endif
        cstate.CSpsymtab->symmax += (cstate.CSpsymtab == &globsym) ? SYMINC : 1;
        //assert(cstate.CSpsymtab->symmax * sizeof(symbol *) < 4096 * 4);
        cstate.CSpsymtab->tab = symtab_realloc(cstate.CSpsymtab->tab, cstate.CSpsymtab->symmax);
    }
    cstate.CSpsymtab->tab[sitop] = s;
#ifdef DEBUG
    if (debugy)
        dbg_printf("symbol_add(%p '%s') = %d\n",s,s->Sident,cstate.CSpsymtab->top);
#endif
    assert(s->Ssymnum == -1);
    return s->Ssymnum = cstate.CSpsymtab->top++;
}

/****************************
 * Free up the symbol table, from symbols n1 through n2, not
 * including n2.
 */

void freesymtab(symbol **stab,SYMIDX n1,SYMIDX n2)
{   SYMIDX si;

    if (!stab)
        return;
#ifdef DEBUG
    if (debugy)
        dbg_printf("freesymtab(from %d to %d)\n",n1,n2);
#endif
    assert(stab != globsym.tab || (n1 <= n2 && n2 <= globsym.top));
    for (si = n1; si < n2; si++)
    {   symbol *s;

        s = stab[si];
        if (s && s->Sflags & SFLfree)
        {   stab[si] = NULL;
#ifdef DEBUG
            if (debugy)
                dbg_printf("Freeing %p '%s' (%d)\n",s,s->Sident,si);
            symbol_debug(s);
#endif
            s->Sl = s->Sr = NULL;
            s->Ssymnum = -1;
            symbol_free(s);
        }
    }
}

/****************************
 * Create a copy of a symbol.
 */

symbol * symbol_copy(symbol *s)
{   symbol *scopy;
    type *t;

    symbol_debug(s);
    /*dbg_printf("symbol_copy(%s)\n",s->Sident);*/
    scopy = symbol_calloc(s->Sident);
    memcpy(scopy,s,sizeof(symbol) - sizeof(s->Sident));
    scopy->Sl = scopy->Sr = scopy->Snext = NULL;
    scopy->Ssymnum = -1;
    if (scopy->Sdt)
    {
        DtBuilder dtb;
        dtb.nzeros(type_size(scopy->Stype));
        scopy->Sdt = dtb.finish();
    }
    if (scopy->Sflags & (SFLvalue | SFLdtorexp))
        scopy->Svalue = el_copytree(s->Svalue);
    t = scopy->Stype;
    if (t)
    {   t->Tcount++;            /* one more parent of the type  */
        type_debug(t);
    }
    return scopy;
}

/*******************************************
 * Hydrate a symbol tree.
 */

#if HYDRATE
void symbol_tree_hydrate(symbol **ps)
{   symbol *s;

    while (isdehydrated(*ps))           /* if symbol is dehydrated      */
    {
        s = symbol_hydrate(ps);
        symbol_debug(s);
        if (s->Scover)
            symbol_hydrate(&s->Scover);
        symbol_tree_hydrate(&s->Sl);
        ps = &s->Sr;
    }

}
#endif

/*******************************************
 * Dehydrate a symbol tree.
 */

#if DEHYDRATE
void symbol_tree_dehydrate(symbol **ps)
{   symbol *s;

    while ((s = *ps) != NULL && !isdehydrated(s)) /* if symbol exists   */
    {
        symbol_debug(s);
        symbol_dehydrate(ps);
#if DEBUG_XSYMGEN
        if (xsym_gen && ph_in_head(s))
            return;
#endif
        symbol_dehydrate(&s->Scover);
        symbol_tree_dehydrate(&s->Sl);
        ps = &s->Sr;
    }
}
#endif

/*******************************************
 * Hydrate a symbol.
 */

#if HYDRATE
symbol *symbol_hydrate(symbol **ps)
{   symbol *s;

    s = *ps;
    if (isdehydrated(s))                /* if symbol is dehydrated      */
    {   type *t;
        struct_t *st;

        s = (symbol *) ph_hydrate(ps);
#ifdef DEBUG
        debugy && dbg_printf("symbol_hydrate('%s')\n",s->Sident);
#endif
        symbol_debug(s);
        if (!isdehydrated(s->Stype))    // if this symbol is already dehydrated
            return s;                   // no need to do it again
        if (pstate.SThflag != FLAG_INPLACE && s->Sfl != FLreg)
            s->Sxtrnnum = 0;            // not written to .OBJ file yet
        type_hydrate(&s->Stype);
        //dbg_printf("symbol_hydrate(%p, '%s', t = %p)\n",s,s->Sident,s->Stype);
        t = s->Stype;
        if (t)
            type_debug(t);

        if (t && tyfunc(t->Tty) && ph_hydrate(&s->Sfunc))
        {
            func_t *f = s->Sfunc;
            SYMIDX si;

            debug(assert(f));

            list_hydrate(&f->Fsymtree,(list_free_fp)symbol_tree_hydrate);
            blocklist_hydrate(&f->Fstartblock);

            ph_hydrate(&f->Flocsym.tab);
            for (si = 0; si < f->Flocsym.top; si++)
                symbol_hydrate(&f->Flocsym.tab[si]);

            srcpos_hydrate(&f->Fstartline);
            srcpos_hydrate(&f->Fendline);

            symbol_hydrate(&f->F__func__);

        }
        switch (s->Sclass)
        {
            case SCstruct:
              {
                ph_hydrate(&s->Sstruct);
                symbol_tree_hydrate(&s->Sstruct->Sroot);
                list_hydrate(&s->Sstruct->Sfldlst,(list_free_fp)symbol_hydrate);
              }
                break;

            case SCenum:
                assert(s->Senum);
                ph_hydrate(&s->Senum);
                break;

            case SCtemplate:
            {   template_t *tm;

                tm = (template_t *) ph_hydrate(&s->Stemplate);
                list_hydrate(&tm->TMinstances,(list_free_fp)symbol_hydrate);
                list_hydrate(&tm->TMfriends,(list_free_fp)symbol_hydrate);
                param_hydrate(&tm->TMptpl);
                param_hydrate(&tm->TMptal);
                token_hydrate(&tm->TMbody);
                list_hydrate(&tm->TMmemberfuncs,(list_free_fp)tmf_hydrate);
                list_hydrate(&tm->TMexplicit,(list_free_fp)tme_hydrate);
                list_hydrate(&tm->TMnestedexplicit,(list_free_fp)tmne_hydrate);
                list_hydrate(&tm->TMnestedfriends,(list_free_fp)tmnf_hydrate);
                ph_hydrate(&tm->TMnext);
                symbol_hydrate(&tm->TMpartial);
                symbol_hydrate(&tm->TMprimary);
                break;
            }

            case SCnamespace:
                symbol_tree_hydrate(&s->Snameroot);
                list_hydrate(&s->Susing,(list_free_fp)symbol_hydrate);
                break;

            case SCmemalias:
            case SCfuncalias:
            case SCadl:
                list_hydrate(&s->Spath,(list_free_fp)symbol_hydrate);
            case SCalias:
                ph_hydrate(&s->Smemalias);
                break;

            default:
                if (s->Sflags & (SFLvalue | SFLdtorexp))
                    el_hydrate(&s->Svalue);
                break;
        }
        {   dt_t **pdt,*dt;

            for (pdt = &s->Sdt; isdehydrated(*pdt); pdt = &dt->DTnext)
            {
                dt = (dt_t *) ph_hydrate(pdt);
                switch (dt->dt)
                {   case DT_abytes:
                    case DT_nbytes:
                        ph_hydrate(&dt->DTpbytes);
                        break;
                    case DT_xoff:
                        symbol_hydrate(&dt->DTsym);
                        break;
                }
            }
        }
        if (s->Scover)
            symbol_hydrate(&s->Scover);
    }
    return s;
}
#endif

/*******************************************
 * Dehydrate a symbol.
 */

#if DEHYDRATE
void symbol_dehydrate(symbol **ps)
{
    symbol *s;

    if ((s = *ps) != NULL && !isdehydrated(s)) /* if symbol exists      */
    {   type *t;
        struct_t *st;

#ifdef DEBUG
        if (debugy)
            dbg_printf("symbol_dehydrate('%s')\n",s->Sident);
#endif
        ph_dehydrate(ps);
#if DEBUG_XSYMGEN
        if (xsym_gen && ph_in_head(s))
            return;
#endif
        symbol_debug(s);
        t = s->Stype;
        if (isdehydrated(t))
            return;
        type_dehydrate(&s->Stype);

        if (tyfunc(t->Tty) && !isdehydrated(s->Sfunc))
        {
            func_t *f = s->Sfunc;
            SYMIDX si;

            debug(assert(f));
            ph_dehydrate(&s->Sfunc);

            list_dehydrate(&f->Fsymtree,(list_free_fp)symbol_tree_dehydrate);
            blocklist_dehydrate(&f->Fstartblock);
            assert(!isdehydrated(&f->Flocsym.tab));

#if DEBUG_XSYMGEN
            if (!xsym_gen || !ph_in_head(f->Flocsym.tab))

#endif
            for (si = 0; si < f->Flocsym.top; si++)
                symbol_dehydrate(&f->Flocsym.tab[si]);
            ph_dehydrate(&f->Flocsym.tab);

            srcpos_dehydrate(&f->Fstartline);
            srcpos_dehydrate(&f->Fendline);
            symbol_dehydrate(&f->F__func__);
        }
        switch (s->Sclass)
        {
            case SCstruct:
              {
                symbol_tree_dehydrate(&s->Sstruct->Sroot);
                list_dehydrate(&s->Sstruct->Sfldlst,(list_free_fp)symbol_dehydrate);
                ph_dehydrate(&s->Sstruct);
              }
                break;

            case SCenum:
                assert(s->Senum);
                if (!isdehydrated(s->Senum))
                {
                    ph_dehydrate(&s->Senum);
                }
                break;

            case SCtemplate:
            {   template_t *tm;

                tm = s->Stemplate;
                if (!isdehydrated(tm))
                {
                    ph_dehydrate(&s->Stemplate);
                    list_dehydrate(&tm->TMinstances,(list_free_fp)symbol_dehydrate);
                    list_dehydrate(&tm->TMfriends,(list_free_fp)symbol_dehydrate);
                    list_dehydrate(&tm->TMnestedfriends,(list_free_fp)tmnf_dehydrate);
                    param_dehydrate(&tm->TMptpl);
                    param_dehydrate(&tm->TMptal);
                    token_dehydrate(&tm->TMbody);
                    list_dehydrate(&tm->TMmemberfuncs,(list_free_fp)tmf_dehydrate);
                    list_dehydrate(&tm->TMexplicit,(list_free_fp)tme_dehydrate);
                    list_dehydrate(&tm->TMnestedexplicit,(list_free_fp)tmne_dehydrate);
                    ph_dehydrate(&tm->TMnext);
                    symbol_dehydrate(&tm->TMpartial);
                    symbol_dehydrate(&tm->TMprimary);
                }
                break;
            }

            case SCnamespace:
                symbol_tree_dehydrate(&s->Snameroot);
                list_dehydrate(&s->Susing,(list_free_fp)symbol_dehydrate);
                break;

            case SCmemalias:
            case SCfuncalias:
            case SCadl:
                list_dehydrate(&s->Spath,(list_free_fp)symbol_dehydrate);
            case SCalias:
                ph_dehydrate(&s->Smemalias);
                break;

            default:
                if (s->Sflags & (SFLvalue | SFLdtorexp))
                    el_dehydrate(&s->Svalue);
                break;
        }
        {   dt_t **pdt,*dt;

            for (pdt = &s->Sdt;
                 (dt = *pdt) != NULL && !isdehydrated(dt);
                 pdt = &dt->DTnext)
            {
                ph_dehydrate(pdt);
                switch (dt->dt)
                {   case DT_abytes:
                    case DT_nbytes:
                        ph_dehydrate(&dt->DTpbytes);
                        break;
                    case DT_xoff:
                        symbol_dehydrate(&dt->DTsym);
                        break;
                }
            }
        }
        if (s->Scover)
            symbol_dehydrate(&s->Scover);
    }
}
#endif

/***************************
 * Dehydrate threaded list of symbols.
 */

#if DEHYDRATE
void symbol_symdefs_dehydrate(symbol **ps)
{
    symbol *s;

    for (; *ps; ps = &s->Snext)
    {
        s = *ps;
        symbol_debug(s);
        //dbg_printf("symbol_symdefs_dehydrate(%p, '%s')\n",s,s->Sident);
        symbol_dehydrate(ps);
    }
}
#endif

/************************************
 * Hydrate/dehydrate an mptr_t.
 */

#if HYDRATE
STATIC void mptr_hydrate(mptr_t **pm)
{   mptr_t *m;

    m = (mptr_t *) ph_hydrate(pm);
    symbol_hydrate(&m->MPf);
    symbol_hydrate(&m->MPparent);
}
#endif

#if DEHYDRATE
STATIC void mptr_dehydrate(mptr_t **pm)
{   mptr_t *m;

    m = *pm;
    if (m && !isdehydrated(m))
    {
        ph_dehydrate(pm);
#if DEBUG_XSYMGEN
        if (xsym_gen && ph_in_head(m->MPf))
            ph_dehydrate(&m->MPf);
        else
#endif
        symbol_dehydrate(&m->MPf);
        symbol_dehydrate(&m->MPparent);
    }
}
#endif

/************************************
 * Hydrate/dehydrate a baseclass_t.
 */

#if HYDRATE
STATIC void baseclass_hydrate(baseclass_t **pb)
{   baseclass_t *b;

    assert(pb);
    while (isdehydrated(*pb))
    {
        b = (baseclass_t *) ph_hydrate(pb);

        ph_hydrate(&b->BCbase);
        ph_hydrate(&b->BCpbase);
        list_hydrate(&b->BCpublics,(list_free_fp)symbol_hydrate);
        list_hydrate(&b->BCmptrlist,(list_free_fp)mptr_hydrate);
        symbol_hydrate(&b->BCvtbl);
        Classsym_hydrate(&b->BCparent);

        pb = &b->BCnext;
    }
}
#endif

/**********************************
 * Dehydrate a baseclass_t.
 */

#if DEHYDRATE
STATIC void baseclass_dehydrate(baseclass_t **pb)
{   baseclass_t *b;

    while ((b = *pb) != NULL && !isdehydrated(b))
    {
        ph_dehydrate(pb);

#if DEBUG_XSYMGEN
        if (xsym_gen && ph_in_head(b))
            return;
#endif

        ph_dehydrate(&b->BCbase);
        ph_dehydrate(&b->BCpbase);
        list_dehydrate(&b->BCpublics,(list_free_fp)symbol_dehydrate);
        list_dehydrate(&b->BCmptrlist,(list_free_fp)mptr_dehydrate);
        symbol_dehydrate(&b->BCvtbl);
        Classsym_dehydrate(&b->BCparent);

        pb = &b->BCnext;
    }
}
#endif

/***************************
 * Look down baseclass list to find sbase.
 * Returns:
 *      NULL    not found
 *      pointer to baseclass
 */

baseclass_t *baseclass_find(baseclass_t *bm,Classsym *sbase)
{
    symbol_debug(sbase);
    for (; bm; bm = bm->BCnext)
        if (bm->BCbase == sbase)
            break;
    return bm;
}

baseclass_t *baseclass_find_nest(baseclass_t *bm,Classsym *sbase)
{
    symbol_debug(sbase);
    for (; bm; bm = bm->BCnext)
    {
        if (bm->BCbase == sbase ||
            baseclass_find_nest(bm->BCbase->Sstruct->Sbase, sbase))
            break;
    }
    return bm;
}

/******************************
 * Calculate number of baseclasses in list.
 */

int baseclass_nitems(baseclass_t *b)
{   int i;

    for (i = 0; b; b = b->BCnext)
        i++;
    return i;
}

/*************************************
 * Reset Symbol so that it's now an "extern" to the next obj file being created.
 */
void symbol_reset(Symbol *s)
{
    s->Soffset = 0;
    s->Sxtrnnum = 0;
    s->Stypidx = 0;
    s->Sflags &= ~(STRoutdef | SFLweak);
    s->Sdw_ref_idx = 0;
    if (s->Sclass == SCglobal || s->Sclass == SCcomdat ||
        s->Sfl == FLudata || s->Sclass == SCstatic)
    {   s->Sclass = SCextern;
        s->Sfl = FLextern;
    }
}
