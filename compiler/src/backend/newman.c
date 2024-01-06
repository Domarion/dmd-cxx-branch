// Copyright (C) 1992-1998 by Symantec
// Copyright (C) 2000-2021 by The D Language Foundation, All Rights Reserved
// http://www.digitalmars.com
// Written by Walter Bright
/*
 * This source file is made available for personal use
 * only. The license is in backendlicense.txt
 * For any other uses, please contact Digital Mars.
 */

#if !SPP

#include        <stdio.h>
#include        <ctype.h>
#include        <string.h>
#include        <stdlib.h>
#include        <time.h>

#include        "cc.h"
#include        "token.h"
#include        "global.h"
#include        "oper.h"
#include        "el.h"
#include        "type.h"
#include        "filespec.h"

static char __file__[] = __FILE__;      /* for tassert.h                */
#include        "tassert.h"

#define BUFIDMAX (2 * IDMAX)

struct Mangle
{
    char buf[BUFIDMAX + 2];

    char *np;                   // index into buf[]

    // Used for compression of redundant znames
    const char *zname[10];
    int znamei;

    type *arg[10];              // argument_replicator
    int argi;                   // number used in arg[]
};

static Mangle mangle;

static int mangle_inuse;

struct MangleInuse
{
    MangleInuse()
    {
#if 0
        assert(mangle_inuse == 0);
        mangle_inuse++;
#endif
    }

    ~MangleInuse()
    {
#if 0
        assert(mangle_inuse == 1);
        mangle_inuse--;
#endif
    }
};

/* Names for special variables  */
char cpp_name_new[]     = "?2";
char cpp_name_delete[]  = "?3";
char cpp_name_anew[]    = "?_P";
char cpp_name_adelete[] = "?_Q";
char cpp_name_ct[]      = "?0";
char cpp_name_dt[]      = "?1";
char cpp_name_as[]      = "?4";
char cpp_name_vc[]      = "?_H";
char cpp_name_primdt[]  = "?_D";
char cpp_name_scaldeldt[] = "?_G";
char cpp_name_priminv[] = "?_R";

STATIC int cpp_cvidx ( tym_t ty );
STATIC int cpp_protection ( symbol *s );
STATIC void cpp_decorated_name ( symbol *s );
STATIC void cpp_symbol_name ( symbol *s );
STATIC void cpp_zname ( const char *p );
STATIC void cpp_scope ( symbol *s );
STATIC void cpp_type_encoding ( symbol *s );
STATIC void cpp_external_function_type(symbol *s);
STATIC void cpp_external_data_type ( symbol *s );
STATIC void cpp_member_function_type ( symbol *s );
STATIC void cpp_static_member_function_type ( symbol *s );
STATIC void cpp_static_member_data_type ( symbol *s );
STATIC void cpp_local_static_data_type ( symbol *s );
STATIC void cpp_vftable_type(symbol *s);
STATIC void cpp_adjustor_thunk_type(symbol *s);
STATIC void cpp_function_type ( type *t );
STATIC void cpp_throw_types ( type *t );
STATIC void cpp_ecsu_name ( symbol *s );
STATIC void cpp_return_type ( symbol *s );
STATIC void cpp_data_type ( type *t );
STATIC void cpp_storage_convention ( symbol *s );
STATIC void cpp_this_type ( type *t,Classsym *s );
STATIC void cpp_vcall_model_type ( void );
STATIC void cpp_calling_convention ( type *t );
STATIC void cpp_argument_types ( type *t );
STATIC void cpp_argument_list ( type *t, int flag );
STATIC void cpp_primary_data_type ( type *t );
STATIC void cpp_reference_type ( type *t );
STATIC void cpp_pointer_type ( type *t );
STATIC void cpp_ecsu_data_indirect_type ( type *t );
STATIC void cpp_data_indirect_type ( type *t );
STATIC void cpp_function_indirect_type ( type *t );
STATIC void cpp_basic_data_type ( type *t );
STATIC void cpp_ecsu_data_type(type *t);
STATIC void cpp_pointer_data_type ( type *t );
STATIC void cpp_reference_data_type ( type *t, int flag );
STATIC void cpp_enum_name ( symbol *s );
STATIC void cpp_dimension ( targ_ullong u );
STATIC void cpp_dimension_ld ( targ_ldouble ld );
STATIC void cpp_string ( char *s, size_t len );

