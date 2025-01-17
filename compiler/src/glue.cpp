
/* Compiler implementation of the D programming language
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/D-Programming-Language/dmd/blob/master/src/glue.c
 */

#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include <assert.h>

#include "mars.hpp"
#include "module.hpp"
#include "mtype.hpp"
#include "declaration.hpp"
#include "statement.hpp"
#include "enum.hpp"
#include "aggregate.hpp"
#include "init.hpp"
#include "attrib.hpp"
#include "id.hpp"
#include "import.hpp"
#include "template.hpp"
#include "lib.hpp"
#include "target.hpp"

#include "root/rmem.hpp"
#include "cc.hpp"
#include "global.hpp"
#include "oper.hpp"
#include "code.hpp"
#include "type.hpp"
#include "dt.hpp"
#include "outbuf.hpp"
#include "irstate.hpp"

void clearStringTab();
RET retStyle(TypeFunction *tf, bool needsThis);

elem *addressElem(elem *e, Type *t, bool alwaysCopy = false);
void Statement_toIR(Statement *s, IRState *irs);
void insertFinallyBlockCalls(block *startblock);
elem *toEfilename(Module *m);
Symbol *toSymbol(Dsymbol *s);
void buildClosure(FuncDeclaration *fd, IRState *irs);
Symbol *toStringSymbol(const char *str, size_t len, size_t pad);

typedef Array<symbol *> symbols;
Dsymbols *Dsymbols_create();
Expressions *Expressions_create();
type *Type_toCtype(Type *t);
void toObjFile(Dsymbol *ds, bool multiobj);
void genModuleInfo(Module *m);
void genObjFile(Module *m, bool multiobj);
Symbol *toSymbolX(Dsymbol *ds, const char *prefix, int sclass, type *t, const char *suffix);
int blockExit(Statement *s, FuncDeclaration *func, bool mustNotThrow);

elem *eictor;
symbol *ictorlocalgot;
symbols sctors;
StaticDtorDeclarations ectorgates;
symbols sdtors;
symbols stests;

symbols ssharedctors;
SharedStaticDtorDeclarations esharedctorgates;
symbols sshareddtors;

char *lastmname;

bool onlyOneMain(Loc loc);

/**************************************
 * Append s to list of object files to generate later.
 */

Dsymbols obj_symbols_towrite;

void obj_append(Dsymbol *s)
{
    //printf("deferred: %s\n", s->toChars());
    obj_symbols_towrite.push(s);
}

void obj_write_deferred(Library *library)
{
    for (size_t i = 0; i < obj_symbols_towrite.length; i++)
    {
        Dsymbol *s = obj_symbols_towrite[i];
        Module *m = s->getModule();

        char *mname;
        if (m)
        {
            mname = (char*)m->srcfile->toChars();
            lastmname = mname;
        }
        else
        {
            //mname = s->ident->toChars();
            mname = lastmname;
            assert(mname);
        }

        obj_start(mname);

        static int count;
        count++;                // sequence for generating names

        /* Create a module that's a doppelganger of m, with just
         * enough to be able to create the moduleinfo.
         */
        OutBuffer idbuf;
        idbuf.printf("%s.%d", m ? m->ident->toChars() : mname, count);

        if (!m)
        {
            // it doesn't make sense to make up a module if we don't know where to put the symbol
            //  so output it into it's own object file without ModuleInfo
            objmod->initfile(idbuf.peekChars(), nullptr, mname);
            toObjFile(s, false);
            objmod->termfile();
        }
        else
        {
            Identifier *id = Identifier::create(idbuf.extractChars());

            Module *md = Module::create(mname, id, 0, 0);
            md->members = Dsymbols_create();
            md->members->push(s);   // its only 'member' is s
            md->doppelganger = 1;       // identify this module as doppelganger
            md->md = m->md;
            md->aimports.push(m);       // it only 'imports' m

            genObjFile(md, false);
        }

        /* Set object file name to be source name with sequence number,
         * as mangled symbol names get way too long.
         */
        const char *fname = FileName::removeExt(mname);
        OutBuffer namebuf;
        unsigned hash = 0;
        for (const char *p = s->toChars(); *p; p++)
            hash += *p;
        namebuf.printf("%s_%x_%x.%s", fname, count, hash, global.obj_ext.ptr);
        FileName::free((char *)fname);
        fname = namebuf.extractChars();

        //printf("writing '%s'\n", fname);
        File *objfile = File::create(fname);
        obj_end(library, objfile);
    }
    obj_symbols_towrite.length = 0;
}

/***********************************************
 * Generate function that calls array of functions and gates.
 */

symbol *callFuncsAndGates(Module *m, symbols *sctors, StaticDtorDeclarations *ectorgates,
        const char *id)
{
    symbol *sctor = nullptr;

    if ((sctors && sctors->length) ||
        (ectorgates && ectorgates->length))
    {
        static type *t;
        if (!t)
        {
            /* t will be the type of the functions generated:
             *      extern (C) void func();
             */
            t = type_function(TYnfunc, nullptr, 0, false, tsvoid);
            t->Tmangle = mTYman_c;
        }

        localgot = nullptr;
        sctor = toSymbolX(m, id, SCglobal, t, "FZv");
        cstate.CSpsymtab = &sctor->Sfunc->Flocsym;
        elem *ector = nullptr;

        if (ectorgates)
        {
            for (size_t i = 0; i < ectorgates->length; i++)
            {   StaticDtorDeclaration *f = (*ectorgates)[i];

                Symbol *s = toSymbol(f->vgate);
                elem *e = el_var(s);
                e = el_bin(OPaddass, TYint, e, el_long(TYint, 1));
                ector = el_combine(ector, e);
            }
        }

        if (sctors)
        {
            for (size_t i = 0; i < sctors->length; i++)
            {   symbol *s = (*sctors)[i];
                elem *e = el_una(OPucall, TYvoid, el_var(s));
                ector = el_combine(ector, e);
            }
        }

        block *b = block_calloc();
        b->BC = BCret;
        b->Belem = ector;
        sctor->Sfunc->Fstartline.Sfilename = m->arg;
        sctor->Sfunc->Fstartblock = b;
        writefunc(sctor);
    }
    return sctor;
}

