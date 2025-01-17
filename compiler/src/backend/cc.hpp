// Copyright (C) 1985-1998 by Symantec
// Copyright (C) 2000-2021 by The D Language Foundation, All Rights Reserved
// http://www.digitalmars.com
// Written by Walter Bright
/*
 * This source file is made available for personal use
 * only. The license is in backendlicense.txt
 * For any other uses, please contact Digital Mars.
 */

#pragma once

#define GENOBJ          1       // generating .obj file

#define mskl(i)         (1L << (i))     /* convert int to mask          */

#ifndef STATIC
#define STATIC  static
#endif

// Warnings
enum WM
{
        WM_no_inline    = 1,    //function '%s' is too complicated to inline
        WM_assignment   = 2,    //possible unintended assignment
        WM_nestcomment  = 3,    //comments do not nest
        WM_assignthis   = 4,    //assignment to 'this' is obsolete, use X::operator new/delete
        WM_notagname    = 5,    //no tag name for struct or enum
        WM_valuenotused = 6,    //value of expression is not used
        WM_extra_semi   = 7,    //possible extraneous ';'
        WM_large_auto   = 8,    //very large automatic
        WM_obsolete_del = 9,    //use delete[] rather than delete[expr], expr ignored
        WM_obsolete_inc = 10,   //using operator++() (or --) instead of missing operator++(int)
        WM_init2tmp     = 11,   //non-const reference initialized to temporary
        WM_used_b4_set  = 12,   //variable '%s' used before set
        WM_bad_op       = 13,   //Illegal type/size of operands for the %s instruction
        WM_386_op       = 14,   //Reference to '%s' caused a 386 instruction to be generated
        WM_ret_auto     = 15,   //returning address of automatic '%s'
        WM_ds_ne_dgroup = 16,   //DS is not equal to DGROUP
        WM_unknown_pragma = 17, //unrecognized pragma
        WM_implied_ret  = 18,   //implied return at closing '}' does not return value
        WM_num_args     = 19,   //%d actual arguments expected for %s, had %d
        WM_before_pch   = 20,   //symbols or macros defined before #include of precompiled header
        WM_pch_first    = 21,   //precompiled header must be first #include when -H is used
        WM_pch_config   = 22,
        WM_divby0       = 23,
        WM_badnumber    = 24,
        WM_ccast        = 25,
        WM_obsolete     = 26,
        WM_skip_attribute   = 27, // skip GNUC attribute specification
        WM_warning_message  = 28, // preprocessor warning message
        WM_bad_vastart      = 29, // args for builtin va_start bad
        WM_undefined_inline = 30, // static inline not expanded or defined
};

// Language for error messages
enum LANG
{       LANGenglish,
        LANGgerman,
        LANGfrench,
        LANGjapanese,
};

#include        "cdef.hpp"        // host and target compiler definition

#define INITIALIZED_STATIC_DEF  static

#if MEMMODELS == 1
#define LARGEDATA 0     /* don't want 48 bit pointers */
#define LARGECODE 0
#endif

#include        "ty.hpp"
#include        "../tk/mem.hpp"
#include        "list.hpp"
#include        "vec.hpp"

#define COMPILER "C/C++ Compiler"
#define ACTIVITY "compiling..."

#ifdef DEBUG
#   define debug(a)     (a)
#   define debugx(a)    (a)
#   define debug_assert assert
#else
#   define debug(a)
#   define debugx(a)
#   define debug_assert(e)
#endif

/***************************
 * Print out debugging information.
 */

#ifdef DEBUG
#define debugmes(s)     (debugw && dbg_printf(s))
#define cmes(s)         (debugc && dbg_printf(s))
#define cmes2(s,b)      (debugc && dbg_printf((s),(b)))
#define cmes3(s,b,c)    (debugc && dbg_printf((s),(b),(c)))
#else
#define debugmes(s)
#define cmes(s)
#define cmes2(s,b)
#define cmes3(s,b,c)
#endif

#define TRUE            1
#define FALSE           0

#define arraysize(array)        (sizeof(array) / sizeof(array[0]))

#define IDMAX   900 //467 //254 // identifier max (excluding terminating 0)
#define IDOHD   (4+1+sizeof(int)*3)     // max amount of overhead to ID added by
#define STRMAX  65000           // max length of string (determined by
                                // max ph size)

enum SC;
struct Thunk;
struct token_t;
struct PARAM;
typedef struct PARAM param_t;
struct block;
struct Classsym;
struct Nspacesym;
struct Outbuffer;
struct Aliassym;
struct dt_t;
typedef struct TYPE type;
typedef struct Symbol symbol;
typedef Symbol Funcsym;
//typedef Symbol Classsym;      // a Symbol that is an SCclass, SCstruct or SCunion
struct elem;
typedef list_t symlist_t;       /* list of pointers to Symbols          */
typedef struct SYMTAB_S symtab_t;
struct code;

extern Config config;

/////////// Position in source file

typedef struct Srcpos
{
    unsigned Slinnum;           // 0 means no info available
    unsigned Scharnum;
    const char *Sfilename;
    #define srcpos_name(p)      ((p)->Sfilename)

#if M_UNIX
    short Sfilnum;              // file number
#endif

    void print(const char *func);
} Srcpos;

#include "token.hpp"

/**********************************
 * Current 'state' of the compiler.
 * Used to gather together most global variables.
 * This struct is saved/restored during function body parsing.
 */

