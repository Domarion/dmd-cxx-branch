
/* Compiler implementation of the D programming language
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/D-Programming-Language/dmd/blob/master/src/scope.c
 */

#include "root/dsystem.hpp"               // strlen()
#include "root/root.hpp"
#include "root/rmem.hpp"
#include "root/speller.hpp"

#include "mars.hpp"
#include "init.hpp"
#include "identifier.hpp"
#include "scope.hpp"
#include "attrib.hpp"
#include "dsymbol.hpp"
#include "declaration.hpp"
#include "statement.hpp"
#include "aggregate.hpp"
#include "module.hpp"
#include "id.hpp"
#include "target.hpp"
#include "template.hpp"

Scope *Scope::freelist = nullptr;

void allocFieldinit(Scope *sc, size_t dim)
{
    sc->fieldinit = (unsigned *)mem.xcalloc(sizeof(unsigned), dim);
    sc->fieldinit_dim = dim;
}

void freeFieldinit(Scope *sc)
{
    if (sc->fieldinit)
        mem.xfree(sc->fieldinit);
    sc->fieldinit = nullptr;
    sc->fieldinit_dim = 0;
}

Scope *Scope::alloc()
{
    if (freelist)
    {
        Scope *s = freelist;
        freelist = s->enclosing;
        //printf("freelist %p\n", s);
        assert(s->flags & SCOPEfree);
        s->flags &= ~SCOPEfree;
        return s;
    }

    return new Scope();
}

Scope::Scope()
{
    // Create root scope

    //printf("Scope::Scope() %p\n", this);
    this->_module = nullptr;
    this->scopesym = nullptr;
    this->enclosing = nullptr;
    this->parent = nullptr;
    this->sw = nullptr;
    this->tf = nullptr;
    this->os = nullptr;
    this->tinst = nullptr;
    this->minst = nullptr;
    this->sbreak = nullptr;
    this->scontinue = nullptr;
    this->fes = nullptr;
    this->callsc = nullptr;
    this->aligndecl = nullptr;
    this->func = nullptr;
    this->slabel = nullptr;
    this->linkage = LINKd;
    this->cppmangle = CPPMANGLEdefault;
    this->inlining = PINLINEdefault;
    this->protection = Visibility(Visibility::public_);
    this->explicitProtection = 0;
    this->stc = 0;
    this->depdecl = nullptr;
    this->inunion = 0;
    this->nofree = 0;
    this->noctor = 0;
    this->intypeof = 0;
    this->lastVar = nullptr;
    this->callSuper = 0;
    this->fieldinit = nullptr;
    this->fieldinit_dim = 0;
    this->flags = 0;
    this->lastdc = nullptr;
    this->anchorCounts = nullptr;
    this->prevAnchor = nullptr;
    this->userAttribDecl = nullptr;
}

Scope *Scope::copy()
{
    Scope *sc = Scope::alloc();
    *sc = *this;    // memcpy

    /* Bugzilla 11777: The copied scope should not inherit fieldinit.
     */
    sc->fieldinit = nullptr;

    return sc;
}

Scope *Scope::createGlobal(Module *_module)
{
    Scope *sc = Scope::alloc();
    *sc = Scope();  // memset

    sc->aligndecl = nullptr;
    sc->linkage = LINKd;
    sc->inlining = PINLINEdefault;
    sc->protection = Visibility(Visibility::public_);

    sc->_module = _module;

    sc->tinst = nullptr;
    sc->minst = _module;

    sc->scopesym = new ScopeDsymbol();
    sc->scopesym->symtab = new DsymbolTable();

    // Add top level package as member of this global scope
    Dsymbol *m = _module;
    while (m->parent)
        m = m->parent;
    m->addMember(nullptr, sc->scopesym);
    m->parent = nullptr;                   // got changed by addMember()

    // Create the module scope underneath the global scope
    sc = sc->push(_module);
    sc->parent = _module;
    return sc;
}

Scope *Scope::push()
{
    Scope *s = copy();

    //printf("Scope::push(this = %p) new = %p\n", this, s);
    assert(!(flags & SCOPEfree));
    s->scopesym = nullptr;
    s->enclosing = this;
    s->slabel = nullptr;
    s->nofree = 0;
    s->fieldinit = saveFieldInit();
    s->flags = (flags & (SCOPEcontract | SCOPEdebug | SCOPEctfe | SCOPEcompile | SCOPEconstraint |
                         SCOPEnoaccesscheck | SCOPEignoresymbolvisibility |
                         SCOPEprintf | SCOPEscanf));
    s->lastdc = nullptr;

    assert(this != s);
    return s;
}

