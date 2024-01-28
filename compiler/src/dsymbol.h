
/* Compiler implementation of the D programming language
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/dlang/dmd/blob/master/src/dmd/dsymbol.h
 */

#pragma once

#include "root/port.h"
#include "root/stringtable.h"
#include "ast_node.h"
#include "globals.h"
#include "arraytypes.h"
#include "visitor.h"

class Identifier;
struct Scope;
class DsymbolTable;
class Declaration;
class ThisDeclaration;
class TypeInfoDeclaration;
class TupleDeclaration;
class AliasDeclaration;
class AggregateDeclaration;
class EnumDeclaration;
class ClassDeclaration;
class InterfaceDeclaration;
class StructDeclaration;
class UnionDeclaration;
class FuncDeclaration;
class FuncAliasDeclaration;
class OverDeclaration;
class FuncLiteralDeclaration;
class CtorDeclaration;
class PostBlitDeclaration;
class DtorDeclaration;
class StaticCtorDeclaration;
class StaticDtorDeclaration;
class SharedStaticCtorDeclaration;
class SharedStaticDtorDeclaration;
class InvariantDeclaration;
class UnitTestDeclaration;
class NewDeclaration;
class VarDeclaration;
class AttribDeclaration;
class Package;
class Module;
class Import;
class Type;
class TypeTuple;
class WithStatement;
class LabelDsymbol;
class ScopeDsymbol;
class ForwardingScopeDsymbol;
class TemplateDeclaration;
class TemplateInstance;
class TemplateMixin;
class ForwardingAttribDeclaration;
class Nspace;
class EnumMember;
class WithScopeSymbol;
class ArrayScopeSymbol;
class SymbolDeclaration;
class Expression;
class OverloadSet;
struct AA;
struct Symbol;


struct Ungag
{
    unsigned oldgag;

    Ungag(unsigned old) : oldgag(old) {}
    ~Ungag() { global.gag = oldgag; }
};

void dsymbolSemantic(Dsymbol *dsym, Scope *sc);
void semantic2(Dsymbol *dsym, Scope* sc);
void semantic3(Dsymbol *dsym, Scope* sc);

struct Visibility
{
    enum Kind
    {
        undefined,
        none,           // no access
        private_,
        package_,
        protected_,
        public_,
        export_
    };
    Kind kind;
    Package *pkg;

    Visibility();
    Visibility(Kind kind);

    bool isMoreRestrictiveThan(const Visibility other) const;
    bool operator==(const Visibility& other) const;
};

// in hdrgen.c
void protectionToBuffer(OutBuffer *buf, Visibility prot);
const char *protectionToChars(Visibility::Kind kind);

/* State of symbol in winding its way through the passes of the compiler
 */
enum PASS
{
    PASSinit,           // initial state
    PASSsemantic,       // semantic() started
    PASSsemanticdone,   // semantic() done
    PASSsemantic2,      // semantic2() started
    PASSsemantic2done,  // semantic2() done
    PASSsemantic3,      // semantic3() started
    PASSsemantic3done,  // semantic3() done
    PASSinline,         // inline started
    PASSinlinedone,     // inline done
    PASSobj             // toObjFile() run
};

/* Flags for symbol search
 */
enum
{
    IgnoreNone              = 0x00, // default
    IgnorePrivateImports    = 0x01, // don't search private imports
    IgnoreErrors            = 0x02, // don't give error messages
    IgnoreAmbiguous         = 0x04, // return nullptr if ambiguous
    SearchLocalsOnly        = 0x08, // only look at locals (don't search imports)
    SearchImportsOnly       = 0x10, // only look in imports
    SearchUnqualifiedModule = 0x20, // the module scope search is unqualified,
                                    // meaning don't search imports in that scope,
                                    // because qualified module searches search
                                    // their imports
    IgnoreSymbolVisibility  = 0x80  // also find private and package protected symbols
};

typedef int (*Dsymbol_apply_ft_t)(Dsymbol *, void *);