/**************************************
 * Prepare for generating obj file.
 */

Outbuffer objbuf;

void obj_start(char *srcfile)
{
    //printf("obj_start()\n");

    rtlsym_reset();
    clearStringTab();

    objmod = Obj::init(&objbuf, srcfile, nullptr);

    el_reset();
    cg87_reset();
    out_reset();
}

void obj_end(Library *library, File *objfile)
{
    const char *objfilename = objfile->name->toChars();
    objmod->term(objfilename);
    delete objmod;
    objmod = nullptr;

    if (library)
    {
        // Transfer image to library
        library->addObject(objfilename, objbuf.buf, objbuf.p - objbuf.buf);
        objbuf.buf = nullptr;
    }
    else
    {
        // Transfer image to file
        objfile->setbuffer(objbuf.buf, objbuf.p - objbuf.buf);
        objbuf.buf = nullptr;

        ensurePathToNameExists(Loc(), objfilename);

        //printf("write obj %s\n", objfilename);
        writeFile(Loc(), objfile);
    }
    objbuf.pend = nullptr;
    objbuf.p = nullptr;
    objbuf.len = 0;
    objbuf.inc = 0;
}

bool obj_includelib(const char *name)
{
    return objmod->includelib(name);
}

void obj_startaddress(Symbol *s)
{
    return objmod->startaddress(s);
}


/**************************************
 * Generate .obj file for Module.
 */

void genObjFile(Module *m, bool multiobj)
{
    //EEcontext *ee = env->getEEcontext();

    //printf("Module::genobjfile(multiobj = %d) %s\n", multiobj, m->toChars());

    if (m->ident == Id::entrypoint)
    {
        bool v = global.params.verbose;
        global.params.verbose = false;

        for (size_t i = 0; i < m->members->length; i++)
        {
            Dsymbol *member = (*m->members)[i];
            //printf("toObjFile %s %s\n", member->kind(), member->toChars());
            toObjFile(member, global.params.multiobj);
        }

        global.params.verbose = v;
        return;
    }

    lastmname = (char*)m->srcfile->toChars();

    objmod->initfile(lastmname, nullptr, m->toPrettyChars());

    eictor = nullptr;
    ictorlocalgot = nullptr;
    sctors.setDim(0);
    ectorgates.setDim(0);
    sdtors.setDim(0);
    ssharedctors.setDim(0);
    esharedctorgates.setDim(0);
    sshareddtors.setDim(0);
    stests.setDim(0);

    if (m->doppelganger)
    {
        /* Generate a reference to the moduleinfo, so the module constructors
         * and destructors get linked in.
         */
        Module *mod = m->aimports[0];
        assert(mod);
        if (mod->sictor || mod->sctor || mod->sdtor || mod->ssharedctor || mod->sshareddtor)
        {
            Symbol *s = toSymbol(mod);
            //objextern(s);
            //if (!s->Sxtrnnum) objextdef(s->Sident);
            if (!s->Sxtrnnum)
            {
                //printf("%s\n", s->Sident);
                Symbol *sref = symbol_generate(SCstatic, type_fake(TYnptr));
                sref->Sfl = FLdata;
                DtBuilder dtb;
                dtb.xoff(s, 0, TYnptr);
                sref->Sdt = dtb.finish();
                outdata(sref);
            }
        }
    }

    if (global.params.cov)
    {
        /* Create coverage identifier:
         *  private uint[numlines] __coverage;
         */
        m->cov = symbol_calloc("__coverage");
        m->cov->Stype = type_fake(TYint);
        m->cov->Stype->Tmangle = mTYman_c;
        m->cov->Stype->Tcount++;
        m->cov->Sclass = SCstatic;
        m->cov->Sfl = FLdata;

        DtBuilder dtb;
        dtb.nzeros(4 * m->numlines);
        m->cov->Sdt = dtb.finish();

        outdata(m->cov);

        m->covb = (unsigned *)calloc((m->numlines + 32) / 32, sizeof(*m->covb));
    }

    for (size_t i = 0; i < m->members->length; i++)
    {
        Dsymbol *member = (*m->members)[i];
        //printf("toObjFile %s %s\n", member->kind(), member->toChars());
        toObjFile(member, multiobj);
    }

    if (global.params.cov)
    {
        /* Generate
         *  private bit[numlines] __bcoverage;
         */
        Symbol *bcov = symbol_calloc("__bcoverage");
        bcov->Stype = type_fake(TYuint);
        bcov->Stype->Tcount++;
        bcov->Sclass = SCstatic;
        bcov->Sfl = FLdata;

        DtBuilder dtb;
        dtb.nbytes((m->numlines + 32) / 32 * sizeof(*m->covb), (char *)m->covb);
        bcov->Sdt = dtb.finish();

        outdata(bcov);

        free(m->covb);
        m->covb = nullptr;

        /* Generate:
         *  _d_cover_register(uint[] __coverage, BitArray __bcoverage, string filename);
         * and prepend it to the static constructor.
         */

        /* t will be the type of the functions generated:
         *      extern (C) void func();
         */
        type *t = type_function(TYnfunc, nullptr, 0, false, tsvoid);
        t->Tmangle = mTYman_c;

        m->sictor = toSymbolX(m, "__modictor", SCglobal, t, "FZv");
        cstate.CSpsymtab = &m->sictor->Sfunc->Flocsym;
        localgot = ictorlocalgot;

        elem *ecov  = el_pair(TYdarray, el_long(TYsize_t, m->numlines), el_ptr(m->cov));
        elem *ebcov = el_pair(TYdarray, el_long(TYsize_t, m->numlines), el_ptr(bcov));

        elem *efilename = toEfilename(m);
        elem *e = el_params(
                      el_long(TYuchar, global.params.covPercent),
                      ecov,
                      ebcov,
                      efilename,
                      nullptr);
        e = el_bin(OPcall, TYvoid, el_var(getRtlsym(RTLSYM_DCOVER2)), e);
        eictor = el_combine(e, eictor);
        ictorlocalgot = localgot;
    }

    // If coverage / static constructor / destructor / unittest calls
    if (eictor || sctors.length || ectorgates.length || sdtors.length ||
        ssharedctors.length || esharedctorgates.length || sshareddtors.length || stests.length)
    {
        if (eictor)
        {
            localgot = ictorlocalgot;

            block *b = block_calloc();
            b->BC = BCret;
            b->Belem = eictor;
            m->sictor->Sfunc->Fstartline.Sfilename = m->arg;
            m->sictor->Sfunc->Fstartblock = b;
            writefunc(m->sictor);
        }

        m->sctor = callFuncsAndGates(m, &sctors, &ectorgates, "__modctor");
        m->sdtor = callFuncsAndGates(m, &sdtors, nullptr, "__moddtor");

        m->ssharedctor = callFuncsAndGates(m, &ssharedctors, (StaticDtorDeclarations *)&esharedctorgates, "__modsharedctor");
        m->sshareddtor = callFuncsAndGates(m, &sshareddtors, nullptr, "__modshareddtor");
        m->stest = callFuncsAndGates(m, &stests, nullptr, "__modtest");

        if (m->doppelganger)
            genModuleInfo(m);
    }

    if (m->doppelganger)
    {
        objmod->termfile();
        return;
    }

    /* Generate module info for templates and -cov.
     *  Don't generate ModuleInfo if `object.ModuleInfo` is not declared or
     *  explicitly disabled through compiler switches such as `-betterC`.
     */
    if (global.params.useModuleInfo && Module::moduleinfo /*|| needModuleInfo()*/)
        genModuleInfo(m);

    objmod->termfile();
}