typedef struct Pstate
{
    char STinopeq;              // if in n2_createopeq()
    char STinarglist;           // if !=0, then '>' is the end of a template
                                // argument list, not an operator
    char STinsizeof;            // !=0 if in a sizeof expression. Useful to
                                // prevent <array of> being converted to
                                // <pointer to>.
    char STintemplate;          // if !=0, then expanding a function template
                                // (do not expand template Symbols)
    char STdeferDefaultArg;     // defer parsing of default arg for parameter
    char STnoexpand;            // if !=0, don't expand template symbols
    char STignoretal;           // if !=0 ignore template argument list
    char STexplicitInstantiation;       // if !=0, then template explicit instantiation
    char STexplicitSpecialization;      // if !=0, then template explicit specialization
    char STinconstexp;          // if !=0, then parsing a constant expression
    char STisaddr;              // is this a possible pointer to member expression?
    unsigned STinexp;           // if !=0, then in an expression
    Funcsym *STfuncsym_p;       // if inside a function, then this is the
                                // function Symbol.

#   define funcsym_p (pstate.STfuncsym_p)

    unsigned STflags;
#       define PFLpreprocessor  1       // in preprocessor
#       define preprocessor     (pstate.STflags & PFLpreprocessor)
#       define PFLmasm          2       // in Microsoft-style inline assembler
#       define PFLbasm          4       // in Borland-style inline assembler
#       define inline_asm       (pstate.STflags & (PFLmasm | PFLbasm))
#       define PFLsemi          8       // ';' means start of comment
//#     define PFLautogen       0x10    // automatically generate HX ph file
#       define PFLmftemp        0x20    // if expanding member function template
#       define PFLextdef        0x40    // we had an external def
#       define PFLhxwrote       0x80    // already generated HX ph file
#       define PFLhxdone        0x100   // done with HX ph file
#if TX86
#       define PFLhxread        0x200   // have read in an HX ph file
#       define PFLhxgen         0x400   // need to generate HX ph file
#       define PFLphread        0x800   // read a ph file
#       define PFLcomdef        0x1000  // had a common block
#       define PFLmacdef        0x2000  // defined a macro in the source code
#       define PFLsymdef        0x4000  // declared a global Symbol in the source
#       define PFLinclude       0x8000  // read a .h file
#       define PFLmfc           0x10000 // something will affect MFC compatibility
#endif

#if TX86
    // should probably be inside #ifHYDRATE, but unclear for the dmc source
    char SThflag;               // FLAG_XXXX: hydration flag
#define FLAG_INPLACE    0       // in place hydration
#define FLAG_HX         1       // HX file hydration
#define FLAG_SYM        2       // .SYM file hydration
#endif

    Classsym *STclasssym;       // if in the scope of this class
    symlist_t STclasslist;      // list of classes that have deferred inline
                                // functions to parse
    Classsym *STstag;           // returned by struct_searchmember() and with_search()
    SYMIDX STmarksi;            // to determine if temporaries are created
    char STnoparse;             // add to classlist instead of parsing
    char STdeferparse;          // defer member func parse
    enum SC STgclass;           // default function storage class
    int STdefertemps;           // defer allocation of temps
    int STdeferaccesscheck;     // defer access check for members (BUG: it
                                // never does get done later)
    int STnewtypeid;            // parsing new-type-id
    int STdefaultargumentexpression;    // parsing default argument expression
    block *STbtry;              // current try block
    block *STgotolist;          // threaded goto scoping list
    long STtdbtimestamp;        // timestamp of tdb file
    Symbol *STlastfunc;         // last function symbol parsed by ext_def()

    // For "point of definition" vs "point of instantiation" template name lookup
    unsigned STsequence;        // sequence number (Ssequence) of next Symbol
    unsigned STmaxsequence;     // won't find Symbols with STsequence larger
                                // than STmaxsequence
} Pstate;

extern Pstate pstate;

/****************************
 * Global variables.
 */

typedef struct Cstate
{
    struct BLKLST *CSfilblk;    // current source file we are parsing
    Symbol *CSlinkage;          // table of forward referenced linkage pragmas
    list_t CSlist_freelist;     // free list for list package
    symtab_t *CSpsymtab;        // pointer to current Symbol table
    char *modname;              // module unique identifier
} Cstate;

extern Cstate cstate;

/* Bits for sytab[] that give characteristics of storage classes        */
#define SCEXP   1       // valid inside expressions
#define SCKEP   2       // Symbol should be kept even when function is done
#define SCSCT   4       // storage class is valid for use in static ctor
#define SCSS    8       // storage class is on the stack
#define SCRD    0x10    // we can do reaching definitions on these

// Determine if Symbol has a Ssymnum associated with it.
// (That is, it is allocated on the stack or has live variable analysis
//  done on it, so it is stack and register variables.)
#define symbol_isintab(s)       (sytab[(s)->Sclass] & SCSS)

typedef enum SC enum_SC;


/******************************************
 * Basic blocks:
 *      Basic blocks are a linked list of all the basic blocks
 *      in a function. startblock heads the list.
 */

class ClassDeclaration;
class Declaration;
class Module;

struct Blockx
{
    block *startblock;
    block *curblock;
    Funcsym *funcsym;
    Symbol *context;            // eh frame context variable
    int scope_index;            // current scope index
    int next_index;             // value for next scope index
    unsigned flags;             // value to OR into Bflags
    block *tryblock;            // current enclosing try block
    elem *init;                 // static initializer
    ClassDeclaration *classdec;
    Declaration *member;        // member we're compiling for
    Module *module;             // module we're in
};