class Dsymbol : public ASTNode
{
public:
    Identifier *ident;
    Dsymbol *parent;
    Symbol *csym;               // symbol for code generator
    Symbol *isym;               // import version of csym
    const utf8_t *comment;      // documentation comment for this Dsymbol
    Loc loc;                    // where defined
    Scope *_scope;               // !=nullptr means context to use for semantic()
    const utf8_t *prettystring;
    bool errors;                // this symbol failed to pass semantic()
    PASS semanticRun;
    DeprecatedDeclaration *depdecl; // customized deprecation message
    UserAttributeDeclaration *userAttribDecl;   // user defined attributes
    UnitTestDeclaration *ddocUnittest; // !=nullptr means there's a ddoc unittest associated with this symbol (only use this with ddoc)

    Dsymbol();
    Dsymbol(Identifier *);
    static Dsymbol *create(Identifier *);
    const char *toChars();
    virtual const char *toPrettyCharsHelper(); // helper to print fully qualified (template) arguments
    Loc& getLoc();
    const char *locToChars();
    bool equals(RootObject *o);
    bool isAnonymous();
    void error(Loc loc, const char *format, ...);
    void error(const char *format, ...);
    void deprecation(Loc loc, const char *format, ...);
    void deprecation(const char *format, ...);
    bool checkDeprecated(Loc loc, Scope *sc);
    Module *getModule();
    Module *getAccessModule();
    Dsymbol *pastMixin();
    Dsymbol *pastMixinAndNspace();
    Dsymbol *toParent();
    Dsymbol *toParent2();
    Dsymbol *toParent3();
    TemplateInstance *isInstantiated();
    TemplateInstance *isSpeculative();
    Ungag ungagSpeculative();

    // kludge for template.isSymbol()
    int dyncast() const { return DYNCAST_DSYMBOL; }

    static Dsymbols *arraySyntaxCopy(Dsymbols *a);

    virtual Identifier *getIdent();
    virtual const char *toPrettyChars(bool QualifyTypes = false);
    virtual const char *kind() const;
    virtual Dsymbol *toAlias();                 // resolve real symbol
    virtual Dsymbol *toAlias2();
    virtual int apply(Dsymbol_apply_ft_t fp, void *param);
    virtual void addMember(Scope *sc, ScopeDsymbol *sds);
    virtual void setScope(Scope *sc);
    virtual void importAll(Scope *sc);
    virtual Dsymbol *search(const Loc &loc, Identifier *ident, int flags = IgnoreNone);
    Dsymbol *search_correct(Identifier *id);
    Dsymbol *searchX(Loc loc, Scope *sc, RootObject *id, int flags);
    virtual bool overloadInsert(Dsymbol *s);
    virtual d_uns64 size(Loc loc);
    virtual bool isforwardRef();
    virtual AggregateDeclaration *isThis();     // is a 'this' required to access the member
    virtual bool isExport() const;              // is Dsymbol exported?
    virtual bool isImportedSymbol() const;      // is Dsymbol imported?
    virtual bool isDeprecated();                // is Dsymbol deprecated?
    virtual bool isOverloadable();
    virtual LabelDsymbol *isLabel();            // is this a LabelDsymbol?
    AggregateDeclaration *isMember();           // is this a member of an AggregateDeclaration?
    AggregateDeclaration *isMember2();          // is this a member of an AggregateDeclaration?
    ClassDeclaration *isClassMember();          // is this a member of a ClassDeclaration?
    virtual Type *getType();                    // is this a type?
    virtual bool needThis();                    // need a 'this' pointer?
    virtual Visibility prot();
    virtual Dsymbol *syntaxCopy(Dsymbol *s);    // copy only syntax trees
    virtual bool oneMember(Dsymbol **ps, Identifier *ident);
    static bool oneMembers(Dsymbols *members, Dsymbol **ps, Identifier *ident);
    virtual void setFieldOffset(AggregateDeclaration *ad, unsigned *poffset, bool isunion);
    virtual bool hasPointers();
    virtual bool hasStaticCtorOrDtor();
    virtual void addLocalClass(ClassDeclarations *) { }
    virtual void checkCtorConstInit() { }

    virtual void addComment(const utf8_t *comment);

    bool inNonRoot();