/**************************************
 * Search for a druntime array op
 */
bool isDruntimeArrayOp(Identifier *ident)
{
    /* Some of the array op functions are written as library functions,
     * presumably to optimize them with special CPU vector instructions.
     * List those library functions here, in alpha order.
     */
    static const char *libArrayopFuncs[] =
    {
        "_arrayExpSliceAddass_a",
        "_arrayExpSliceAddass_d",
        "_arrayExpSliceAddass_f",           // T[]+=T
        "_arrayExpSliceAddass_g",
        "_arrayExpSliceAddass_h",
        "_arrayExpSliceAddass_i",
        "_arrayExpSliceAddass_k",
        "_arrayExpSliceAddass_s",
        "_arrayExpSliceAddass_t",
        "_arrayExpSliceAddass_u",
        "_arrayExpSliceAddass_w",

        "_arrayExpSliceDivass_d",           // T[]/=T
        "_arrayExpSliceDivass_f",           // T[]/=T

        "_arrayExpSliceMinSliceAssign_a",
        "_arrayExpSliceMinSliceAssign_d",   // T[]=T-T[]
        "_arrayExpSliceMinSliceAssign_f",   // T[]=T-T[]
        "_arrayExpSliceMinSliceAssign_g",
        "_arrayExpSliceMinSliceAssign_h",
        "_arrayExpSliceMinSliceAssign_i",
        "_arrayExpSliceMinSliceAssign_k",
        "_arrayExpSliceMinSliceAssign_s",
        "_arrayExpSliceMinSliceAssign_t",
        "_arrayExpSliceMinSliceAssign_u",
        "_arrayExpSliceMinSliceAssign_w",

        "_arrayExpSliceMinass_a",
        "_arrayExpSliceMinass_d",           // T[]-=T
        "_arrayExpSliceMinass_f",           // T[]-=T
        "_arrayExpSliceMinass_g",
        "_arrayExpSliceMinass_h",
        "_arrayExpSliceMinass_i",
        "_arrayExpSliceMinass_k",
        "_arrayExpSliceMinass_s",
        "_arrayExpSliceMinass_t",
        "_arrayExpSliceMinass_u",
        "_arrayExpSliceMinass_w",

        "_arrayExpSliceMulass_d",           // T[]*=T
        "_arrayExpSliceMulass_f",           // T[]*=T
        "_arrayExpSliceMulass_i",
        "_arrayExpSliceMulass_k",
        "_arrayExpSliceMulass_s",
        "_arrayExpSliceMulass_t",
        "_arrayExpSliceMulass_u",
        "_arrayExpSliceMulass_w",

        "_arraySliceExpAddSliceAssign_a",
        "_arraySliceExpAddSliceAssign_d",   // T[]=T[]+T
        "_arraySliceExpAddSliceAssign_f",   // T[]=T[]+T
        "_arraySliceExpAddSliceAssign_g",
        "_arraySliceExpAddSliceAssign_h",
        "_arraySliceExpAddSliceAssign_i",
        "_arraySliceExpAddSliceAssign_k",
        "_arraySliceExpAddSliceAssign_s",
        "_arraySliceExpAddSliceAssign_t",
        "_arraySliceExpAddSliceAssign_u",
        "_arraySliceExpAddSliceAssign_w",

        "_arraySliceExpDivSliceAssign_d",   // T[]=T[]/T
        "_arraySliceExpDivSliceAssign_f",   // T[]=T[]/T

        "_arraySliceExpMinSliceAssign_a",
        "_arraySliceExpMinSliceAssign_d",   // T[]=T[]-T
        "_arraySliceExpMinSliceAssign_f",   // T[]=T[]-T
        "_arraySliceExpMinSliceAssign_g",
        "_arraySliceExpMinSliceAssign_h",
        "_arraySliceExpMinSliceAssign_i",
        "_arraySliceExpMinSliceAssign_k",
        "_arraySliceExpMinSliceAssign_s",
        "_arraySliceExpMinSliceAssign_t",
        "_arraySliceExpMinSliceAssign_u",
        "_arraySliceExpMinSliceAssign_w",

        "_arraySliceExpMulSliceAddass_d",   // T[] += T[]*T
        "_arraySliceExpMulSliceAddass_f",
        "_arraySliceExpMulSliceAddass_r",

        "_arraySliceExpMulSliceAssign_d",   // T[]=T[]*T
        "_arraySliceExpMulSliceAssign_f",   // T[]=T[]*T
        "_arraySliceExpMulSliceAssign_i",
        "_arraySliceExpMulSliceAssign_k",
        "_arraySliceExpMulSliceAssign_s",
        "_arraySliceExpMulSliceAssign_t",
        "_arraySliceExpMulSliceAssign_u",
        "_arraySliceExpMulSliceAssign_w",

        "_arraySliceExpMulSliceMinass_d",   // T[] -= T[]*T
        "_arraySliceExpMulSliceMinass_f",
        "_arraySliceExpMulSliceMinass_r",

        "_arraySliceSliceAddSliceAssign_a",
        "_arraySliceSliceAddSliceAssign_d", // T[]=T[]+T[]
        "_arraySliceSliceAddSliceAssign_f", // T[]=T[]+T[]
        "_arraySliceSliceAddSliceAssign_g",
        "_arraySliceSliceAddSliceAssign_h",
        "_arraySliceSliceAddSliceAssign_i",
        "_arraySliceSliceAddSliceAssign_k",
        "_arraySliceSliceAddSliceAssign_r", // T[]=T[]+T[]
        "_arraySliceSliceAddSliceAssign_s",
        "_arraySliceSliceAddSliceAssign_t",
        "_arraySliceSliceAddSliceAssign_u",
        "_arraySliceSliceAddSliceAssign_w",

        "_arraySliceSliceAddass_a",
        "_arraySliceSliceAddass_d",         // T[]+=T[]
        "_arraySliceSliceAddass_f",         // T[]+=T[]
        "_arraySliceSliceAddass_g",
        "_arraySliceSliceAddass_h",
        "_arraySliceSliceAddass_i",
        "_arraySliceSliceAddass_k",
        "_arraySliceSliceAddass_s",
        "_arraySliceSliceAddass_t",
        "_arraySliceSliceAddass_u",
        "_arraySliceSliceAddass_w",

        "_arraySliceSliceMinSliceAssign_a",
        "_arraySliceSliceMinSliceAssign_d", // T[]=T[]-T[]
        "_arraySliceSliceMinSliceAssign_f", // T[]=T[]-T[]
        "_arraySliceSliceMinSliceAssign_g",
        "_arraySliceSliceMinSliceAssign_h",
        "_arraySliceSliceMinSliceAssign_i",
        "_arraySliceSliceMinSliceAssign_k",
        "_arraySliceSliceMinSliceAssign_r", // T[]=T[]-T[]
        "_arraySliceSliceMinSliceAssign_s",
        "_arraySliceSliceMinSliceAssign_t",
        "_arraySliceSliceMinSliceAssign_u",
        "_arraySliceSliceMinSliceAssign_w",

        "_arraySliceSliceMinass_a",
        "_arraySliceSliceMinass_d",         // T[]-=T[]
        "_arraySliceSliceMinass_f",         // T[]-=T[]
        "_arraySliceSliceMinass_g",
        "_arraySliceSliceMinass_h",
        "_arraySliceSliceMinass_i",
        "_arraySliceSliceMinass_k",
        "_arraySliceSliceMinass_s",
        "_arraySliceSliceMinass_t",
        "_arraySliceSliceMinass_u",
        "_arraySliceSliceMinass_w",

        "_arraySliceSliceMulSliceAssign_d", // T[]=T[]*T[]
        "_arraySliceSliceMulSliceAssign_f", // T[]=T[]*T[]
        "_arraySliceSliceMulSliceAssign_i",
        "_arraySliceSliceMulSliceAssign_k",
        "_arraySliceSliceMulSliceAssign_s",
        "_arraySliceSliceMulSliceAssign_t",
        "_arraySliceSliceMulSliceAssign_u",
        "_arraySliceSliceMulSliceAssign_w",

        "_arraySliceSliceMulass_d",         // T[]*=T[]
        "_arraySliceSliceMulass_f",         // T[]*=T[]
        "_arraySliceSliceMulass_i",
        "_arraySliceSliceMulass_k",
        "_arraySliceSliceMulass_s",
        "_arraySliceSliceMulass_t",
        "_arraySliceSliceMulass_u",
        "_arraySliceSliceMulass_w",
    };
    const char *name = ident->toChars();
    int i = binary(name, libArrayopFuncs, sizeof(libArrayopFuncs) / sizeof(char *));
    if (i != -1)
        return true;

#ifdef DEBUG    // Make sure our array is alphabetized
    for (i = 0; i < sizeof(libArrayopFuncs) / sizeof(char *); i++)
    {
        if (strcmp(name, libArrayopFuncs[i]) == 0)
            assert(0);
    }
#endif
    return false;
}