typedef struct block
{
    union
    {
        elem *Belem;            // pointer to elem tree
        list_t  Blist;          // list of expressions
    };

    block *Bnext;               // pointer to next block in list
    list_t        Bsucc;        // linked list of pointers to successors
                                //     of this block
    list_t        Bpred;        // and the predecessor list
    int Bindex;                 // into created object stack
    int Bendindex;              // index at end of block
    block *Btry;                // BCtry,BC_try: enclosing try block, if any
                                // BC???: if in try-block, points to BCtry or BC_try
                                // note that can't have a BCtry and BC_try in
                                // the same function.
    union
    {   targ_llong      *Bswitch;       // BCswitch: pointer to switch data
        struct {
            regm_t usIasmregs;          // Registers modified
            unsigned char  bIasmrefparam;   // References parameters?
            #define usIasmregs          BS.BIASM.usIasmregs
            #define bIasmrefparam       BS.BIASM.bIasmrefparam
        } BIASM;                        // BCasm

        struct
        {   Symbol *catchvar;           // __throw() fills in this
            #define catchvar BS.BITRY.catchvar
        } BITRY;                        // BCtry

        struct
        {   Symbol *catchtype;          // one type for each catch block
            #define Bcatchtype BS.BIJCATCH.catchtype

            unsigned *actionTable;      // EH_DWARF: indices into typeTable, first is # of entries
        } BIJCATCH;                     // BCjcatch


        struct
        {
            Symbol *jcatchvar;          // __d_throw() fills in this
            #define jcatchvar BS.BI_TRY.jcatchvar
            int Bscope_index;           // index into scope table
            #define Bscope_index BS.BI_TRY.Bscope_index
            int Blast_index;            // enclosing index into scope table
            #define Blast_index BS.BI_TRY.Blast_index
        } BI_TRY;                       // BC_try

        struct
        {
            Symbol *flag;               // EH_DWARF: set to 'flag' symbol that encloses finally
            block *b_ret;               // EH_DWARF: associated BC_ret block
        } BI_FINALLY;

    } BS;
    Srcpos      Bsrcpos;        // line number (0 if not known)
    unsigned char BC;           // exit condition (enum BC)

    unsigned char Balign;       // alignment

    unsigned short Bflags;              // flags (BFLxxxx)
        #define BFLvisited    1         // set if block is visited
        #define BFLmark       2         // set if block is visited
        #define BFLjmpoptdone 4         // set when no more jump optimizations
                                        //  are possible for this block
        #define BFLnostackopt 8         // set when stack elimination should not be done

        #define BFLnomerg      0x20     // do not merge with other blocks
        #define BFLprolog      0x80     // generate function prolog
        #define BFLepilog      0x100    // generate function epilog
        #define BFLrefparam    0x200    // referenced parameter
        #define BFLreflocal    0x400    // referenced local
        #define BFLoutsideprolog 0x800  // outside function prolog/epilog
        #define BFLlabel        0x2000  // block preceded by label
        #define BFLvolatile     0x4000  // block is volatile
    code        *Bcode;         // code generated for this block

    unsigned Bweight;           // relative number of times this block
                                // is executed (optimizer and codegen)

    unsigned    Bdfoidx;        // index of this block in dfo[]
    union
    {
        // CPP
        struct
        {
            SYMIDX      symstart;       // (symstart <= symnum < symend) Symbols
            SYMIDX      symend;         // are declared in this block
            block       *endscope;      // block that forms the end of the
                                        // scope for the declared Symbols
            unsigned blknum;            // position of block from startblock
            Symbol      *Binitvar;      // !=nullptr points to an auto variable with
                                        // an explicit or implicit initializer
            block       *gotolist;      // BCtry, BCcatch: backward list of try scopes
            block       *gotothread;    // BCgoto: threaded list of goto's to
                                        // unknown labels

            #define Bsymstart   _BLU._UP.symstart
            #define Bsymend     _BLU._UP.symend
            #define Bendscope   _BLU._UP.endscope
            #define Bblknum     _BLU._UP.blknum
            #define Binitvar    _BLU._UP.Binitvar
            #define Bgotolist   _BLU._UP.gotolist
            #define Bgotothread _BLU._UP.gotothread
        }_UP;

        // OPTIMIZER
        struct
        {
            vec_t       Bdom;           // mask of dominators for this block
            vec_t       Binrd;
            vec_t       Boutrd;         // IN and OUT for reaching definitions
            vec_t       Binlv;
            vec_t       Boutlv;         // IN and OUT for live variables
            vec_t       Bin;
            vec_t       Bout;           // IN and OUT for other flow analyses
            vec_t       Bgen;
            vec_t       Bkill;          // pointers to bit vectors used by data
                                        // flow analysis

            // BCiftrue can have different vectors for the 2nd successor:
            vec_t       Bout2;
            vec_t       Bgen2;
            vec_t       Bkill2;

            #define Bdom        _BLU._UO.Bdom
            #define Binrd       _BLU._UO.Binrd
            #define Boutrd      _BLU._UO.Boutrd
            #define Binlv       _BLU._UO.Binlv
            #define Boutlv      _BLU._UO.Boutlv
            #define Bin         _BLU._UO.Bin
            #define Bout        _BLU._UO.Bout
            #define Bgen        _BLU._UO.Bgen
            #define Bkill       _BLU._UO.Bkill
            #define Bout2       _BLU._UO.Bout2
            #define Bgen2       _BLU._UO.Bgen2
            #define Bkill2      _BLU._UO.Bkill2
        }_UO;

        // CODGEN
        struct
        {
            // For BCswitch, BCjmptab
            targ_size_t Btablesize;     // size of generated table
            targ_size_t Btableoffset;   // offset to start of table
            targ_size_t Btablebase;     // offset to instruction pointer base

            targ_size_t Boffset;        // code offset of start of this block
            targ_size_t Bsize;          // code size of this block
            con_t       Bregcon;        // register state at block exit
            targ_size_t Btryoff;        // BCtry: offset of try block data

            #define Btablesize          _BLU._UD.Btablesize
            #define Btableoffset        _BLU._UD.Btableoffset
            #define Btablebase          _BLU._UD.Btablebase
            #define Boffset             _BLU._UD.Boffset
            #define Bsize               _BLU._UD.Bsize
//          #define Bcode               _BLU._UD.Bcode
            #define Bregcon             _BLU._UD.Bregcon
            #define Btryoff             _BLU._UD.Btryoff
        } _UD;
    } _BLU;

    void appendSucc(block *b)  { list_append(&this->Bsucc, b); }
    void prependSucc(block *b) { list_prepend(&this->Bsucc, b); }
    int numSucc() { return list_nitems(this->Bsucc); }
    block *nthSucc(int n) { return (block *)list_ptr(list_nth(Bsucc, n)); }
    void setNthSucc(int n, block *b) { list_ptr(list_nth(Bsucc, n)) = b; }
} block;

#define list_block(l)   ((block *) list_ptr(l))

/** Basic block control flow operators. **/

enum BC {
    BCgoto      = 1,    // goto Bsucc block
    BCiftrue    = 2,    // if (Belem) goto Bsucc[0] else Bsucc[1]
    BCret       = 3,    // return (no return value)
    BCretexp    = 4,    // return with return value
    BCexit      = 5,    // never reaches end of block (like exit() was called)
    BCasm       = 6,    // inline assembler block (Belem is nullptr, Bcode
                        // contains code generated).
                        // These blocks have one or more successors in Bsucc,
                        // never 0
    BCswitch    = 7,    // switch statement
                        // Bswitch points to switch data
                        // Default is Bsucc
                        // Cases follow in linked list
    BCifthen    = 8,    // a BCswitch is converted to if-then
                        // statements
    BCjmptab    = 9,    // a BCswitch is converted to a jump
                        // table (switch value is index into
                        // the table)
    BCtry       = 10,   // C++ try block
                        // first block in a try-block. The first block in
                        // Bsucc is the next one to go to, subsequent
                        // blocks are the catch blocks
    BCcatch     = 11,   // C++ catch block
    BCjump      = 12,   // Belem specifies (near) address to jump to
    BC_try      = 13,   // SEH: first block of try-except or try-finally
                        // D: try-catch or try-finally
    BC_filter   = 14,   // SEH exception-filter (always exactly one block)
    BC_finally  = 15,   // first block of SEH termination-handler,
                        // or D finally block
    BC_ret      = 16,   // last block of SEH termination-handler or D _finally block
    BC_except   = 17,   // first block of SEH exception-handler
    BCjcatch    = 18,   // D catch block
    BC_lpad     = 19,   // EH_DWARF: landing pad for BC_except
    BCMAX
};

/**********************************
 * Functions
 */

typedef struct SYMTAB_S
{
    SYMIDX top;                 // 1 past end
    SYMIDX symmax;              // max # of entries in tab[] possible
    Symbol **tab;               // local Symbol table
} symtab_t;