/****************************
 */

struct OPTABLE
{
    unsigned char tokn;
    unsigned char oper;
    char *string;
    char *pretty;
}

 oparray[] = {
    {   TKnew, OPnew,           cpp_name_new,   "new" },
    {   TKdelete, OPdelete,     cpp_name_delete,"del" },
    {   TKadd, OPadd,           "?H",           "+" },
    {   TKadd, OPuadd,          "?H",           "+" },
    {   TKmin, OPmin,           "?G",           "-" },
    {   TKmin, OPneg,           "?G",           "-" },
    {   TKstar, OPmul,          "?D",           "*" },
    {   TKstar, OPind,          "?D",           "*" },
    {   TKdiv, OPdiv,           "?K",           "/" },
    {   TKmod, OPmod,           "?L",           "%" },
    {   TKxor, OPxor,           "?T",           "^" },
    {   TKand, OPand,           "?I",           "&" },
    {   TKand, OPaddr,          "?I",           "&" },
    {   TKor, OPor,             "?U",           "|" },
    {   TKcom, OPcom,           "?S",           "~" },
    {   TKnot, OPnot,           "?7",           "!" },
    {   TKeq, OPeq,             cpp_name_as,    "=" },
    {   TKeq, OPstreq,          "?4",           "=" },
    {   TKlt, OPlt,             "?M",           "<" },
    {   TKgt, OPgt,             "?O",           ">" },
    {   TKnew, OPanew,          cpp_name_anew,  "n[]" },
    {   TKdelete, OPadelete,    cpp_name_adelete,"d[]" },
    {   TKunord, OPunord,       "?_S",          "!<>=" },
    {   TKlg, OPlg,             "?_T",          "<>"   },
    {   TKleg, OPleg,           "?_U",          "<>="  },
    {   TKule, OPule,           "?_V",          "!>"   },
    {   TKul, OPul,             "?_W",          "!>="  },
    {   TKuge, OPuge,           "?_X",          "!<"   },
    {   TKug, OPug,             "?_Y",          "!<="  },
    {   TKue, OPue,             "?_Z",          "!<>"  },
    {   TKaddass, OPaddass,     "?Y",           "+=" },
    {   TKminass, OPminass,     "?Z",           "-=" },
    {   TKmulass, OPmulass,     "?X",           "*=" },
    {   TKdivass, OPdivass,     "?_0",          "/=" },
    {   TKmodass, OPmodass,     "?_1",          "%=" },
    {   TKxorass, OPxorass,     "?_6",          "^=" },
    {   TKandass, OPandass,     "?_4",          "&=" },
    {   TKorass, OPorass,       "?_5",          "|=" },
    {   TKshl, OPshl,           "?6",           "<<" },
    {   TKshr, OPshr,           "?5",           ">>" },
    {   TKshrass, OPshrass,     "?_2",          ">>=" },
    {   TKshlass, OPshlass,     "?_3",          "<<=" },
    {   TKeqeq, OPeqeq,         "?8",           "==" },
    {   TKne, OPne,             "?9",           "!=" },
    {   TKle, OPle,             "?N",           "<=" },
    {   TKge, OPge,             "?P",           ">=" },
    {   TKandand, OPandand,     "?V",           "&&" },
    {   TKoror, OPoror,         "?W",           "||" },
    {   TKplpl, OPpostinc,      "?E",           "++" },
    {   TKplpl, OPpreinc,       "?E",           "++" },
    {   TKmimi, OPpostdec,      "?F",           "--" },
    {   TKmimi, OPpredec,       "?F",           "--" },
    {   TKlpar, OPcall,         "?R",           "()" },
    {   TKlbra, OPbrack,        "?A",           "[]" },
    {   TKarrow, OParrow,       "?C",           "->" },
    {   TKcomma, OPcomma,       "?Q",           "," },
    {   TKarrowstar, OParrowstar, "?J",         "->*" },
};