/* ================================================================== */

UnitTestDeclaration *needsDeferredNested(FuncDeclaration *fd)
{
    while (fd && fd->isNested())
    {
        FuncDeclaration *fdp = fd->toParent2()->isFuncDeclaration();
        if (!fdp)
            break;
        if (UnitTestDeclaration *udp = fdp->isUnitTestDeclaration())
            return udp->semanticRun < PASSobj ? udp : nullptr;
        fd = fdp;
    }
    return nullptr;
}

void FuncDeclaration_toObjFile(FuncDeclaration *fd, bool multiobj)
{
    ClassDeclaration *cd = fd->parent->isClassDeclaration();

    if (fd->semanticRun >= PASSobj) // if toObjFile() already run
        return;

    if (fd->type && fd->type->ty == Tfunction && ((TypeFunction *)fd->type)->next == nullptr)
        return;

    // If errors occurred compiling it, such as bugzilla 6118
    if (fd->type && fd->type->ty == Tfunction && ((TypeFunction *)fd->type)->next->ty == Terror)
        return;

    if (fd->semantic3Errors)
        return;

    if (global.errors)
        return;

    if (!fd->fbody)
        return;

    UnitTestDeclaration *ud = fd->isUnitTestDeclaration();
    if (ud && !global.params.useUnitTests)
        return;

    if (multiobj && !fd->isStaticDtorDeclaration() && !fd->isStaticCtorDeclaration())
    {
        obj_append(fd);
        return;
    }

    if (fd->semanticRun == PASSsemanticdone)
    {
        /* What happened is this function failed semantic3() with errors,
         * but the errors were gagged.
         * Try to reproduce those errors, and then fail.
         */
        fd->error("errors compiling the function");
        return;
    }
    assert(fd->semanticRun == PASSsemantic3done);
    assert(fd->ident != Id::empty);

    for (FuncDeclaration *fd2 = fd; fd2; )
    {
        if (fd2->inNonRoot())
            return;
        if (fd2->isNested())
            fd2 = fd2->toParent2()->isFuncDeclaration();
        else
            break;
    }

    if (UnitTestDeclaration *udp = needsDeferredNested(fd))
    {
        /* Can't do unittest's out of order, they are order dependent in that their
         * execution is done in lexical order.
         */
        udp->deferredNested.push(fd);
        //printf("%s @[%s]\n\t--> pushed to unittest @[%s]\n",
        //    fd->toPrettyChars(), fd->loc.toChars(), udp->loc.toChars());
        return;
    }

    if (fd->isArrayOp && isDruntimeArrayOp(fd->ident))
    {
        // Implementation is in druntime
        return;
    }

    // start code generation
    fd->semanticRun = PASSobj;

    if (global.params.verbose)
        fprintf(global.stdmsg, "function  %s\n", fd->toPrettyChars());

    Symbol *s = toSymbol(fd);
    func_t *f = s->Sfunc;

    // tunnel type of "this" to debug info generation
    if (AggregateDeclaration* ad = fd->parent->isAggregateDeclaration())
    {
        ::type* t = Type_toCtype(ad->getType());
        if (cd)
            t = t->Tnext; // skip reference
        f->Fclass = (Classsym *)t;
    }

    /* This is done so that the 'this' pointer on the stack is the same
     * distance away from the function parameters, so that an overriding
     * function can call the nested fdensure or fdrequire of its overridden function
     * and the stack offsets are the same.
     */
    if (fd->isVirtual() && (fd->fensure || fd->frequire))
        f->Fflags3 |= Ffakeeh;

    s->Sclass = SCglobal;

    for (Dsymbol *p = fd->parent; p; p = p->parent)
    {
        if (p->isTemplateInstance())
        {
            s->Sclass = SCcomdat;
            break;
        }
    }

    /* Vector operations should be comdat's
     */
    if (fd->isArrayOp)
        s->Sclass = SCcomdat;

    if (fd->isNested())
    {
        //if (!(config.flags3 & CFG3pic))
        //    s->Sclass = SCstatic;
        f->Fflags3 |= Fnested;

        /* The enclosing function must have its code generated first,
         * in order to calculate correct frame pointer offset.
         */
        FuncDeclaration *fdp = fd->toParent2()->isFuncDeclaration();
        if (fdp && fdp->semanticRun < PASSobj)
        {
            toObjFile(fdp, multiobj);
        }
    }
    else
    {
        const char *libname = (global.params.symdebug)
                                ? global.params.debuglibname.ptr
                                : global.params.defaultlibname.ptr;

        // Pull in RTL startup code (but only once)
        if (fd->isMain() && onlyOneMain(fd->loc))
        {
            objmod->external_def("_main");
            objmod->ehsections();   // initialize exception handling sections

            objmod->includelib(libname);
            s->Sclass = SCglobal;
        }
        else if (strcmp(s->Sident, "main") == 0 && fd->linkage == LINKc)
        {
            s->Sclass = SCglobal;
        }
        else if (fd->ident == Id::tls_get_addr && fd->linkage == LINKd)
        {
            // TODO: Change linkage in druntime to extern(C).
            f->Fredirect = (char *)Id::tls_get_addr->toChars();
        }
    }

    symtab_t *symtabsave = cstate.CSpsymtab;
    cstate.CSpsymtab = &f->Flocsym;

    // Find module m for this function
    Module *m = nullptr;
    for (Dsymbol *p = fd->parent; p; p = p->parent)
    {
        m = p->isModule();
        if (m)
            break;
    }

    IRState irs(m, fd);
    Dsymbols deferToObj;                   // write these to OBJ file later
    irs.deferToObj = &deferToObj;
    void *labels = nullptr;
    irs.labels = &labels;

    symbol *shidden = nullptr;
    Symbol *sthis = nullptr;
    tym_t tyf = tybasic(s->Stype->Tty);
    //printf("linkage = %d, tyf = x%x\n", linkage, tyf);
    int reverse = tyrevfunc(s->Stype->Tty);

    assert(fd->type->ty == Tfunction);
    TypeFunction *tf = (TypeFunction *)fd->type;
    RET retmethod = retStyle(tf, fd->needThis());
    if (retmethod == RETstack)
    {
        // If function returns a struct, put a pointer to that
        // as the first argument
        ::type *thidden = Type_toCtype(tf->next->pointerTo());
        char hiddenparam[5+4+1];
        static int hiddenparami;    // how many we've generated so far

        sprintf(hiddenparam,"__HID%d",++hiddenparami);
        shidden = symbol_name(hiddenparam,SCparameter,thidden);
        shidden->Sflags |= SFLtrue | SFLfree;
        if (fd->nrvo_can && fd->nrvo_var && fd->nrvo_var->nestedrefs.length)
            type_setcv(&shidden->Stype, shidden->Stype->Tty | mTYvolatile);
        irs.shidden = shidden;
        fd->shidden = shidden;
    }
    else
    {
        // Register return style cannot make nrvo.
        // Auto functions keep the nrvo_can flag up to here,
        // so we should eliminate it before entering backend.
        fd->nrvo_can = 0;
    }

    if (fd->vthis)
    {
        assert(!fd->vthis->csym);
        sthis = toSymbol(fd->vthis);
        irs.sthis = sthis;
        if (!(f->Fflags3 & Fnested))
            f->Fflags3 |= Fmember;
    }

    // Estimate number of parameters, pi
    size_t pi = (fd->v_arguments != nullptr);
    if (fd->parameters)
        pi += fd->parameters->length;

    // Create a temporary buffer, params[], to hold function parameters
    Symbol *paramsbuf[10];
    Symbol **params = paramsbuf;    // allocate on stack if possible
    if (pi + 2 > 10)                // allow extra 2 for sthis and shidden
    {
        params = (Symbol **)malloc((pi + 2) * sizeof(Symbol *));
        assert(params);
    }

    // Get the actual number of parameters, pi, and fill in the params[]
    pi = 0;
    if (fd->v_arguments)
    {
        params[pi] = toSymbol(fd->v_arguments);
        pi += 1;
    }
    if (fd->parameters)
    {
        for (size_t i = 0; i < fd->parameters->length; i++)
        {
            VarDeclaration *v = (*fd->parameters)[i];
            //printf("param[%d] = %p, %s\n", i, v, v->toChars());
            assert(!v->csym);
            params[pi + i] = toSymbol(v);
        }
        pi += fd->parameters->length;
    }

    if (reverse)
    {
        // Reverse params[] entries
        for (size_t i = 0; i < pi/2; i++)
        {
            Symbol *sptmp = params[i];
            params[i] = params[pi - 1 - i];
            params[pi - 1 - i] = sptmp;
        }
    }

    if (shidden)
    {
        // shidden becomes first parameter
        memmove(params + 1, params, pi * sizeof(params[0]));
        params[0] = shidden;
        pi++;
    }


    if (sthis)
    {
        // sthis becomes first parameter
        memmove(params + 1, params, pi * sizeof(params[0]));
        params[0] = sthis;
        pi++;
    }

    if ((global.params.isLinux) &&
         fd->linkage != LINKd && shidden && sthis)
    {
        /* swap shidden and sthis
         */
        Symbol *sp = params[0];
        params[0] = params[1];
        params[1] = sp;
    }

    for (size_t i = 0; i < pi; i++)
    {
        Symbol *sp = params[i];
        sp->Sclass = SCparameter;
        sp->Sflags &= ~SFLspill;
        sp->Sfl = FLpara;
        symbol_add(sp);
    }

    // Determine register assignments
    if (pi)
    {
        FuncParamRegs fpr(tyf);

        for (size_t i = 0; i < pi; i++)
        {
            Symbol *sp = params[i];
            if (fpr.alloc(sp->Stype, sp->Stype->Tty, &sp->Spreg, &sp->Spreg2))
            {
                sp->Sclass = SCfastpar;
                sp->Sfl = (sp->Sclass == SCshadowreg) ? FLpara : FLfast;
            }
        }
    }

    // Done with params
    if (params != paramsbuf)
        free(params);
    params = nullptr;

    if (fd->fbody)
    {
        localgot = nullptr;

        Statement *sbody = fd->fbody;

        Blockx bx;
        memset(&bx,0,sizeof(bx));
        bx.startblock = block_calloc();
        bx.curblock = bx.startblock;
        bx.funcsym = s;
        bx.scope_index = -1;
        bx.classdec = cd;
        bx.member = fd;
        bx.module = fd->getModule();
        irs.blx = &bx;

        // Initialize argptr
        if (fd->v_argptr)
        {
            // Declare va_argsave
            if (global.params.is64bit)
            {
                type *t = type_struct_class("__va_argsave_t", 16, 8 * 6 + 8 * 16 + 8 * 3, nullptr, nullptr, false, false, true);
                // The backend will pick this up by name
                Symbol *s = symbol_name("__va_argsave", SCauto, t);
                s->Stype->Tty |= mTYvolatile;
                symbol_add(s);
            }

            Symbol *s = toSymbol(fd->v_argptr);
            symbol_add(s);
            elem *e = el_una(OPva_start, TYnptr, el_ptr(s));
            block_appendexp(irs.blx->curblock, e);
        }

        /* Doing this in semantic3() caused all kinds of problems:
         * 1. couldn't reliably get the final mangling of the function name due to fwd refs
         * 2. impact on function inlining
         * 3. what to do when writing out .di files, or other pretty printing
         */
        if (global.params.trace && !fd->isCMain())
        {
            /* The profiler requires TLS, and TLS may not be set up yet when C main()
             * gets control (i.e. OSX), leading to a crash.
             */
            /* Wrap the entire function body in:
             *   trace_pro("funcname");
             *   try
             *     body;
             *   finally
             *     _c_trace_epi();
             */
            StringExp *se = StringExp::create(Loc(), s->Sident);
            se->type = Type::tstring;
            se->type = typeSemantic(se->type, Loc(), nullptr);
            Expressions *exps = Expressions_create();
            exps->push(se);
            FuncDeclaration *fdpro = FuncDeclaration::genCfunc(nullptr, Type::tvoid, "trace_pro");
            Expression *ec = VarExp::create(Loc(), fdpro);
            Expression *e = CallExp::create(Loc(), ec, exps);
            e->type = Type::tvoid;
            Statement *sp = ExpStatement::create(fd->loc, e);

            FuncDeclaration *fdepi = FuncDeclaration::genCfunc(nullptr, Type::tvoid, "_c_trace_epi");
            ec = VarExp::create(Loc(), fdepi);
            e = CallExp::create(Loc(), ec);
            e->type = Type::tvoid;
            Statement *sf = ExpStatement::create(fd->loc, e);

            Statement *stf;
            if (blockExit(sbody, fd, false) == BEfallthru)
                stf = CompoundStatement::create(Loc(), sbody, sf);
            else
                stf = TryFinallyStatement::create(Loc(), sbody, sf);
            sbody = CompoundStatement::create(Loc(), sp, stf);
        }

        if (fd->interfaceVirtual)
        {
            // Adjust the 'this' pointer instead of using a thunk
            assert(irs.sthis);
            elem *ethis = el_var(irs.sthis);
            elem *e = el_bin(OPminass, TYnptr, ethis, el_long(TYsize_t, fd->interfaceVirtual->offset));
            block_appendexp(irs.blx->curblock, e);
        }

        buildClosure(fd, &irs);

        Statement_toIR(sbody, &irs);
        bx.curblock->BC = BCret;

        f->Fstartblock = bx.startblock;
//      einit = el_combine(einit,bx.init);

        if (fd->isCtorDeclaration())
        {
            assert(sthis);
            for (block *b = f->Fstartblock; b; b = b->Bnext)
            {
                if (b->BC == BCret)
                {
                    b->BC = BCretexp;
                    b->Belem = el_combine(b->Belem, el_var(sthis));
                }
            }
        }
        insertFinallyBlockCalls(f->Fstartblock);
    }

    // If static constructor
    if (fd->isSharedStaticCtorDeclaration())        // must come first because it derives from StaticCtorDeclaration
    {
        ssharedctors.push(s);
    }
    else if (fd->isStaticCtorDeclaration())
    {
        sctors.push(s);
    }

    // If static destructor
    if (fd->isSharedStaticDtorDeclaration())        // must come first because it derives from StaticDtorDeclaration
    {
        SharedStaticDtorDeclaration *f = fd->isSharedStaticDtorDeclaration();
        assert(f);
        if (f->vgate)
        {
            /* Increment destructor's vgate at construction time
             */
            esharedctorgates.push(f);
        }

        sshareddtors.shift(s);
    }
    else if (fd->isStaticDtorDeclaration())
    {
        StaticDtorDeclaration *f = fd->isStaticDtorDeclaration();
        assert(f);
        if (f->vgate)
        {
            /* Increment destructor's vgate at construction time
             */
            ectorgates.push(f);
        }

        sdtors.shift(s);
    }

    // If unit test
    if (ud)
    {
        stests.push(s);
    }

    if (global.errors)
    {
        // Restore symbol table
        cstate.CSpsymtab = symtabsave;
        return;
    }

    writefunc(s);
    // Restore symbol table
    cstate.CSpsymtab = symtabsave;

    if (fd->isExport())
        objmod->export_symbol(s, Para.offset);

    for (size_t i = 0; i < irs.deferToObj->length; i++)
    {
        Dsymbol *s = (*irs.deferToObj)[i];
        toObjFile(s, false);
    }

    if (ud)
    {
        for (size_t i = 0; i < ud->deferredNested.length; i++)
        {
            FuncDeclaration *fd = ud->deferredNested[i];
            toObjFile(fd, false);
        }
    }

    // A hack to get a pointer to this function put in the .dtors segment
    if (fd->ident && memcmp(fd->ident->toChars(), "_STD", 4) == 0)
        objmod->staticdtor(s);

    if (irs.startaddress)
    {
        //printf("Setting start address\n");
        objmod->startaddress(irs.startaddress);
    }
}