typedef struct FUNC_S
{
    symlist_t Fsymtree;         // local Symbol table
    block *Fstartblock;         // list of blocks comprising function
    symtab_t Flocsym;           // local Symbol table
    Srcpos Fstartline;          // starting line # of function
    Srcpos Fendline;            // line # of closing brace of function
    Symbol *F__func__;          // symbol for __func__[] string
    unsigned Fflags;
#       define Fpending  1      // if function has been queued for being written
#       define Foutput   2      /* if function has been written out     */
#       define Finline   0x10   /* if SCinline, and function really is inline */
#       define Foverload 0x20   /* if function can be overloaded        */
#       define Ftypesafe 0x40   /* if function name needs type appended */
#       define Fmustoutput 0x80 /* set for forward ref'd functions that */
                                /* must be output                       */
#       define Finlinenest 0x1000 /* used as a marker to prevent nested */
                                /* inlines from expanding               */
#       define Flinkage 0x2000  /* linkage is already specified         */
#       define Fstatic  0x4000  /* static member function (no this)     */
#       define Foperator 4      /* if operator overload                 */
#       define Fcast     8      /* if cast overload                     */
#       define Fvirtual 0x100   /* if function is a virtual function    */
#       define Fctor    0x200   /* if function is a constructor         */
#       define Fdtor    0x400   /* if function is a destructor          */
#       define Fnotparent 0x800 /* if function is down Foversym chain   */
#       define Fbitcopy 0x8000  /* it's a simple bitcopy (op=() or X(X&)) */
#       define Fpure     0x10000        // pure function
#       define Finstance 0x20000        // function is an instance of a template
#       define Ffixed    0x40000        // ctor has had cpp_fixconstructor() run on it,
                                        // dtor has had cpp_fixdestructor()
#       define Fintro    0x80000        // function doesn't hide a previous virtual function
//#     define unused    0x100000       // unused bit
#       define Fkeeplink 0x200000       // don't change linkage to default
#       define Fnodebug  0x400000       // do not generate debug info for this function
#       define Fgen      0x800000       // compiler generated function
#       define Finvariant 0x1000000     // __invariant function
#       define Fexplicit  0x2000000     // explicit constructor
#       define Fsurrogate 0x4000000     // surrogate call function
    unsigned Fflags3;
        #define Fvtblgen        0x01    // generate vtbl[] when this function is defined
        #define Femptyexc       0x02    // empty exception specification (obsolete, use Tflags & TFemptyexc)
        #define Fcppeh          0x04    // uses C++ EH
        #define Fdeclared       0x10    // already declared function Symbol
        #define Fmark           0x20    // has unbalanced OPctor's
        #define Fnteh           0x08    // uses NT Structured EH
        #define Fdoinline       0x40    // do inline walk
        #define Foverridden     0x80    // ignore for overriding purposes
        #define Fjmonitor       0x100   // Mars synchronized function
        #define Fnosideeff      0x200   // function has no side effects
        #define F3badoparrow    0x400   // bad operator->()
        #define Fmain           0x800   // function is main() or wmain()
        #define Fnested         0x1000  // D nested function with 'this'
        #define Fmember         0x2000  // D member function with 'this'
        #define Fnotailrecursion 0x4000 // no tail recursion optimizations
        #define Ffakeeh         0x8000  // allocate space for NT EH context sym anyway
        #define Fnothrow        0x10000 // function does not throw (even if not marked 'nothrow')
    unsigned char Foper;        // operator number (OPxxxx) if Foperator

    Symbol *Fparsescope;        // use this scope to parse friend functions
                                // which are defined within a class, so the
                                // class is in scope, but are not members
                                // of the class

    Classsym *Fclass;           // if member of a class, this is the class
                                // (I think this is redundant with Sscope)
    Funcsym *Foversym;          // overloaded function at same scope
    symlist_t Fclassfriends;    /* Symbol list of classes of which this */
                                /* function is a friend                 */
    block *Fbaseblock;          // block where base initializers get attached
    block *Fbaseendblock;       // block where member destructors get attached
    elem *Fbaseinit;            /* list of member initializers (meminit_t) */
                                /* this field has meaning only for      */
                                /* functions which are constructors     */
    struct token_t *Fbody;      /* if deferred parse, this is the list  */
                                /* of tokens that make up the function  */
                                /* body                                 */
                                // also used if SCfunctempl, SCftexpspec
    unsigned Fsequence;         // sequence number at point of definition
    union
    {
        Symbol *Ftempl;         // if Finstance this is the template that generated it
        struct Thunk *Fthunk;   // !=nullptr if this function is actually a thunk
    };
    Funcsym *Falias;            // SCfuncalias: function Symbol referenced
                                // by using-declaration
    symlist_t Fthunks;          // list of thunks off of this function
    param_t *Farglist;          // SCfunctempl: the template-parameter-list
    param_t *Fptal;             // Finstance: this is the template-argument-list
                                // SCftexpspec: for explicit specialization, this
                                // is the template-argument-list
    list_t Ffwdrefinstances;    // SCfunctempl: list of forward referenced instances
    list_t Fexcspec;            // List of types in the exception-specification
                                // (nullptr if none or empty)
    Funcsym *Fexplicitspec;     // SCfunctempl, SCftexpspec: threaded list
                                // of SCftexpspec explicit specializations
    Funcsym *Fsurrogatesym;     // Fsurrogate: surrogate cast function

    char *Fredirect;            // redirect function name to this name in object

    // Array of catch types for EH_DWARF Types Table generation
    Symbol **typesTable;
    size_t typesTableDim;       // number used in typesTable[]
    size_t typesTableCapacity;  // allocated capacity of typesTable[]

#if ELFOBJ
    unsigned LSDAoffset;        // offset in LSDA segment of the LSDA data for this function
#endif
} func_t;

#define func_calloc()   ((func_t *) mem_fcalloc(sizeof(func_t)))
#define func_free(f)    mem_ffree(f)

/**************************
 * Item in list for member initializer.
 */

typedef struct MEMINIT
{
    list_t  MIelemlist;         /* arg list for initializer             */
    Symbol *MIsym;              /* if nullptr, then this initializer is    */
                                /* for the base class. Otherwise, this  */
                                /* is the member that needs the ctor    */
                                /* called for it                        */
} meminit_t;


/************************************
 * Base classes are a list of these.
 */

typedef struct BASECLASS
{
    Classsym         *BCbase;           // base class Symbol
    struct BASECLASS *BCnext;           // next base class
    targ_size_t       BCoffset;         // offset from start of derived class to this
    unsigned short    BCvbtbloff;       // for BCFvirtual, offset from start of
                                        //     vbtbl[] to entry for this virtual base.
                                        //     Valid in Sbase list
    symlist_t         BCpublics;        // public members of base class (list is freeable)
    list_t            BCmptrlist;       // (in Smptrbase only) this is the vtbl
                                        // (nullptr if not different from base class's vtbl
    Symbol           *BCvtbl;           // Symbol for vtbl[] array (in Smptrbase list)
                                        // Symbol for vbtbl[] array (in Svbptrbase list)
    unsigned          BCflags;          // base class flags
#define BCFpublic       1                   // base class is public
#define BCFprotected    2                   // base class is protected
#define BCFprivate      4                   // base class is private
#define BCFpmask        (BCFpublic | BCFprotected | BCFprivate)

#define BCFvirtual      8                   // base class is virtual
#define BCFvfirst       0x10                // virtual base class, and this is the
                                            // first virtual appearance of it
#define BCFnewvtbl      0x20                // new vtbl generated for this base class
#define BCFvirtprim     0x40                // Primary base class of a virtual base class
#define BCFdependent    0x80                // base class is a dependent type
    Classsym         *BCparent;         // immediate parent of this base class
                                        //     in Smptrbase
#if TX86
    struct BASECLASS *BCpbase;          // parent base, nullptr if did not come from a parent
#endif
} baseclass_t;

#define baseclass_malloc()      ((baseclass_t *)mem_fmalloc(sizeof(baseclass_t)))
#define baseclass_free(b)       ((void)(b))

/*************************
 * For virtual tables.
 */