Scope *Scope::push(ScopeDsymbol *ss)
{
    //printf("Scope::push(%s)\n", ss->toChars());
    Scope *s = push();
    s->scopesym = ss;
    return s;
}

Scope *Scope::pop()
{
    //printf("Scope::pop() %p nofree = %d\n", this, nofree);
    Scope *enc = enclosing;

    if (enclosing)
    {
        enclosing->callSuper |= callSuper;
        if (fieldinit)
        {
            if (enclosing->fieldinit)
            {
                assert(fieldinit != enclosing->fieldinit);
                size_t dim = fieldinit_dim;
                for (size_t i = 0; i < dim; i++)
                    enclosing->fieldinit[i] |= fieldinit[i];
            }
            freeFieldinit(this);
        }
    }

    if (!nofree)
    {
        enclosing = freelist;
        freelist = this;
        flags |= SCOPEfree;
    }

    return enc;
}

Scope *Scope::startCTFE()
{
    Scope *sc = this->push();
    sc->flags = this->flags | SCOPEctfe;
    return sc;
}

Scope *Scope::endCTFE()
{
    assert(flags & SCOPEctfe);
    return pop();
}

void Scope::mergeCallSuper(Loc loc, unsigned cs)
{
    // This does a primitive flow analysis to support the restrictions
    // regarding when and how constructors can appear.
    // It merges the results of two paths.
    // The two paths are callSuper and cs; the result is merged into callSuper.

    if (cs != callSuper)
    {
        // Have ALL branches called a constructor?
        int aAll = (cs        & (CSXthis_ctor | CSXsuper_ctor)) != 0;
        int bAll = (callSuper & (CSXthis_ctor | CSXsuper_ctor)) != 0;

        // Have ANY branches called a constructor?
        bool aAny = (cs        & CSXany_ctor) != 0;
        bool bAny = (callSuper & CSXany_ctor) != 0;

        // Have any branches returned?
        bool aRet = (cs        & CSXreturn) != 0;
        bool bRet = (callSuper & CSXreturn) != 0;

        // Have any branches halted?
        bool aHalt = (cs        & CSXhalt) != 0;
        bool bHalt = (callSuper & CSXhalt) != 0;

        bool ok = true;

        if (aHalt && bHalt)
        {
            callSuper = CSXhalt;
        }
        else if ((!aHalt && aRet && !aAny && bAny) ||
                 (!bHalt && bRet && !bAny && aAny))
        {
            // If one has returned without a constructor call, there must be never
            // have been ctor calls in the other.
            ok = false;
        }
        else if (aHalt || (aRet && aAll))
        {
            // If one branch has called a ctor and then exited, anything the
            // other branch has done is OK (except returning without a
            // ctor call, but we already checked that).
            callSuper |= cs & (CSXany_ctor | CSXlabel);
        }
        else if (bHalt || (bRet && bAll))
        {
            callSuper = cs | (callSuper & (CSXany_ctor | CSXlabel));
        }
        else
        {
            // Both branches must have called ctors, or both not.
            ok = (aAll == bAll);
            // If one returned without a ctor, we must remember that
            // (Don't bother if we've already found an error)
            if (ok && aRet && !aAny)
                callSuper |= CSXreturn;
            callSuper |= cs & (CSXany_ctor | CSXlabel);
        }
        if (!ok)
            error(loc, "one path skips constructor");
    }
}

unsigned *Scope::saveFieldInit()
{
    unsigned *fi = nullptr;
    if (fieldinit)  // copy
    {
        size_t dim = fieldinit_dim;
        fi = (unsigned *)mem.xmalloc(sizeof(unsigned) * dim);
        for (size_t i = 0; i < dim; i++)
            fi[i] = fieldinit[i];
    }
    return fi;
}

/****************************************
 * Merge `b` flow analysis results into `a`.
 * Params:
 *      a = the path to merge fi into
 *      b = the other path
 * Returns:
 *      false means either `a` or `b` skips initialization
 */
static bool mergeFieldInit(unsigned &a, const unsigned b)
{
    if (b == a)
        return true;

    // Have any branches returned?
    bool aRet = (a & CSXreturn) != 0;
    bool bRet = (b & CSXreturn) != 0;

    // Have any branches halted?
    bool aHalt = (a & CSXhalt) != 0;
    bool bHalt = (b & CSXhalt) != 0;

    if (aHalt && bHalt)
    {
        a = CSXhalt;
        return true;
    }

    // The logic here is to prefer the branch that neither halts nor returns.
    bool ok;
    if (!bHalt && bRet)
    {
        // Branch b returns, no merging required.
        ok = (b & CSXthis_ctor);
    }
    else if (!aHalt && aRet)
    {
        // Branch a returns, but b doesn't, b takes precedence.
        ok = (a & CSXthis_ctor);
        a = b;
    }
    else if (bHalt)
    {
        // Branch b halts, no merging required.
        ok = (a & CSXthis_ctor);
    }
    else if (aHalt)
    {
        // Branch a halts, but b doesn't, b takes precedence
        ok = (b & CSXthis_ctor);
        a = b;
    }
    else
    {
        // Neither branch returns nor halts, merge flags
        ok = !((a ^ b) & CSXthis_ctor);
        a |= b;
    }
    return ok;
}