bool onlyOneMain(Loc loc)
{
    static Loc lastLoc;
    static bool hasMain = false;
    if (hasMain)
    {
        const char *msg = "";
        if (global.params.addMain)
            msg = ", -main switch added another main()";
        const char *othermain = "";
        error(lastLoc, "only one main%s allowed%s", othermain, msg);
        return false;
    }
    lastLoc = loc;
    hasMain = true;
    return true;
}

/* ================================================================== */

/*****************************
 * Return back end type corresponding to D front end type.
 */

unsigned totym(Type *tx)
{
    unsigned t;
    switch (tx->ty)
    {
        case Tvoid:     t = TYvoid;     break;
        case Tint8:     t = TYschar;    break;
        case Tuns8:     t = TYuchar;    break;
        case Tint16:    t = TYshort;    break;
        case Tuns16:    t = TYushort;   break;
        case Tint32:    t = TYint;      break;
        case Tuns32:    t = TYuint;     break;
        case Tint64:    t = TYllong;    break;
        case Tuns64:    t = TYullong;   break;
        case Tfloat32:  t = TYfloat;    break;
        case Tfloat64:  t = TYdouble;   break;
        case Tfloat80:  t = TYldouble;  break;
        case Timaginary32: t = TYifloat; break;
        case Timaginary64: t = TYidouble; break;
        case Timaginary80: t = TYildouble; break;
        case Tcomplex32: t = TYcfloat;  break;
        case Tcomplex64: t = TYcdouble; break;
        case Tcomplex80: t = TYcldouble; break;
        case Tbool:     t = TYbool;     break;
        case Tchar:     t = TYchar;     break;
        case Twchar:    t = TYwchar_t;  break;
        case Tdchar:    t = TYdchar;    break;
        case Taarray:   t = TYaarray;   break;
        case Tclass:
        case Treference:
        case Tpointer:  t = TYnptr;     break;
        case Tdelegate: t = TYdelegate; break;
        case Tarray:    t = TYdarray;   break;
        case Tsarray:   t = TYstruct;   break;
        case Tnoreturn: t = TYbool;     break;

        case Tstruct:
            t = TYstruct;
            break;

        case Tenum:
        {
            Type *tb = tx->toBasetype();
            const Identifier *id = tx->toDsymbol(nullptr)->ident;
            if (id == Id::__c_long)
                t = tb->ty == Tint32 ? TYlong : TYllong;
            else if (id == Id::__c_ulong)
                t = tb->ty == Tuns32 ? TYulong : TYullong;
            else if (id == Id::__c_long_double)
                t = TYdouble;
            else if (id == Id::__c_complex_float)
                t = TYcfloat;
            else if (id == Id::__c_complex_double)
                t = TYcdouble;
            else if (id == Id::__c_complex_real)
                t = TYcldouble;
            else
                t = totym(tb);
            break;
        }

        case Tident:
        case Ttypeof:
        case Tmixin:
#ifdef DEBUG
            printf("ty = %d, '%s'\n", tx->ty, tx->toChars());
#endif
            error(Loc(), "forward reference of %s", tx->toChars());
            t = TYint;
            break;

        case Tnull:
            t = TYnptr;
            break;

        case Tvector:
        {
            TypeVector *tv = (TypeVector *)tx;
            TypeBasic *tb = tv->elementType();
            switch (tb->ty)
            {
                case Tvoid:
                case Tint8:     t = TYschar16;  break;
                case Tuns8:     t = TYuchar16;  break;
                case Tint16:    t = TYshort8;   break;
                case Tuns16:    t = TYushort8;  break;
                case Tint32:    t = TYlong4;    break;
                case Tuns32:    t = TYulong4;   break;
                case Tint64:    t = TYllong2;   break;
                case Tuns64:    t = TYullong2;  break;
                case Tfloat32:  t = TYfloat4;   break;
                case Tfloat64:  t = TYdouble2;  break;
                default:
                    assert(0);
                    break;
            }
            assert(global.params.is64bit);
            break;
        }

        case Tfunction:
        {
            TypeFunction *tf = (TypeFunction *)tx;
            switch (tf->linkage)
            {
                case LINKc:
                case LINKcpp:
                Lc:
                    t = TYnfunc;
                    if (I32 && retStyle(tf, false) == RETstack)
                        t = TYhfunc;

                    break;

                case LINKd:
                    t = (tf->parameterList.varargs == VARARGvariadic) ? TYnfunc : TYjfunc;
                    break;

                default:
                    printf("linkage = %d\n", tf->linkage);
                    assert(0);
            }
            if (tf->isnothrow)
                t |= mTYnothrow;
            return t;
        }
        default:
#ifdef DEBUG
            printf("ty = %d, '%s'\n", tx->ty, tx->toChars());
            halt();
#endif
            assert(0);
    }

    // Add modifiers
    switch (tx->mod)
    {
        case 0:
            break;
        case MODconst:
        case MODwild:
        case MODwildconst:
            t |= mTYconst;
            break;
        case MODshared:
            t |= mTYshared;
            break;
        case MODshared | MODconst:
        case MODshared | MODwild:
        case MODshared | MODwildconst:
            t |= mTYshared | mTYconst;
            break;
        case MODimmutable:
            t |= mTYimmutable;
            break;
        default:
            assert(0);
    }

    return t;
}

/**************************************
 */

Symbol *toSymbol(Type *t)
{
    if (t->ty == Tclass)
    {
        return toSymbol(((TypeClass *)t)->sym);
    }
    assert(0);
    return nullptr;
}

/**************************************
 * Generate elem that is a dynamic array slice of the module file name.
 */

elem *toEfilename(Module *m)
{
    //printf("toEfilename(%s)\n", m->toChars());
    const char *id = m->srcfile->toChars();
    size_t len = strlen(id);

    if (!m->sfilename)
    {
        // Put out as a static array
        m->sfilename = toStringSymbol(id, len, 1);
    }

    // Turn static array into dynamic array
    return el_pair(TYdarray, el_long(TYsize_t, len), el_ptr(m->sfilename));
}

elem *toEfilenamePtr(Module *m)
{
    //printf("toEfilenamePtr(%s)\n", m.toChars());
    const char *id = m->srcfile->toChars();
    size_t len = strlen(id);
    Symbol *s = toStringSymbol(id, len, 1);
    return el_ptr(s);
}