typedef struct MPTR
{   targ_short     MPd;
    targ_short     MPi;
    Symbol        *MPf;
    Symbol        *MPparent;    // immediate parent of this base class
                                //   in Smptrbase
    unsigned char  MPflags;
#define MPTRvirtual     1       // it's an offset to a virtual base
#define MPTRcovariant   2       // need covariant return pointer adjustment
} mptr_t;

#define list_mptr(l)    ((mptr_t *) list_ptr(l))


/***********************************
 * Information gathered about externally defined template member functions,
 * member data, and member classes.
 */

struct TMF
{
    Classsym *stag;             // if !=nullptr, this is the enclosing class
    token_t *tbody;             // tokens making it up
    token_t *to;                // pointer within tbody where we left off in
                                // template_function_decl()
    param_t *temp_arglist;      // template parameter list
    int member_class;           // 1: it's a member class

    // These are for member templates
    int castoverload;           // 1: it's a user defined cast
    char *name;                 // name of template (nullptr if castoverload)
    int member_template;        // 0: regular template
                                // 1: member template
    param_t *temp_arglist2;     // if member template,
                                // then member's template parameter list

    param_t *ptal;              // if explicit specialization, this is the
                                // explicit template-argument-list
    Symbol *sclassfriend;       // if member function is a friend of class X,
                                // this is class X
    unsigned access_specifier;
};

/***********************************
 * Information gathered about primary member template explicit specialization.
 */

struct TME
{
    /* Given:
     *  template<> template<class T2> struct A<short>::B { };
     *  temp_arglist2 = <class T2>
     *  name = "B"
     *  ptal = <short>
     */
    param_t *ptal;              // explicit template-argument-list for enclosing
                                // template A
    symbol *stempl;             // template symbol for B
};

/***********************************
 * Information gathered about nested explicit specializations.
 */

struct TMNE
{
    /* For:
     *  template<> template<> struct A<short>::B<double> { };
     */

    enum_TK tk;                 // TKstruct / TKclass / TKunion
    char *name;                 // name of template, i.e. "B"
    param_t *ptal;              // explicit template-argument-list for enclosing
                                // template A, i.e. "short"
    token_t *tdecl;             // the tokens "<double> { }"
};

/***********************************
 * Information gathered about nested class friends.
 */

struct TMNF
{
    /* Given:
     *  template<class T> struct A { struct B { }; };
     *  class C { template<class T> friend struct A<T>::B;
     */
    token_t *tdecl;             // the tokens "A<T>::B;"
    param_t *temp_arglist;      // <class T>
    Classsym *stag;             // the class symbol C
    symbol *stempl;             // the template symbol A
};

/***********************************
 * Special information for class templates.
 */

typedef struct TEMPLATE
{
    symlist_t     TMinstances;  // list of Symbols that are instances
    param_t      *TMptpl;       // template-parameter-list
    struct token_t *TMbody;     // tokens making up class body
    unsigned TMsequence;        // sequence number at point of definition
    list_t TMmemberfuncs;       // templates for member functions (list of TMF's)
    list_t TMexplicit;          // list of TME's: primary member template explicit specializations
    list_t TMnestedexplicit;    // list of TMNE's: primary member template nested explicit specializations
    Symbol *TMnext;             // threaded list of template classes headed
                                // up by template_class_list
    enum_TK        TMtk;        // TKstruct, TKclass or TKunion
    int            TMflags;     // STRxxx flags

    symbol *TMprimary;          // primary class template
    symbol *TMpartial;          // next class template partial specialization
    param_t *TMptal;            // template-argument-list for partial specialization
                                // (nullptr for primary class template)
    list_t TMfriends;           // list of Classsym's for which any instantiated
                                // classes of this template will be friends of
    list_t TMnestedfriends;     // list of TMNF's
    int TMflags2;               // !=0 means dummy template created by template_createargtab()
} template_t;

/***********************************
 * Special information for enums.
 */

typedef struct ENUM
{
    unsigned SEflags;
#define SENnotagname    1       // no tag name for enum
#define SENforward      2       // forward referenced enum

    Symbol *SEalias;            // pointer to identifier E to use if
                                /* enum was defined as:                 */
                                /*      typedef enum { ... } E;         */
    symlist_t SEenumlist;       // all members of enum
} enum_t;

/***********************************
 * Special information for structs.
 */

typedef struct STRUCT
{
    targ_size_t Sstructsize;    // size of struct
    symlist_t Sfldlst;          // all members of struct (list freeable)
    Symbol *Sroot;              // root of binary tree Symbol table
    unsigned Salignsize;        // size of struct for alignment purposes
    unsigned char Sstructalign; // struct member alignment in effect
    unsigned long Sflags;
#define STRanonymous    0x01    // set for unions with no tag names
#define STRglobal       0x02    // defined at file scope
#define STRnotagname    0x04    // struct/class with no tag name
#define STRoutdef       0x08    // we've output the debug definition
#define STRbitfields    0x10    // set if struct contains bit fields
#define STRpredef       0x1000 // a predefined struct
#define STRunion        0x4000  // actually, it's a union

#define STRabstract     0x20    // abstract class
#define STRbitcopy      0x40    // set if operator=() is merely a bit copy
#define STRanyctor      0x80    // set if any constructors were defined
                                // by the user
#define STRnoctor       0x100   // no constructors allowed
#define STRgen          0x200   // if struct is an instantiation of a
                                // template class, and was generated by
                                // that template
#define STRvtblext      0x400   // generate vtbl[] only when first member function
                                // definition is encountered (see Fvtblgen)
#define STRexport       0x800   // all member functions are to be _export
#define STRclass        0x8000  // it's a class, not a struct
#if TX86
#define STRimport       0x40000 // imported class
#define STRstaticmems   0x80000 // class has static members
#endif
#define STR0size        0x100000        // zero sized struct
#define STRinstantiating 0x200000       // if currently being instantiated
#define STRexplicit     0x400000        // if explicit template instantiation
#define STRgenctor0     0x800000        // need to gen X::X()
#define STRnotpod       0x1000000       // struct is not POD
    tym_t ptrtype;              // type of pointer to refer to classes by
    unsigned short access;      // current access privilege, here so
                                // enum declarations can get at it
    targ_size_t Snonvirtsize;   // size of struct excluding virtual classes
    list_t Svirtual;            // freeable list of mptrs
                                // that go into vtbl[]
#if TX86
    list_t *Spvirtder;          // pointer into Svirtual that points to start
                                // of virtual functions for this (derived) class
    symlist_t Sopoverload;      // overloaded operator funcs (list freeable)
#endif
    symlist_t Scastoverload;    // overloaded cast funcs (list freeable)
    symlist_t Sclassfriends;    // list of classes of which this is a friend
                                // (list is freeable)
    symlist_t Sfriendclass;     // classes which are a friend to this class
                                // (list is freeable)
    symlist_t Sfriendfuncs;     // functions which are a friend to this class
                                // (list is freeable)
    symlist_t Sinlinefuncs;     // list of tokenized functions
    baseclass_t *Sbase;         // list of direct base classes
    baseclass_t *Svirtbase;     // list of all virtual base classes
    baseclass_t *Smptrbase;     // list of all base classes that have
                                // their own vtbl[]
    baseclass_t *Sprimary;      // if not nullptr, then points to primary
                                // base class
    Funcsym *Svecctor;          // constructor for use by vec_new()
    Funcsym *Sctor;             // constructor function

    Funcsym *Sdtor;             // basic destructor
    Funcsym *Sprimdtor;         // primary destructor
    Funcsym *Spriminv;          // primary invariant
    Funcsym *Sscaldeldtor;      // scalar deleting destructor

    Funcsym *Sinvariant;        // basic invariant function

    Symbol *Svptr;              // Symbol of vptr
    Symbol *Svtbl;              // Symbol of vtbl[]
    Symbol *Svbptr;             // Symbol of pointer to vbtbl[]
    Symbol *Svbptr_parent;      // base class for which Svbptr is a member.
                                // nullptr if Svbptr is a member of this class
    targ_size_t Svbptr_off;     // offset of Svbptr member
    Symbol *Svbtbl;             // virtual base offset table
    baseclass_t *Svbptrbase;    // list of all base classes in canonical
                                // order that have their own vbtbl[]
    Funcsym *Sopeq;             // X& X::operator =(X&)
    Funcsym *Sopeq2;            // Sopeq, but no copy of virtual bases
    Funcsym *Scpct;             // copy constructor
    Funcsym *Sveccpct;          // vector copy constructor
    Symbol *Salias;             // pointer to identifier S to use if
                                // struct was defined as:
                                //      typedef struct { ... } S;

    Symbol *Stempsym;           // if this struct is an instantiation
                                // of a template class, this is the
                                // template class Symbol

    // For 64 bit Elf function ABI
    type *Sarg1type;
    type *Sarg2type;

    /* For:
     *  template<class T> struct A { };
     *  template<class T> struct A<T *> { };
     *
     *  A<int> a;               // primary
     * Gives:
     *  Sarglist = <int>
     *  Spr_arglist = nullptr;
     *
     *  A<int*> a;              // specialization
     * Gives:
     *  Sarglist = <int>
     *  Spr_arglist = <int*>;
     */

    param_t *Sarglist;          // if this struct is an instantiation
                                // of a template class, this is the
                                // actual arg list used
    param_t *Spr_arglist;       // if this struct is an instantiation
                                // of a specialized template class, this is the
                                // actual primary arg list used.
                                // It is nullptr for the
                                // primary template class (since it would be
                                // identical to Sarglist).
} struct_t;