/***********************************
 * Generate and return a pointer to a string constructed from
 * the type, appended to the prefix.
 * Since these generated strings determine the uniqueness of names,
 * they are also used to determine if two types are the same.
 * Returns:
 *      pointer to static name[]
 */

char *cpp_typetostring(type *t,char *prefix)
{   int i;

    if (prefix)
    {   strcpy(mangle.buf,prefix);
        i = strlen(prefix);
    }
    else
        i = 0;
    //dbg_printf("cpp_typetostring:\n");
    //type_print(t);
    MangleInuse m;
    mangle.znamei = 0;
    mangle.argi = 0;
    mangle.np = mangle.buf + i;
    mangle.buf[BUFIDMAX + 1] = 0x55;
    cpp_data_type(t);
    *mangle.np = 0;                     // 0-terminate mangle.buf[]
    //dbg_printf("cpp_typetostring: '%s'\n", mangle.buf);
    assert(strlen(mangle.buf) <= BUFIDMAX);
    assert(mangle.buf[BUFIDMAX + 1] == 0x55);
    return mangle.buf;
}

/********************************
 * 'Mangle' a name for output.
 * Returns:
 *      pointer to mangled name (a static buffer)
 */

char *cpp_mangle(symbol *s)
{
    symbol_debug(s);
    //printf("cpp_mangle(s = %p, '%s')\n", s, s->Sident);
    //type_print(s->Stype);

    if (type_mangle(s->Stype) != mTYman_cpp)
        return symbol_ident(s);
    else
    {
        MangleInuse m;

        mangle.znamei = 0;
        mangle.argi = 0;
        mangle.np = mangle.buf;
        mangle.buf[BUFIDMAX + 1] = 0x55;
        cpp_decorated_name(s);
        *mangle.np = 0;                 // 0-terminate cpp_name[]
        //dbg_printf("cpp_mangle() = '%s'\n", mangle.buf);
        assert(strlen(mangle.buf) <= BUFIDMAX);
        assert(mangle.buf[BUFIDMAX + 1] == 0x55);
        return mangle.buf;
    }
}

///////////////////////////////////////////////////////

/*********************************
 * Add char into cpp_name[].
 */

STATIC void __inline CHAR(char c)
{
    if (mangle.np < &mangle.buf[BUFIDMAX])
        *mangle.np++ = c;
}

/*********************************
 * Add char into cpp_name[].
 */

STATIC void STR(const char *p)
{
    size_t len;

    len = strlen(p);
    if (mangle.np + len <= &mangle.buf[BUFIDMAX])
    {   memcpy(mangle.np,p,len);
        mangle.np += len;
    }
    else
        for (; *p; p++)
            CHAR(*p);
}

/***********************************
 * Convert const volatile combinations into 0..3
 */

STATIC int cpp_cvidx(tym_t ty)
{   int i;

    i  = (ty & mTYconst) ? 1 : 0;
    i |= (ty & mTYvolatile) ? 2 : 0;
    return i;
}

/******************************
 * Turn protection into 0..2
 */

STATIC int cpp_protection(symbol *s)
{   int i;

    switch (s->Sflags & SFLpmask)
    {   case SFLprivate:        i = 0;  break;
        case SFLprotected:      i = 1;  break;
        case SFLpublic:         i = 2;  break;
        default:
            symbol_print(s);
            assert(0);
    }
    return i;
}

//////////////////////////////////////////////////////
// Functions corresponding to the name mangling grammar in the
// "Microsoft Object Mapping Specification"