    // Eliminate need for dynamic_cast
    virtual Package *isPackage() { return nullptr; }
    virtual Module *isModule() { return nullptr; }
    virtual EnumMember *isEnumMember() { return nullptr; }
    virtual TemplateDeclaration *isTemplateDeclaration() { return nullptr; }
    virtual TemplateInstance *isTemplateInstance() { return nullptr; }
    virtual TemplateMixin *isTemplateMixin() { return nullptr; }
    virtual ForwardingAttribDeclaration *isForwardingAttribDeclaration() { return nullptr; }
    virtual Nspace *isNspace() { return nullptr; }
    virtual Declaration *isDeclaration() { return nullptr; }
    virtual StorageClassDeclaration *isStorageClassDeclaration(){ return nullptr; }
    virtual ThisDeclaration *isThisDeclaration() { return nullptr; }
    virtual TypeInfoDeclaration *isTypeInfoDeclaration() { return nullptr; }
    virtual TupleDeclaration *isTupleDeclaration() { return nullptr; }
    virtual AliasDeclaration *isAliasDeclaration() { return nullptr; }
    virtual AggregateDeclaration *isAggregateDeclaration() { return nullptr; }
    virtual FuncDeclaration *isFuncDeclaration() { return nullptr; }
    virtual FuncAliasDeclaration *isFuncAliasDeclaration() { return nullptr; }
    virtual OverDeclaration *isOverDeclaration() { return nullptr; }
    virtual FuncLiteralDeclaration *isFuncLiteralDeclaration() { return nullptr; }
    virtual CtorDeclaration *isCtorDeclaration() { return nullptr; }
    virtual PostBlitDeclaration *isPostBlitDeclaration() { return nullptr; }
    virtual DtorDeclaration *isDtorDeclaration() { return nullptr; }
    virtual StaticCtorDeclaration *isStaticCtorDeclaration() { return nullptr; }
    virtual StaticDtorDeclaration *isStaticDtorDeclaration() { return nullptr; }
    virtual SharedStaticCtorDeclaration *isSharedStaticCtorDeclaration() { return nullptr; }
    virtual SharedStaticDtorDeclaration *isSharedStaticDtorDeclaration() { return nullptr; }
    virtual InvariantDeclaration *isInvariantDeclaration() { return nullptr; }
    virtual UnitTestDeclaration *isUnitTestDeclaration() { return nullptr; }
    virtual NewDeclaration *isNewDeclaration() { return nullptr; }
    virtual VarDeclaration *isVarDeclaration() { return nullptr; }
    virtual VersionSymbol *isVersionSymbol() { return nullptr; }
    virtual DebugSymbol *isDebugSymbol() { return nullptr; }
    virtual ClassDeclaration *isClassDeclaration() { return nullptr; }
    virtual StructDeclaration *isStructDeclaration() { return nullptr; }
    virtual UnionDeclaration *isUnionDeclaration() { return nullptr; }
    virtual InterfaceDeclaration *isInterfaceDeclaration() { return nullptr; }
    virtual ScopeDsymbol *isScopeDsymbol() { return nullptr; }
    virtual ForwardingScopeDsymbol *isForwardingScopeDsymbol() { return nullptr; }
    virtual WithScopeSymbol *isWithScopeSymbol() { return nullptr; }
    virtual ArrayScopeSymbol *isArrayScopeSymbol() { return nullptr; }
    virtual Import *isImport() { return nullptr; }
    virtual EnumDeclaration *isEnumDeclaration() { return nullptr; }
    virtual SymbolDeclaration *isSymbolDeclaration() { return nullptr; }
    virtual AttribDeclaration *isAttribDeclaration() { return nullptr; }
    virtual AnonDeclaration *isAnonDeclaration() { return nullptr; }
    virtual OverloadSet *isOverloadSet() { return nullptr; }
    void accept(Visitor *v) { v->visit(this); }
};

// Dsymbol that generates a scope

class ScopeDsymbol : public Dsymbol
{
public:
    Dsymbols *members;          // all Dsymbol's in this scope
    DsymbolTable *symtab;       // members[] sorted into table
    unsigned endlinnum;         // the linnumber of the statement after the scope (0 if unknown)

private:
    Dsymbols *importedScopes;   // imported Dsymbol's
    Visibility::Kind *prots;            // array of PROTKIND, one for each import