#define struct_calloc() ((struct_t *) mem_fcalloc(sizeof(struct_t)))
#define struct_free(st) ((void)(st))

/**********************************
 * Symbol Table
 */

#define list_symbol(l)          ((Symbol *) list_ptr(l))
#define list_setsymbol(l,s)     list_ptr(l) = (s)
#define list_Classsym(l)        ((Classsym *) list_ptr(l))

struct Symbol
{
#ifdef DEBUG
    unsigned short      id;
#define IDsymbol 0x5678
#define symbol_debug(s) assert((s)->id == IDsymbol)
#define class_debug(s) assert((s)->id == IDsymbol)
#else
#define symbol_debug(s)
#define class_debug(s)
#endif

    Symbol *Sl,*Sr;             // left, right child
    Symbol *Snext;              // next in threaded list
    dt_t *Sdt;                  // variables: initializer
    int Salignment;             // variables: alignment, 0 or -1 means default alignment
    int Salignsize();           // variables: return alignment
    type *Stype;                // type of Symbol
    #define ty() Stype->Tty

    union                       // variants for different Symbol types
    {
        enum_t *Senum_;         // SCenum
        #define Senum _SU.Senum_
        struct
        {    func_t *Sfunc_;    // tyfunc
             #define Sfunc _SU._SF.Sfunc_
             list_t Spath1_;    // SCfuncalias member functions: same as Spath
             #define Spath1 _SU._SF.Spath1_
                                // and in same position
                                // SCadl: list of associated functions for ADL lookup
        }_SF;
        struct                  // SClabel
        {   int Slabel_;                // TRUE if label was defined
             #define Slabel _SU._SL.Slabel_
            block *Slabelblk_;  // label block
             #define Slabelblk _SU._SL.Slabelblk_
        }_SL;
        #define Senumlist Senum->SEenumlist

        struct
        {
            char Sbit_;         // SCfield: bit position of start of bit field
             #define Sbit _SU._SB.Sbit_
            char Swidth_;       // SCfield: width in bits of bit field
             #define Swidth _SU._SB.Swidth_
            targ_size_t Smemoff_; // SCmember,SCfield: offset from start of struct
             #define Smemoff _SU._SB.Smemoff_
        }_SB;

        elem *Svalue_;          /* SFLvalue: value of const
                                   SFLdtorexp: for objects with destructor,
                                   conditional expression to precede dtor call
                                 */
        #define Svalue _SU.Svalue_

        struct_t *Sstruct_;     // SCstruct
        #define Sstruct _SU.Sstruct_
        template_t *Stemplate_; // SCtemplate
        #define Stemplate _SU.Stemplate_
        struct                  // SCfastpar, SCshadowreg
        {
            reg_t Spreg_;       // register parameter is passed in
            reg_t Spreg2_;      // if 2 registers, this is the most significant, else NOREG
        }_SR;
        #define Spreg _SU._SR.Spreg_
        #define Spreg2 _SU._SR.Spreg2_
    }_SU;

    regm_t Spregm();            // return mask of Spreg and Spreg2

    Symbol *Sscope;             // enclosing scope (could be struct tag,
                                // enclosing inline function for statics,
                                // or namespace)
#define isclassmember(s)        ((s)->Sscope && (s)->Sscope->Sclass == SCstruct)
    const char *prettyIdent;    // the symbol identifier as the user sees it

    enum_SC Sclass;             // storage class (SCxxxx)
    char Sfl;                   // flavor (FLxxxx)
    SYMFLGS Sflags;             // flag bits (SFLxxxx)
        #define SFLmark         0x08    // temporary marker
        #define SFLvalue        0x01    // Svalue contains const expression
        #define SFLimplem       0x02    // if seen implementation of Symbol
                                        // (function body for functions,
                                        // initializer for variables)
        #define SFLdouble       0x02    // SCregpar or SCparameter, where float
                                        // is really passed as a double
        #define SFLfree         0x04    // if we can symbol_free() a Symbol in
                                        // a Symbol table[]
        #define SFLexit         0x10    // tyfunc: function does not return
                                        // (ex: exit,abort,_assert,longjmp)
        #define SFLtrue         0x200   // value of Symbol != 0
        #define SFLreplace      SFLmark // variable gets replaced in inline expansion
        #define SFLskipinit     0x10000 // SCfield, SCmember: initializer is skipped
        #define SFLnodebug      0x20000 // don't generate debug info
        #define SFLwasstatic    0x800000 // was an uninitialized static
        #define SFLweak         0x1000000 // resolve to nullptr if not found
        #define SFLartifical    0x4000000 // compiler generated symbol