STATIC void cpp_string(char *s,size_t len)
{   char c;

    for (; --len; s++)
    {   static char special_char[] = ",/\\:. \n\t'-";
        char *p;

        c = *s;
        if (c & 0x80 && isalpha(c & 0x7F))
        {   CHAR('?');
            c &= 0x7F;
        }
        else if (isalnum(c))
            ;
        else
        {
            CHAR('?');
            if ((p = (char *)strchr(special_char,c)) != NULL)
                c = '0' + (p - special_char);
            else
            {
                CHAR('$');
                CHAR('A' + ((c >> 4) & 0x0F));
                c = 'A' + (c & 0x0F);
            }
        }
        CHAR(c);
    }
    CHAR('@');
}

STATIC void cpp_dimension(targ_ullong u)
{
    if (u && u <= 10)
        CHAR('0' + (char)u - 1);
    else
    {   char buffer[sizeof(u) * 2 + 1];
        char *p;

        buffer[sizeof(buffer) - 1] = 0;
        for (p = &buffer[sizeof(buffer) - 1]; u; u >>= 4)
        {
            *--p = 'A' + (u & 0x0F);
        }
        STR(p);
        CHAR('@');
    }
}

#if 0
STATIC void cpp_dimension_ld(targ_ldouble ld)
{   unsigned char ldbuf[sizeof(targ_ldouble)];

    memcpy(ldbuf,&ld,sizeof(ld));
    if (u && u <= 10)
        CHAR('0' + (char)u - 1);
    else
    {   char buffer[sizeof(u) * 2 + 1];
        char *p;

        buffer[sizeof(buffer) - 1] = 0;
        for (p = &buffer[sizeof(buffer) - 1]; u; u >>= 4)
        {
            *--p = 'A' + (u & 0x0F);
        }
        STR(p);
        CHAR('@');
    }
}
#endif

STATIC void cpp_enum_name(symbol *s)
{   type *t;
    char c;

    t = tsint;
    switch (tybasic(t->Tty))
    {
        case TYschar:   c = '0';        break;
        case TYuchar:   c = '1';        break;
        case TYshort:   c = '2';        break;
        case TYushort:  c = '3';        break;
        case TYint:     c = '4';        break;
        case TYuint:    c = '5';        break;
        case TYlong:    c = '6';        break;
        case TYulong:   c = '7';        break;
        default:        assert(0);
    }
    CHAR(c);
    cpp_ecsu_name(s);
}

STATIC void cpp_reference_data_type(type *t, int flag)
{
    if (tybasic(t->Tty) == TYarray)
    {
        int ndim;
        type *tn;
        int i;

        CHAR('Y');

        // Compute number of dimensions (we have at least one)
        ndim = 0;
        tn = t;
        do
        {   ndim++;
            tn = tn->Tnext;
        } while (tybasic(tn->Tty) == TYarray);

        cpp_dimension(ndim);
        for (; tybasic(t->Tty) == TYarray; t = t->Tnext)
        {
            if (t->Tflags & TFvla)
                CHAR('X');                      // DMC++ extension
            else
                cpp_dimension(t->Tdim);
        }

        // DMC++ extension
        if (flag)                       // if template type argument
        {
            i = cpp_cvidx(t->Tty);
            if (i)
            {   CHAR('_');
                //CHAR('X' + i - 1);            // _X, _Y, _Z
                CHAR('O' + i - 1);              // _O, _P, _Q
            }
        }

        cpp_basic_data_type(t);
    }
    else
        cpp_basic_data_type(t);
}

STATIC void cpp_pointer_data_type(type *t)
{
    if (tybasic(t->Tty) == TYvoid)
        CHAR('X');
    else
        cpp_reference_data_type(t, 0);
}

STATIC void cpp_ecsu_data_type(type *t)
{   char c;
    symbol *stag;
    int i;

    type_debug(t);
    switch (tybasic(t->Tty))
    {
        case TYstruct:
            stag = t->Ttag;
            switch (stag->Sstruct->Sflags & (STRclass | STRunion))
            {   case 0:         c = 'U';        break;
                case STRunion:  c = 'T';        break;
                case STRclass:  c = 'V';        break;
                default:
                    assert(0);
            }
            CHAR(c);
            cpp_ecsu_name(stag);
            break;
        case TYenum:
            CHAR('W');
            cpp_enum_name(t->Ttag);
            break;
        default:
#ifdef DEBUG
            type_print(t);
#endif
            assert(0);
    }
}