void Scope::mergeFieldInit(Loc loc, unsigned *fies)
{
    if (fieldinit && fies)
    {
        FuncDeclaration *f = func;
        if (fes) f = fes->func;
        AggregateDeclaration *ad = f->isMember2();
        assert(ad);

        for (size_t i = 0; i < ad->fields.length; i++)
        {
            VarDeclaration *v = ad->fields[i];
            bool mustInit = (v->storage_class & STCnodefaultctor ||
                             v->type->needsNested());

            if (!::mergeFieldInit(fieldinit[i], fies[i]) && mustInit)
            {
                ::error(loc, "one path skips field %s", v->toChars());
            }
        }
    }
}

Module *Scope::instantiatingModule()
{
    // TODO: in speculative context, returning 'module' is correct?
    return minst ? minst : _module;
}

static Dsymbol *searchScopes(Scope *scope, Loc loc, Identifier *ident, Dsymbol **pscopesym, int flags)
{
    for (Scope *sc = scope; sc; sc = sc->enclosing)
    {
        assert(sc != sc->enclosing);
        if (!sc->scopesym)
            continue;
        //printf("\tlooking in scopesym '%s', kind = '%s', flags = x%x\n", sc->scopesym->toChars(), sc->scopesym->kind(), flags);

        if (sc->scopesym->isModule())
            flags |= SearchUnqualifiedModule;        // tell Module.search() that SearchLocalsOnly is to be obeyed

        if (Dsymbol *s = sc->scopesym->search(loc, ident, flags))
        {
            if (!(flags & (SearchImportsOnly | IgnoreErrors)) &&
                ident == Id::length && sc->scopesym->isArrayScopeSymbol() &&
                sc->enclosing && sc->enclosing->search(loc, ident, nullptr, flags))
            {
                warning(s->loc, "array `length` hides other `length` name in outer scope");
            }
            if (pscopesym)
                *pscopesym = sc->scopesym;
            return s;
        }
        // Stop when we hit a module, but keep going if that is not just under the global scope
        if (sc->scopesym->isModule() && !(sc->enclosing && !sc->enclosing->enclosing))
            break;
    }
    return nullptr;
}

/************************************
 * Perform unqualified name lookup by following the chain of scopes up
 * until found.
 *
 * Params:
 *  loc = location to use for error messages
 *  ident = name to look up
 *  pscopesym = if supplied and name is found, set to scope that ident was found in
 *  flags = modify search based on flags
 *
 * Returns:
 *  symbol if found, null if not
 */
Dsymbol *Scope::search(Loc loc, Identifier *ident, Dsymbol **pscopesym, int flags)
{
    // This function is called only for unqualified lookup
    assert(!(flags & (SearchLocalsOnly | SearchImportsOnly)));

    /* If ident is "start at module scope", only look at module scope
     */
    if (ident == Id::empty)
    {
        // Look for module scope
        for (Scope *sc = this; sc; sc = sc->enclosing)
        {
            assert(sc != sc->enclosing);
            if (!sc->scopesym)
                continue;

            if (Dsymbol *s = sc->scopesym->isModule())
            {
                if (pscopesym)
                    *pscopesym = sc->scopesym;
                return s;
            }
        }
        return nullptr;
    }

    if (this->flags & SCOPEignoresymbolvisibility)
        flags |= IgnoreSymbolVisibility;

    // First look in local scopes
    Dsymbol *s = searchScopes(this, loc, ident, pscopesym, flags | SearchLocalsOnly);
    if (!s)
    {
        // Second look in imported modules
        s = searchScopes(this, loc, ident, pscopesym, flags | SearchImportsOnly);
    }
    return s;
}