        // CPP
        #define SFLnodtor       0x10    // set if destructor for Symbol is already called
        #define SFLdtorexp      0x80    // Svalue has expression to tack onto dtor
        #define SFLmutable      0x100000        // SCmember or SCfield is mutable
        #define SFLdyninit      0x200000        // symbol has dynamic initializer
        #define SFLtmp          0x400000        // symbol is a generated temporary
        #define SFLthunk        0x40000 // symbol is temporary for thunk


        // Possible values for protection bits
        #define SFLprivate      0x60
        #define SFLprotected    0x40
        #define SFLpublic       0x20
        #define SFLnone         0x00
        #define SFLpmask        0x60    // mask for the protection bits
#if VEC_VTBL_LIST
        #define SFLvtbl         0x2000  // Symbol is a vtable or vbtable
#endif

        // OPTIMIZER and CODGEN
        #define GTregcand       0x100   // if Symbol is a register candidate
        #define SFLdead         0x800   // this variable is dead
        #define GTunregister    0x2000000       // 'unregister' a previous register assignment

        // OPTIMIZER only
        #define SFLunambig      0x400   // only accessible by unambiguous reference,
                                        // i.e. cannot be addressed via pointer
                                        // (GTregcand is a subset of this)
                                        // P.S. code generator turns off this
                                        // flag if any reads are done from it.
                                        // This is to eliminate stores to it
                                        // that are never read.
        #define SFLlivexit      0x1000  // live on exit from function
        #define SFLnotbasiciv   0x4000  // not a basic induction variable
        #define SFLnord         SFLdouble // SCauto,SCregister,SCtmp: disallow redundant warnings

        // CODGEN only
        #define GTtried         SFLmark // tried to place in register
        #define GTbyte          0x8000  // variable is sometimes accessed as
        #define SFLread         0x40000 // variable is actually read from
                                        // (to eliminate dead stores)
        #define SFLspill        0x80000 // only in register part of the time

    vec_t       Srange;         // live range, if any
    vec_t       Slvreg;         // when symbol is in register
    targ_size_t Ssize;          // tyfunc: size of function
    targ_size_t Soffset;        // variables: offset of Symbol in its storage class

    // CPP || OPTIMIZER
    SYMIDX Ssymnum;             // Symbol number (index into globsym.tab[])
                                // SCauto,SCparameter,SCtmp,SCregpar,SCregister
    // CODGEN
    int Sseg;                   // segment index
    int Sweight;                // usage count, the higher the number,
                                // the more worthwhile it is to put in
                                // a register
    int Sdw_ref_idx;            // !=0 means index of DW.ref.name symbol (Dwarf EH)

    union
    {
        unsigned Sxtrnnum_;     // SCcomdef,SCextern,SCcomdat: external symbol # (starting from 1)
        #define Sxtrnnum _SXR.Sxtrnnum_
        unsigned long Stypidx_; // SCstruct,SCunion,SCclass,SCenum,SCtypedef: debug info type index
        #define Stypidx _SXR.Stypidx_
        struct
        { unsigned char Sreglsw_;
          #define Sreglsw _SXR._SR.Sreglsw_
          unsigned char Sregmsw_;
          #define Sregmsw _SXR._SR.Sregmsw_
          regm_t Sregm_;        // mask of registers
          #define Sregm _SXR._SR.Sregm_
        }_SR;                   // SCregister,SCregpar,SCpseudo: register number
    }_SXR;
    regm_t      Sregsaved;      // mask of registers not affected by this func

    char Sident[1];             // identifier string (dynamic array)

    int needThis();             // !=0 if symbol needs a 'this' pointer
    bool Sisdead(bool anyiasm); // if variable is not referenced
};

// Class, struct or union

struct Classsym : Symbol { };

// Namespace Symbol
struct Nspacesym : Symbol { };

// Alias for another Symbol
struct Aliassym : Symbol { };

// Function symbol
//struct Funcsym : Symbol { };

// Determine if this Symbol is stored in a COMDAT

#define symbol_iscomdat(s)      ((s)->Sclass == SCcomdat ||             \
        config.flags2 & CFG2comdat && (s)->Sclass == SCinline ||        \
        config.flags4 & CFG4allcomdat && ((s)->Sclass == SCglobal))

/* Format the identifier for presentation to the user   */
inline char *prettyident(Symbol *s) { return s->Sident; }


/**********************************
 * Function parameters:
 *      Pident          identifier of parameter
 *      Ptype           type of argument
 *      Pelem           default value for argument
 *      Psym            symbol corresponding to Pident when using the
 *                      parameter list as a symbol table
 * For template-parameter-list:
 *      Pident          identifier of parameter
 *      Ptype           if nullptr, this is a type-parameter
 *                      else the type for a parameter-declaration value argument
 *      Pelem           default value for value argument
 *      Pdeftype        default value for type-parameter
 *      Pptpl           template-parameter-list for template-template-parameter
 *      Psym            default value for template-template-parameter
 * For template-arg-list: (actual arguments)
 *      Pident          nullptr
 *      Ptype           type-name
 *      Pelem           expression (either Ptype or Pelem is nullptr)
 *      Psym            SCtemplate for template-template-argument
 */

struct PARAM
{
#ifdef DEBUG
    unsigned short      id;
#define IDparam 0x7050
#define param_debug(s) assert((s)->id == IDparam)
#else
#define param_debug(s)
#endif

    char *Pident;               // identifier
    type *Ptype;                // type of parameter (nullptr if not known yet)
    elem *Pelem;                // default value
    token_t *PelemToken;        // tokens making up default elem
    type *Pdeftype;             // Ptype==nullptr: default type for type-argument
    param_t *Pptpl;             // template-parameter-list for template-template-parameter
    Symbol *Psym;
    PARAM *Pnext;               // next in list
    unsigned Pflags;
    #define PFexplicit  1       // this template argument was explicit, i.e. in < >

    PARAM *createTal(PARAM *);  // create template-argument-list blank from
                                // template-parameter-list
    PARAM *search(char *id);    // look for Pident matching id
    int searchn(char *id);      // look for Pident matching id, return index
    unsigned length();          // number of parameters in list
    void print();               // print this param_t
    void print_list();          // print this list of param_t's
};

/**************************************
 * Element types.
 * These should be combined with storage classes.
 */

enum FL
{
        // Change this, update debug.c too
        FLunde,
        FLconst,        // numerical constant
        FLoper,         // operator node
        FLfunc,         // function symbol
        FLdata,         // ref to data segment variable
        FLreg,          // ref to register variable
        FLpseudo,       // pseuodo register variable
        FLauto,         // ref to automatic variable
        FLfast,         // ref to variable passed as register
        FLpara,         // ref to function parameter variable
        FLextern,       // ref to external variable
        FLcode,         // offset to code
        FLblock,        // offset to block
        FLudata,        // ref to udata segment variable
        FLcs,           // ref to common subexpression number
        FLswitch,       // ref to offset of switch data block
        FLfltreg,       // ref to floating reg on stack, int contains offset
        FLoffset,       // offset (a variation on constant, needed so we
                        // can add offsets (different meaning for FLconst))
        FLdatseg,       // ref to data segment offset
        FLctor,         // constructed object
        FLdtor,         // destructed object
        FLregsave,      // ref to saved register on stack, int contains offset
        FLasm,          // (code) an ASM code