STATIC void cpp_basic_data_type(type *t)
{   char c;
    int i;

    //printf("cpp_basic_data_type(t)\n");
    //type_print(t);
    switch (tybasic(t->Tty))
    {
        case TYschar:   c = 'C';        goto dochar;
        case TYchar:    c = 'D';        goto dochar;
        case TYuchar:   c = 'E';        goto dochar;
        case TYshort:   c = 'F';        goto dochar;
        case TYushort:  c = 'G';        goto dochar;
        case TYint:     c = 'H';        goto dochar;
        case TYuint:    c = 'I';        goto dochar;
        case TYlong:    c = 'J';        goto dochar;
        case TYulong:   c = 'K';        goto dochar;
        case TYfloat:   c = 'M';        goto dochar;
        case TYdouble:  c = 'N';        goto dochar;

        case TYdouble_alias:
                        if (intsize == 4)
                        {   c = 'O';
                            goto dochar;
                        }
                        c = 'Z';
                        goto dochar2;

        case TYldouble:
                        if (intsize == 2)
                        {   c = 'O';
                            goto dochar;
                        }
                        c = 'Z';
                        goto dochar2;
        dochar:
            CHAR(c);
            break;

        case TYllong:   c = 'J';        goto dochar2;
        case TYullong:  c = 'K';        goto dochar2;
        case TYbool:    c = 'N';        goto dochar2;   // was 'X' prior to 8.1b8
        case TYwchar_t:
            if (config.flags4 & CFG4nowchar_t)
            {
                c = 'G';
                goto dochar;    // same as TYushort
            }
            else
            {
                pstate.STflags |= PFLmfc;
                c = 'Y';
                goto dochar2;
            }

        // Digital Mars extensions
        case TYifloat:  c = 'R';        goto dochar2;
        case TYidouble: c = 'S';        goto dochar2;
        case TYildouble: c = 'T';       goto dochar2;
        case TYcfloat:  c = 'U';        goto dochar2;
        case TYcdouble: c = 'V';        goto dochar2;
        case TYcldouble: c = 'W';       goto dochar2;

        case TYchar16:   c = 'X';       goto dochar2;
        case TYdchar:    c = 'Y';       goto dochar2;
        case TYnullptr:  c = 'Z';       goto dochar2;

        dochar2:
            CHAR('_');
            goto dochar;

        case TYsptr:
        case TYcptr:
        case TYf16ptr:
        case TYfptr:
        case TYhptr:
        case TYvptr:
        case TYnptr:
            c = 'P' + cpp_cvidx(t->Tty);
            CHAR(c);
            if(I64)
                CHAR('E'); // __ptr64 modifier
            cpp_pointer_type(t);
            break;
        case TYstruct:
        case TYenum:
            cpp_ecsu_data_type(t);
            break;
        case TYarray:
            i = cpp_cvidx(t->Tty);
            i |= 1;                     // always const
            CHAR('P' + i);
            cpp_pointer_type(t);
            break;
        case TYvoid:
            c = 'X';
            goto dochar;

        default:
        Ldefault:
            if (tyfunc(t->Tty))
                cpp_function_type(t);
            else
            {
            }
    }
}

STATIC void cpp_function_indirect_type(type *t)
{   int farfunc;

    farfunc = tyfarfunc(t->Tnext->Tty) != 0;
        CHAR('6' + farfunc);
}

STATIC void cpp_data_indirect_type(type *t)
{   int i;
        cpp_ecsu_data_indirect_type(t);
}