    BitArray accessiblePackages, privateAccessiblePackages;

public:
    ScopeDsymbol();
    ScopeDsymbol(Identifier *id);
    Dsymbol *syntaxCopy(Dsymbol *s);
    Dsymbol *search(const Loc &loc, Identifier *ident, int flags = SearchLocalsOnly);
    OverloadSet *mergeOverloadSet(Identifier *ident, OverloadSet *os, Dsymbol *s);
    virtual void importScope(Dsymbol *s, Visibility protection);
    void addAccessiblePackage(Package *p, Visibility protection);
    virtual bool isPackageAccessible(Package *p, Visibility protection, int flags = 0);
    bool isforwardRef();
    static void multiplyDefined(Loc loc, Dsymbol *s1, Dsymbol *s2);
    const char *kind() const;
    FuncDeclaration *findGetMembers();
    virtual Dsymbol *symtabInsert(Dsymbol *s);
    virtual Dsymbol *symtabLookup(Dsymbol *s, Identifier *id);
    bool hasStaticCtorOrDtor();

    static size_t dim(Dsymbols *members);
    static Dsymbol *getNth(Dsymbols *members, size_t nth, size_t *pn = nullptr);

    ScopeDsymbol *isScopeDsymbol() { return this; }
    void accept(Visitor *v) { v->visit(this); }
};

// With statement scope

class WithScopeSymbol : public ScopeDsymbol
{
public:
    WithStatement *withstate;

    WithScopeSymbol(WithStatement *withstate);
    Dsymbol *search(const Loc &loc, Identifier *ident, int flags = SearchLocalsOnly);

    WithScopeSymbol *isWithScopeSymbol() { return this; }
    void accept(Visitor *v) { v->visit(this); }
};

// Array Index/Slice scope

class ArrayScopeSymbol : public ScopeDsymbol
{
public:
    Expression *exp;    // IndexExp or SliceExp
    TypeTuple *type;    // for tuple[length]
    TupleDeclaration *td;       // for tuples of objects
    Scope *sc;

    ArrayScopeSymbol(Scope *sc, Expression *e);
    ArrayScopeSymbol(Scope *sc, TypeTuple *t);
    ArrayScopeSymbol(Scope *sc, TupleDeclaration *td);
    Dsymbol *search(const Loc &loc, Identifier *ident, int flags = IgnoreNone);

    ArrayScopeSymbol *isArrayScopeSymbol() { return this; }
    void accept(Visitor *v) { v->visit(this); }
};

// Overload Sets

class OverloadSet : public Dsymbol
{
public:
    Dsymbols a;         // array of Dsymbols

    OverloadSet(Identifier *ident, OverloadSet *os = nullptr);
    void push(Dsymbol *s);
    OverloadSet *isOverloadSet() { return this; }
    const char *kind() const;
    void accept(Visitor *v) { v->visit(this); }
};

// Forwarding ScopeDsymbol

class ForwardingScopeDsymbol : public ScopeDsymbol
{
public:
    ScopeDsymbol *forward;

    ForwardingScopeDsymbol(ScopeDsymbol *forward);
    Dsymbol *symtabInsert(Dsymbol *s);
    Dsymbol *symtabLookup(Dsymbol *s, Identifier *id);
    void importScope(Dsymbol *s, Visibility protection);
    const char *kind() const;

    ForwardingScopeDsymbol *isForwardingScopeDsymbol() { return this; }
};

// Table of Dsymbol's

class DsymbolTable : public RootObject
{
public:
    AA *tab;

    DsymbolTable();

    // Look up Identifier. Return Dsymbol if found, nullptr if not.
    Dsymbol *lookup(Identifier const * const ident);

    // Insert Dsymbol in table. Return nullptr if already there.
    Dsymbol *insert(Dsymbol *s);

    // Look for Dsymbol in table. If there, return it. If not, insert s and return that.
    Dsymbol *update(Dsymbol *s);
    Dsymbol *insert(Identifier const * const ident, Dsymbol *s);     // when ident and s are not the same
};