        FLndp,          // saved 8087 register

        // Segmented systems
        FLfardata,      // ref to far data segment
        FLcsdata,       // ref to code segment variable

        FLlocalsize,    // replaced with # of locals in the stack frame
        FLtlsdata,      // thread local storage
        FLbprel,        // ref to variable at fixed offset from frame pointer
        FLframehandler, // ref to C++ frame handler for NT EH
        FLblockoff,     // address of block
        FLallocatmp,    // temp for built-in alloca()
        FLstack,        // offset from ESP rather than EBP
        FLdsymbol,      // it's a Dsymbol

        // Global Offset Table
        FLgot,          // global offset table entry outside this object file
        FLgotoff,       // global offset table entry inside this object file

        FLfuncarg,      // argument to upcoming function call

        FLMAX
};

/**************************************************
 * This is to support compiling expressions within the context of a function.
 */

struct EEcontext
{
    unsigned EElinnum;          // line number to insert expression
    char *EEexpr;               // expression
    char *EEtypedef;            // typedef identifier
    char EEpending;             // !=0 means we haven't compiled it yet
    char EEimminent;            // we've installed it in the source text
    char EEcompile;             // we're compiling for the EE expression
    char EEin;                  // we are parsing an EE expression
    elem *EEelem;               // compiled version of EEexpr
    Symbol *EEfunc;             // function expression is in
    code *EEcode;               // generated code
};

extern EEcontext eecontext;

#include "rtlsym.hpp"

#undef SYMBOL_Z
#define SYMBOL_Z(e,fl,saved,n,flags,ty)         RTLSYM_##e,

enum
{
    RTLSYMS

    RTLSYM_MAX
};

extern Symbol *rtlsym[RTLSYM_MAX];

// Different goals for el_optimize()
typedef unsigned goal_t;
#define GOALnone        0       // evaluate for side effects only
#define GOALvalue       1       // evaluate for value
#define GOALflags       2       // evaluate for flags
#define GOALagain       4
#define GOALstruct      8
#define GOALhandle      0x10    // don't replace handle'd objects

/* Globals returned by declar() */
struct Declar
{
    Classsym *class_sym;
    Nspacesym *namespace_sym;
    int oper;
    bool constructor;
    bool destructor;
    bool invariant;
    param_t *ptal;
    bool explicitSpecialization;
    int hasExcSpec;             // has exception specification
};

extern Declar gdeclar;

/**********************************
 * Data definitions
 *      DTibytes        1..7 bytes
 *      DTabytes        offset of bytes of data
 *                      a { a data bytes }
 *      DTnbytes        bytes of data
 *                      a { a data bytes }
 *                      a = offset
 *      DTazeros        # of 0 bytes
 *                      a
 *      DTsymsize       same as DTazeros, but the type of the symbol gives
 *                      the size
 *      DTcommon        # of 0 bytes (in a common block)
 *                      a
 *      DTxoff          offset from symbol
 *                      w a
 *                      w = symbol number (pointer for CPP)
 *                      a = offset
 *      DTcoff          offset into code segment
 */

struct dt_t
{   dt_t *DTnext;                       // next in list
    char dt;                            // type (DTxxxx)
    unsigned char Dty;                  // pointer type
    unsigned char DTn;                  // DTibytes: number of bytes
    union
    {
        struct                          // DTibytes
        {
            #define DTibytesMax (sizeof(char *) + sizeof(unsigned) + sizeof(int) + sizeof(targ_size_t))
            char DTdata_[DTibytesMax];  // data
            #define DTdata _DU._DI.DTdata_
        }_DI;
        targ_size_t DTazeros_;          // DTazeros,DTcommon,DTsymsize
        #define DTazeros _DU.DTazeros_
        struct                          // DTabytes
        {
            char *DTpbytes_;            // pointer to the bytes
            #define DTpbytes _DU._DN.DTpbytes_
            unsigned DTnbytes_;         // # of bytes
            #define DTnbytes _DU._DN.DTnbytes_
            int DTseg_;                 // segment it went into
            #define DTseg _DU._DN.DTseg_
            targ_size_t DTabytes_;              // offset of abytes for DTabytes
            #define DTabytes _DU._DN.DTabytes_
        }_DN;
        struct                          // DTxoff
        {
            symbol *DTsym_;             // symbol pointer
            #define DTsym _DU._DS.DTsym_
            targ_size_t DToffset_;      // offset from symbol
            #define DToffset _DU._DS.DToffset_
        }_DS;
    }_DU;
};

enum
{
    DT_abytes,
    DT_azeros,  // 1
    DT_xoff,
    DT_nbytes,
    DT_common,
    DT_coff,
    DT_ibytes, // 6
};

// An efficient way to clear aligned memory
#define MEMCLEAR(p,sz)                  \
    if ((sz) == 10 * sizeof(size_t))    \
    {                                   \
        ((size_t *)(p))[0] = 0;         \
        ((size_t *)(p))[1] = 0;         \
        ((size_t *)(p))[2] = 0;         \
        ((size_t *)(p))[3] = 0;         \
        ((size_t *)(p))[4] = 0;         \
        ((size_t *)(p))[5] = 0;         \
        ((size_t *)(p))[6] = 0;         \
        ((size_t *)(p))[7] = 0;         \
        ((size_t *)(p))[8] = 0;         \
        ((size_t *)(p))[9] = 0;         \
    }                                   \
    else if ((sz) == 14 * sizeof(size_t))       \
    {                                   \
        ((size_t *)(p))[0] = 0;         \
        ((size_t *)(p))[1] = 0;         \
        ((size_t *)(p))[2] = 0;         \
        ((size_t *)(p))[3] = 0;         \
        ((size_t *)(p))[4] = 0;         \
        ((size_t *)(p))[5] = 0;         \
        ((size_t *)(p))[6] = 0;         \
        ((size_t *)(p))[7] = 0;         \
        ((size_t *)(p))[8] = 0;         \
        ((size_t *)(p))[9] = 0;         \
        ((size_t *)(p))[10] = 0;        \
        ((size_t *)(p))[11] = 0;        \
        ((size_t *)(p))[12] = 0;        \
        ((size_t *)(p))[13] = 0;        \
    }                                   \
    else                                \
    {                                   \
        /*printf("%s(%d) sz = %d\n",__FILE__,__LINE__,(sz));fflush(stdout);*(char*)0=0;*/  \
        for (size_t i = 0; i < sz / sizeof(size_t); ++i)        \
            ((size_t *)(p))[i] = 0;                             \
    }