STATIC void cpp_ecsu_data_indirect_type(type *t)
{   int i;
    tym_t ty;

    i = 0;
    if (t->Tnext)
    {   ty = t->Tnext->Tty & (mTYconst | mTYvolatile);
        switch (tybasic(t->Tty))
        {
            case TYfptr:
            case TYvptr:
            case TYfref:
                ty |= mTYfar;
                break;

            case TYhptr:
                i += 8;
                break;
            case TYref:
            case TYarray:
                if (LARGEDATA && !(ty & mTYLINK))
                    ty |= mTYfar;
                break;
        }
    }
    else
        ty = t->Tty & (mTYLINK | mTYconst | mTYvolatile);
    i |= cpp_cvidx(ty);
    if (ty & (mTYcs | mTYfar))
        i += 4;
    CHAR('A' + i);
}

STATIC void cpp_pointer_type(type *t)
{   tym_t ty;

    if (tyfunc(t->Tnext->Tty))
    {
        cpp_function_indirect_type(t);
        cpp_function_type(t->Tnext);
    }
    else
    {
        cpp_data_indirect_type(t);
        cpp_pointer_data_type(t->Tnext);
    }
}

STATIC void cpp_reference_type(type *t)
{
    cpp_data_indirect_type(t);
    cpp_reference_data_type(t->Tnext, 0);
}

STATIC void cpp_primary_data_type(type *t)
{
    if (tyref(t->Tty))
    {
#if 1
        // C++98 8.3.2 says cv-qualified references are ignored
        CHAR('A');
#else
        switch (t->Tty & (mTYconst | mTYvolatile))
        {
            case 0:                      CHAR('A');     break;
            case mTYvolatile:            CHAR('B');     break;

            // Digital Mars extensions
            case mTYconst | mTYvolatile: CHAR('_'); CHAR('L');  break;
            case mTYconst:               CHAR('_'); CHAR('M');  break;
        }
#endif
        cpp_reference_type(t);
    }
    else
        cpp_basic_data_type(t);
}

/*****
 * flag: 1 = template argument
 */

STATIC void cpp_argument_list(type *t, int flag)
{   int i;
    tym_t ty;

    //printf("cpp_argument_list(flag = %d)\n", flag);
    // If a data type that encodes only into one character
    ty = tybasic(t->Tty);
    if (ty <= TYldouble && ty != TYenum
        && ty != TYbool         // added for versions >= 8.1b9
        && !(t->Tty & (mTYconst | mTYvolatile))
       )
    {
        cpp_primary_data_type(t);
    }
    else
    {
        // See if a match with a previously used type
        for (i = 0; 1; i++)
        {
            if (i == mangle.argi)               // no match
            {
                if (ty <= TYcldouble || ty == TYstruct)
                {
                    int cvidx = cpp_cvidx(t->Tty);
                    if (cvidx)
                    {
                        // Digital Mars extensions
                        CHAR('_');
                        CHAR('N' + cvidx);      // _O, _P, _Q prefix
                    }
                }
                if (flag && tybasic(t->Tty) == TYarray)
                {
                   cpp_reference_data_type(t, flag);
                }
                else
                    cpp_primary_data_type(t);
                if (mangle.argi < 10)
                    mangle.arg[mangle.argi++] = t;
                break;
            }
            if (typematch(t,mangle.arg[i],0))
            {
                CHAR('0' + i);          // argument_replicator
                break;
            }
        }
    }
}

STATIC void cpp_argument_types(type *t)
{   param_t *p;
    char c;

    //printf("cpp_argument_types()\n");
    //type_debug(t);
    for (p = t->Tparamtypes; p; p = p->Pnext)
        cpp_argument_list(p->Ptype, 0);
    if (t->Tflags & TFfixed)
        c = t->Tparamtypes ? '@' : 'X';
    else
        c = 'Z';
    CHAR(c);
}