Dsymbol *Scope::insert(Dsymbol *s)
{
    if (VarDeclaration *vd = s->isVarDeclaration())
    {
        if (lastVar)
            vd->lastVar = lastVar;
        lastVar = vd;
    }
    else if (WithScopeSymbol *ss = s->isWithScopeSymbol())
    {
        if (VarDeclaration *wthis = ss->withstate->wthis)
        {
            if (lastVar)
                wthis->lastVar = lastVar;
            lastVar = wthis;
        }
        return nullptr;
    }
    for (Scope *sc = this; sc; sc = sc->enclosing)
    {
        //printf("\tsc = %p\n", sc);
        if (sc->scopesym)
        {
            //printf("\t\tsc->scopesym = %p\n", sc->scopesym);
            if (!sc->scopesym->symtab)
                sc->scopesym->symtab = new DsymbolTable();
            return sc->scopesym->symtabInsert(s);
        }
    }
    assert(0);
    return nullptr;
}

/********************************************
 * Search enclosing scopes for ClassDeclaration.
 */

ClassDeclaration *Scope::getClassScope()
{
    for (Scope *sc = this; sc; sc = sc->enclosing)
    {
        if (!sc->scopesym)
            continue;

        ClassDeclaration *cd = sc->scopesym->isClassDeclaration();
        if (cd)
            return cd;
    }
    return nullptr;
}

/********************************************
 * Search enclosing scopes for ClassDeclaration or StructDeclaration.
 */

AggregateDeclaration *Scope::getStructClassScope()
{
    for (Scope *sc = this; sc; sc = sc->enclosing)
    {
        if (!sc->scopesym)
            continue;

        AggregateDeclaration *ad = sc->scopesym->isClassDeclaration();
        if (ad)
            return ad;
        ad = sc->scopesym->isStructDeclaration();
        if (ad)
            return ad;
    }
    return nullptr;
}

/*******************************************
 * For TemplateDeclarations, we need to remember the Scope
 * where it was declared. So mark the Scope as not
 * to be free'd.
 */

void Scope::setNoFree()
{
    //int i = 0;

    //printf("Scope::setNoFree(this = %p)\n", this);
    for (Scope *sc = this; sc; sc = sc->enclosing)
    {
        //printf("\tsc = %p\n", sc);
        sc->nofree = 1;

        assert(!(flags & SCOPEfree));
        //assert(sc != sc->enclosing);
        //assert(!sc->enclosing || sc != sc->enclosing->enclosing);
        //if (++i == 10)
            //assert(0);
    }
}

structalign_t Scope::alignment()
{
    if (aligndecl)
        return aligndecl->getAlignment(this);
    else
        return STRUCTALIGN_DEFAULT;
}

/************************************************
 * Given the failed search attempt, try to find
 * one with a close spelling.
 */

static void *scope_search_fp(void *arg, const char *seed, int* cost)
{
    //printf("scope_search_fp('%s')\n", seed);

    /* If not in the lexer's string table, it certainly isn't in the symbol table.
     * Doing this first is a lot faster.
     */
    size_t len = strlen(seed);
    if (!len)
        return nullptr;
    Identifier *id = Identifier::lookup(seed, len);
    if (!id)
        return nullptr;

    Scope *sc = (Scope *)arg;
    Module::clearCache();
    Dsymbol *scopesym = nullptr;
    Dsymbol *s = sc->search(Loc(), id, &scopesym, IgnoreErrors);
    if (s)
    {
        for (*cost = 0; sc; sc = sc->enclosing, (*cost)++)
            if (sc->scopesym == scopesym)
                break;
        if (scopesym != s->parent)
        {
            (*cost)++; // got to the symbol through an import
            if (s->prot().kind == Visibility::private_)
                return nullptr;
        }
    }
    return (void*)s;
}

Dsymbol *Scope::search_correct(Identifier *ident)
{
    if (global.gag)
        return nullptr;            // don't do it for speculative compiles; too time consuming

    Dsymbol *scopesym = nullptr;
    // search for exact name first
    if (Dsymbol *s = search(Loc(), ident, &scopesym, IgnoreErrors))
        return s;
    return (Dsymbol *)speller(ident->toChars(), &scope_search_fp, this, idchars);
}

/************************************
 * Maybe `ident` was a C or C++ name. Check for that,
 * and suggest the D equivalent.
 * Params:
 *  ident = unknown identifier
 * Returns:
 *  D identifier string if found, null if not
 */
const char *Scope::search_correct_C(Identifier *ident)
{
    TOK tok;
    if (ident == Id::C_NULL)
        tok = TOKnull;
    else if (ident == Id::C_TRUE)
        tok = TOKtrue;
    else if (ident == Id::C_FALSE)
        tok = TOKfalse;
    else if (ident == Id::C_unsigned)
        tok = TOKuns32;
    else if (ident == Id::C_wchar_t)
        tok = target.c.twchar_t->ty == Twchar ? TOKwchar : TOKdchar;
    else
        return nullptr;
    return Token::toChars(tok);
}
