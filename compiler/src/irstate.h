
/* Compiler implementation of the D programming language
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/D-Programming-Language/dmd/blob/master/src/irstate.h
 */

#pragma once

class Module;
class Statement;
struct block;
class Dsymbol;
class Identifier;
struct Symbol;
class FuncDeclaration;
struct Blockx;
struct elem;
struct Label;
#include "arraytypes.h"

struct IRState
{
    IRState *prev;
    Statement *statement;
    Module *m;                  // module
    Dsymbol *symbol;
    Identifier *ident;
    Symbol *shidden;            // hidden parameter to function
    Symbol *sthis;              // 'this' parameter to function (member and nested)
    Symbol *sclosure;           // pointer to closure instance
    Blockx *blx;
    Dsymbols *deferToObj;       // array of Dsymbol's to run toObjFile(bool multiobj) on later
    elem *ehidden;              // transmit hidden pointer to CallExp::toElem()
    Symbol *startaddress;
    VarDeclarations *varsInScope; // variables that are in scope that will need destruction later
    void **labels;                // table of labels used/declared in function

    block *breakBlock;
    block *contBlock;
    block *switchBlock;
    block *defaultBlock;
    block *finallyBlock;

    IRState(IRState *irs, Statement *s)
    {
        prev = irs;
        statement = s;
        symbol = nullptr;
        breakBlock = nullptr;
        contBlock = nullptr;
        switchBlock = nullptr;
        defaultBlock = nullptr;
        finallyBlock = nullptr;
        ident = nullptr;
        ehidden = nullptr;
        startaddress = nullptr;
        if (irs)
        {
            m = irs->m;
            shidden = irs->shidden;
            sclosure = irs->sclosure;
            sthis = irs->sthis;
            blx = irs->blx;
            deferToObj = irs->deferToObj;
            varsInScope = irs->varsInScope;
            labels = irs->labels;
        }
        else
        {
            m = nullptr;
            shidden = nullptr;
            sclosure = nullptr;
            sthis = nullptr;
            blx = nullptr;
            deferToObj = nullptr;
            varsInScope = nullptr;
            labels = nullptr;
        }
    }

    IRState(IRState *irs, Dsymbol *s)
    {
        prev = irs;
        statement = nullptr;
        symbol = s;
        breakBlock = nullptr;
        contBlock = nullptr;
        switchBlock = nullptr;
        defaultBlock = nullptr;
        finallyBlock = nullptr;
        ident = nullptr;
        ehidden = nullptr;
        startaddress = nullptr;
        if (irs)
        {
            m = irs->m;
            shidden = irs->shidden;
            sclosure = irs->sclosure;
            sthis = irs->sthis;
            blx = irs->blx;
            deferToObj = irs->deferToObj;
            varsInScope = irs->varsInScope;
            labels = irs->labels;
        }
        else
        {
            m = nullptr;
            shidden = nullptr;
            sclosure = nullptr;
            sthis = nullptr;
            blx = nullptr;
            deferToObj = nullptr;
            varsInScope = nullptr;
            labels = nullptr;
        }
    }

    IRState(Module *m, Dsymbol *s)
    {
        prev = nullptr;
        statement = nullptr;
        this->m = m;
        symbol = s;
        breakBlock = nullptr;
        contBlock = nullptr;
        switchBlock = nullptr;
        defaultBlock = nullptr;
        finallyBlock = nullptr;
        ident = nullptr;
        ehidden = nullptr;
        shidden = nullptr;
        sclosure = nullptr;
        sthis = nullptr;
        blx = nullptr;
        deferToObj = nullptr;
        startaddress = nullptr;
        varsInScope = nullptr;
        labels = nullptr;
    }

    Label **lookupLabel(Statement *s);
    void insertLabel(Statement *s, Label *label);

    block *getBreakBlock(Identifier *ident);
    block *getContBlock(Identifier *ident);
    block *getSwitchBlock();
    block *getDefaultBlock();
    block *getFinallyBlock();
    FuncDeclaration *getFunc();
    bool arrayBoundsCheck();
};