STATIC void cpp_calling_convention(type *t)
{   char c;

    switch (tybasic(t->Tty))
    {
        case TYnfunc:
        case TYhfunc:
        case TYffunc:
            c = 'A';        break;
        case TYf16func:
        case TYfpfunc:
        case TYnpfunc:
            c = 'C';        break;
        case TYnsfunc:
        case TYfsfunc:
            c = 'G';        break;
        case TYjfunc:
        case TYmfunc:
        case TYnsysfunc:
        case TYfsysfunc:
            c = 'E';       break;
        case TYifunc:
            c = 'K';        break;
        default:
            assert(0);
    }
    CHAR(c);
}

STATIC void cpp_vcall_model_type()
{
}


STATIC void cpp_this_type(type *tfunc,Classsym *stag)
{   type *t;

    type_debug(tfunc);
    symbol_debug(stag);
    t = type_pointer(stag->Stype);

    //cpp_data_indirect_type(t);
    cpp_ecsu_data_indirect_type(t);
    type_free(t);
}



STATIC void cpp_storage_convention(symbol *s)
{   tym_t ty;
    type *t = s->Stype;

    ty = t->Tty;
    if (LARGEDATA && !(ty & mTYLINK))
        t->Tty |= mTYfar;
    cpp_data_indirect_type(t);
    t->Tty = ty;
}

STATIC void cpp_data_type(type *t)
{
    type_debug(t);
    switch (tybasic(t->Tty))
    {   case TYvoid:
            CHAR('X');
            break;
        case TYstruct:
        case TYenum:
            CHAR('?');
            cpp_ecsu_data_indirect_type(t);
            cpp_ecsu_data_type(t);
            break;
        default:
            cpp_primary_data_type(t);
            break;
    }
}

STATIC void cpp_return_type(symbol *s)
{
    if (s->Sfunc->Fflags & (Fctor | Fdtor))     // if ctor or dtor
        CHAR('@');                              // no type
    else
        cpp_data_type(s->Stype->Tnext);
}

STATIC void cpp_ecsu_name(symbol *s)
{
    //printf("cpp_ecsu_name(%s)\n", symbol_ident(s));
    cpp_zname(symbol_ident(s));
    if (s->Sscope)
        cpp_scope(s->Sscope);

    CHAR('@');
}

STATIC void cpp_throw_types(type *t)
{
    //cpp_argument_types(?);
    CHAR('Z');
}

STATIC void cpp_function_type(type *t)
{   tym_t ty;
    type *tn;

    //printf("cpp_function_type()\n");
    //type_debug(t);
    assert(tyfunc(t->Tty));
    cpp_calling_convention(t);
    //cpp_return_type(s);
    tn = t->Tnext;
    ty = tn->Tty;
    if (LARGEDATA && (tybasic(ty) == TYstruct || tybasic(ty) == TYenum) &&
        !(ty & mTYLINK))
        tn->Tty |= mTYfar;
    cpp_data_type(tn);
    tn->Tty = ty;
    cpp_argument_types(t);
    cpp_throw_types(t);
}

STATIC void cpp_adjustor_thunk_type(symbol *s)
{
}

STATIC void cpp_vftable_type(symbol *s)
{
    cpp_ecsu_data_indirect_type(s->Stype);
//      vpath_name();
    CHAR('@');
}

STATIC void cpp_local_static_data_type(symbol *s)
{
    //cpp_lexical_frame(?);
    cpp_external_data_type(s);
}

STATIC void cpp_static_member_data_type(symbol *s)
{
    cpp_external_data_type(s);
}

STATIC void cpp_static_member_function_type(symbol *s)
{
    cpp_function_type(s->Stype);
}

STATIC void cpp_member_function_type(symbol *s)
{
    assert(tyfunc(s->Stype->Tty));
    cpp_this_type(s->Stype,(Classsym *)s->Sscope);
    if (s->Sfunc->Fflags & (Fctor | Fdtor))
    {   type *t = s->Stype;

        cpp_calling_convention(t);
        CHAR('@');                      // return_type for ctors & dtors
        cpp_argument_types(t);
        cpp_throw_types(t);
    }
    else
        cpp_static_member_function_type(s);
}


STATIC void cpp_external_data_type(symbol *s)
{
    cpp_primary_data_type(s->Stype);
    cpp_storage_convention(s);
}

STATIC void cpp_external_function_type(symbol *s)
{
    cpp_function_type(s->Stype);
}

STATIC void cpp_type_encoding(symbol *s)
{   char c;

    //printf("cpp_type_encoding()\n");
    if (tyfunc(s->Stype->Tty))
    {   int farfunc;

        farfunc = tyfarfunc(s->Stype->Tty) != 0;

        if (isclassmember(s))
        {   // Member function
            int protection;
            int ftype;

            protection = cpp_protection(s);
            if (s->Sfunc->Fthunk && !(s->Sfunc->Fflags & Finstance))
                ftype = 3;
            else
                switch (s->Sfunc->Fflags & (Fvirtual | Fstatic))
                {   case Fvirtual:      ftype = 2;      break;
                    case Fstatic:       ftype = 1;      break;
                    case 0:             ftype = 0;      break;
                    default:            assert(0);
                }
            CHAR('A' + farfunc + protection * 8 + ftype * 2);
            switch (ftype)
            {   case 0: cpp_member_function_type(s);            break;
                case 1: cpp_static_member_function_type(s);     break;
                case 2: cpp_member_function_type(s);            break;
                case 3: cpp_adjustor_thunk_type(s);             break;
            }
        }
        else

        {   // Non-member function
            CHAR('Y' + farfunc);
            cpp_external_function_type(s);
        }
    }
    else
    {

        if (isclassmember(s))
        {
            {   // Static data member
                CHAR(cpp_protection(s) + '0');
                cpp_static_member_data_type(s);
            }
        }
        else

        {
            if (s->Sclass == SCstatic
                || (s->Sscope &&
                 s->Sscope->Sclass != SCstruct &&
                 s->Sscope->Sclass != SCnamespace)

                )
            {   CHAR('4');
                cpp_local_static_data_type(s);
            }
            else
            {   CHAR('3');
                cpp_external_data_type(s);
            }
        }
    }
}

STATIC void cpp_scope(symbol *s)
{
    /*  scope ::=
                zname [ scope ]
                '?' decorated_name [ scope ]
                '?' lexical_frame [ scope ]
                '?' '$' template_name [ scope ]
     */
    while (s)
    {   char *p;

        symbol_debug(s);
        switch (s->Sclass)
        {
            case SCnamespace:
                cpp_zname(s->Sident);
                break;

            case SCstruct:
                cpp_zname(symbol_ident(s));
                break;

            default:
                STR("?1?");                     // Why? Who knows.
                cpp_decorated_name(s);
                break;
        }

        s = s->Sscope;

    }
}

STATIC void cpp_zname(const char *p)
{
    //printf("cpp_zname(%s)\n", p);
    if (*p != '?' ||                            // if not operator_name
        (NEWTEMPMANGLE && p[1] == '$'))         // ?$ is a template name
    {

        /* Scan forward past any dots
         */
        for (const char *q = p; *q; q++)
        {
            if (*q == '.')
                p = q + 1;
        }


        for (int i = 0; i < mangle.znamei; i++)
        {
            if (strcmp(p,mangle.zname[i]) == 0)
            {   CHAR('0' + i);
                return;
            }
        }
        if (mangle.znamei < 10)
            mangle.zname[mangle.znamei++] = p;
        STR(p);
        CHAR('@');
    }
    else if (p[1] == 'B')
        STR("?B");                      // skip return value encoding
    else
    {
        STR(p);
    }
}

STATIC void cpp_symbol_name(symbol *s)
{   char *p;

    p = s->Sident;

    cpp_zname(p);
}

STATIC void cpp_decorated_name(symbol *s)
{   char *p;

    CHAR('?');
    cpp_symbol_name(s);

    if (s->Sscope)
        cpp_scope(s->Sscope);

    CHAR('@');
    cpp_type_encoding(s);
}

#endif
