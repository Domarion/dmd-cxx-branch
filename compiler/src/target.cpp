
/* Compiler implementation of the D programming language
 * Copyright (C) 2013-2021 by The D Language Foundation, All Rights Reserved
 * written by Iain Buclaw
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/D-Programming-Language/dmd/blob/master/src/target.c
 */

#include "root/dsystem.hpp"

#if defined(__GNUC__) || defined(__clang__)
#include <limits> // for std::numeric_limits
#else
#include <math.h>
#include <float.h>
#endif

#include "target.hpp"
#include "aggregate.hpp"
#include "enum.hpp"
#include "id.hpp"
#include "mars.hpp"
#include "mtype.hpp"
#include "statement.hpp"
#include "root/outbuffer.hpp"

const char *toCppMangleItanium(Dsymbol *);
const char *cppTypeInfoMangleItanium(Dsymbol *);
const char *toCppMangleMSVC(Dsymbol *);
const char *cppTypeInfoMangleMSVC(Dsymbol *);
TypeTuple *toArgTypes(Type *t);

Target target;

/* Initialize the floating point constants for TYPE.  */

template <typename T>
static void initFloatConstants(Target::FPTypeProperties<T> &f)
{
#if defined(__GNUC__) || defined(__clang__)
    f.max = std::numeric_limits<T>::max();
    f.min_normal = std::numeric_limits<T>::min();

    assert(std::numeric_limits<T>::has_quiet_NaN);
    f.nan = std::numeric_limits<T>::quiet_NaN();

    assert(std::numeric_limits<T>::has_signaling_NaN);
    f.snan = std::numeric_limits<T>::signaling_NaN();

    assert(std::numeric_limits<T>::has_infinity);
    f.infinity = std::numeric_limits<T>::infinity();

    f.epsilon = std::numeric_limits<T>::epsilon();
    f.dig = std::numeric_limits<T>::digits10;
    f.mant_dig = std::numeric_limits<T>::digits;
    f.max_exp = std::numeric_limits<T>::max_exponent;
    f.min_exp = std::numeric_limits<T>::min_exponent;
    f.max_10_exp = std::numeric_limits<T>::max_exponent10;
    f.min_10_exp = std::numeric_limits<T>::min_exponent10;
#else
    union
    {   unsigned int ui[4];
        real_t ld;
    } snan = {{ 0, 0xA0000000, 0x7FFF, 0 }};

    if (sizeof(T) == sizeof(float))
    {
        f.max = FLT_MAX;
        f.min_normal = FLT_MIN;

        f.nan = NAN;
        f.snan = snan.ld;
        f.infinity = INFINITY;

        f.epsilon = FLT_EPSILON;
        f.dig = FLT_DIG;
        f.mant_dig = FLT_MANT_DIG;
        f.max_exp = FLT_MAX_EXP;
        f.min_exp = FLT_MIN_EXP;
        f.max_10_exp = FLT_MAX_10_EXP;
        f.min_10_exp = FLT_MIN_10_EXP;
    }
    else if (sizeof(T) == sizeof(double))
    {
        f.max = DBL_MAX;
        f.min_normal = DBL_MIN;

        f.nan = NAN;
        f.snan = snan.ld;
        f.infinity = INFINITY;

        f.epsilon = DBL_EPSILON;
        f.dig = DBL_DIG;
        f.mant_dig = DBL_MANT_DIG;
        f.max_exp = DBL_MAX_EXP;
        f.min_exp = DBL_MIN_EXP;
        f.max_10_exp = DBL_MAX_10_EXP;
        f.min_10_exp = DBL_MIN_10_EXP;
    }
    else
    {
        f.max = LDBL_MAX;
        f.min_normal = LDBL_MIN;

        f.nan = NAN;
        f.snan = snan.ld;
        f.infinity = INFINITY;

        f.epsilon = LDBL_EPSILON;
        f.dig = LDBL_DIG;
        f.mant_dig = LDBL_MANT_DIG;
        f.max_exp = LDBL_MAX_EXP;
        f.min_exp = LDBL_MIN_EXP;
        f.max_10_exp = LDBL_MAX_10_EXP;
        f.min_10_exp = LDBL_MIN_10_EXP;
    }
#endif
}

void Target::_init(const Param &params)
{
    initFloatConstants<float>(target.FloatProperties);
    initFloatConstants<double>(target.DoubleProperties);
    initFloatConstants<real_t>(target.RealProperties);

    // These have default values for 32 bit code, they get
    // adjusted for 64 bit code.
    ptrsize = 4;
    classinfosize = 0x4C;   // 76

    /* gcc uses int.max for 32 bit compilations, and long.max for 64 bit ones.
     * Set to int.max for both, because the rest of the compiler cannot handle
     * 2^64-1 without some pervasive rework. The trouble is that much of the
     * front and back end uses 32 bit ints for sizes and offsets. Since C++
     * silently truncates 64 bit ints to 32, finding all these dependencies will be a problem.
     */
    maxStaticDataSize = 0x7FFFFFFF;

    if (params.isLP64)
    {
        ptrsize = 8;
        classinfosize = 0x98;   // 152
    }

    if (params.isLinux)
    {
        realsize = 12;
        realpad = 2;
        realalignsize = 4;
    }
    else
        assert(0);

    if (params.is64bit)
    {
        if (params.isLinux)
        {
            realsize = 16;
            realpad = 6;
            realalignsize = 16;
        }
    }

    if (params.isLinux)
        c.longsize = 4;
    else
        assert(0);
    if (params.is64bit)
    {
        if (params.isLinux)
            c.longsize = 8;
    }
    c.long_doublesize = realsize;
    c.twchar_t = Type::tdchar;

    if (params.isLinux)
        cpp.twoDtorInVtable = true;
    else
        assert(0);
    cpp.exceptions = params.isLinux;
}

/******************************
 * Return memory alignment size of type.
 */

unsigned Target::alignsize(Type* type)
{
    assert (type->isTypeBasic());

    switch (type->ty)
    {
        case Tfloat80:
        case Timaginary80:
        case Tcomplex80:
            return Target::realalignsize;

        case Tcomplex32:
            if (global.params.isLinux)
                return 4;
            break;

        case Tint64:
        case Tuns64:
        case Tfloat64:
        case Timaginary64:
        case Tcomplex64:
            if (global.params.isLinux)
                return global.params.is64bit ? 8 : 4;
            break;

        default:
            break;
    }
    return (unsigned)type->size(Loc());
}

/******************************
 * Return field alignment size of type.
 */

unsigned Target::fieldalign(Type* type)
{
    return type->alignsize();
}

/***********************************
 * Returns a Type for the va_list type of the target.
 * NOTE: For Posix/x86_64 this returns the type which will really
 * be used for passing an argument of type va_list.
 */
Type *Target::va_listType(const Loc &loc, Scope *sc)
{
    if (tvalist)
        return tvalist;

    if (global.params.isLinux)
    {
        if (global.params.is64bit)
        {
            tvalist = (new TypeIdentifier(Loc(), Identifier::idPool("__va_list_tag")))->pointerTo();
            tvalist = typeSemantic(tvalist, loc, sc);
        }
        else
        {
            tvalist = Type::tchar->pointerTo();
        }
    }
    else
    {
        assert(0);
        return nullptr;
    }

    return tvalist;
}

/**
 * Checks whether the target supports a vector type with total size `sz`
 * (in bytes) and element type `type`.
 *
 * Returns: 0 if the type is supported, or else: 1 if vector types are not
 *     supported on the target at all, 2 if the element type isn't, or 3 if
 *     the given size isn't.
 */

int Target::isVectorTypeSupported(int sz, Type *type)
{
    if (!global.params.is64bit)
        return 1; // not supported

    switch (type->ty)
    {
        case Tvoid:
        case Tint8:
        case Tuns8:
        case Tint16:
        case Tuns16:
        case Tint32:
        case Tuns32:
        case Tfloat32:
        case Tint64:
        case Tuns64:
        case Tfloat64:
            break;
        default:
            return 2; // wrong base type
    }

    if (sz != 16 && sz != 32)
        return 3; // wrong size

    return 0;
}

/**
 * Checks whether the target supports operation `op` for vectors of type `type`.
 * For binary ops `t2` is the type of the 2nd operand.
 *
 * Returns:
 *      true if the operation is supported or type is not a vector
 */
bool Target::isVectorOpSupported(Type *type, TOK op, Type *)
{
    if (type->ty != Tvector)
        return true; // not a vector op
    TypeVector *tvec = (TypeVector*)type;

    bool supported;
    switch (op)
    {
        case TOKneg: case TOKuadd:
            supported = tvec->isscalar();
            break;

        case TOKlt: case TOKgt: case TOKle: case TOKge: case TOKequal: case TOKnotequal: case TOKidentity: case TOKnotidentity:
            supported = false;
            break;

        case TOKunord: case TOKlg: case TOKleg: case TOKule: case TOKul: case TOKuge: case TOKug: case TOKue:
            supported = false;
            break;

        case TOKshl: case TOKshlass: case TOKshr: case TOKshrass: case TOKushr: case TOKushrass:
            supported = false;
            break;

        case TOKadd: case TOKaddass: case TOKmin: case TOKminass:
            supported = tvec->isscalar();
            break;

        case TOKmul: case TOKmulass:
            // only floats and short[8]/ushort[8] (PMULLW)
            if (tvec->isfloating() || tvec->elementType()->size(Loc()) == 2)
                supported = true;
            else
                supported = false;
            break;

        case TOKdiv: case TOKdivass:
            supported = tvec->isfloating();
            break;

        case TOKmod: case TOKmodass:
            supported = false;
            break;

        case TOKand: case TOKandass: case TOKor: case TOKorass: case TOKxor: case TOKxorass:
            supported = tvec->isintegral();
            break;

        case TOKnot:
            supported = false;
            break;

        case TOKtilde:
            supported = tvec->isintegral();
            break;

        case TOKpow: case TOKpowass:
            supported = false;
            break;

        default:
            assert(0);
    }
    return supported;
}

/******************************
 * Return the default system linkage for the target.
 */
LINK Target::systemLinkage()
{
    return LINKc;
}

/**
 * Return a tuple describing how argument type is put to a function.
 * Value is an empty tuple if type is always passed on the stack.
 * nullptr if the type is a `void` or argtypes aren't supported by the target.
 */
TypeTuple *Target::toArgTypes(Type *t)
{
  return ::toArgTypes(t);
}

/**
 * Determine return style of function - whether in registers or
 * through a hidden pointer to the caller's stack.
 * Params:
 *   tf = function type to check
 *   needsThis = true if the function type is for a non-static member function
 * Returns:
 *   true if return value from function is on the stack
 */
bool Target::isReturnOnStack(TypeFunction *tf, bool needsThis)
{
    if (tf->isref)
    {
        //printf("  ref false\n");
        return false;                 // returns a pointer
    }

    Type *tn = tf->next;
    if (TypeEnum *te = tn->isTypeEnum())
    {
        if (te->sym->isSpecial())
        {
            // Special enums with target-specific return style
            if (te->sym->ident == Id::__c_complex_float)
                tn = Type::tcomplex32->castMod(tn->mod);
            else if (te->sym->ident == Id::__c_complex_double)
                tn = Type::tcomplex64->castMod(tn->mod);
            else if (te->sym->ident == Id::__c_complex_real)
                tn = Type::tcomplex80->castMod(tn->mod);
        }
    }
    tn = tn->toBasetype();
    //printf("tn = %s\n", tn->toChars());
    d_uns64 sz = tn->size();
    Type *tns = tn;

Lagain:
    if (tns->ty == Tsarray)
    {
        tns = tns->baseElemOf();
        if (tns->ty != Tstruct)
        {
L2:
            if (global.params.isLinux && tf->linkage != LINKd && !global.params.is64bit)
            {
                ;                               // 32 bit C/C++ structs always on stack
            }
            else
            {
                switch (sz)
                {
                    case 1:
                    case 2:
                    case 4:
                    case 8:
                        //printf("  sarray false\n");
                        return false; // return small structs in regs
                                      // (not 3 byte structs!)
                    default:
                        break;
                }
            }
            //printf("  sarray true\n");
            return true;
        }
    }

    if (tns->ty == Tstruct)
    {
        StructDeclaration *sd = ((TypeStruct *)tns)->sym;
        if (global.params.isLinux && tf->linkage != LINKd && !global.params.is64bit)
        {
            //printf("  2 true\n");
            return true;            // 32 bit C/C++ structs always on stack
        }

        if (sd->arg1type && !sd->arg2type)
        {
            tns = sd->arg1type;
            if (tns->ty != Tstruct)
                goto L2;
            goto Lagain;
        }
        else if (global.params.is64bit && !sd->arg1type && !sd->arg2type)
            return true;
        else if (sd->isPOD())
        {
            switch (sz)
            {
                case 1:
                case 2:
                case 4:
                case 8:
                    //printf("  3 false\n");
                    return false;     // return small structs in regs
                                      // (not 3 byte structs!)
                case 16:
                    if (global.params.is64bit)
                       return false;

                default:
                    break;
            }
        }
        //printf("  3 true\n");
        return true;
    }
    else if ((global.params.isLinux) &&
             tf->linkage == LINKc &&
             tns->iscomplex())
    {
        if (tns->ty == Tcomplex32)
            return false;     // in EDX:EAX, not ST1:ST0
        else
            return true;
    }
    else
    {
        //assert(sz <= 16);
        //printf("  4 false\n");
        return false;
    }
}

/**
 * Get targetInfo by key
 * Params:
 *  name = name of targetInfo to get
 *  loc = location to use for error messages
 * Returns:
 *  Expression for the requested targetInfo
 */
Expression *Target::getTargetInfo(const char* name, const Loc& loc)
{
    switch (strlen(name))
    {
        case 6:
            if (strcmp(name, "cppStd") == 0)
                return new IntegerExp(loc, global.params.cplusplus, Type::tint32);
            break;

        case 8:
            if (strcmp(name, "floatAbi") == 0)
                return new StringExp(loc, const_cast<char*>("hard"));
            break;

        case 12:
            if (strcmp(name, "objectFormat") == 0)
            {
                return new StringExp(loc, const_cast<char*>("elf"));
            }
            break;

        case 17:
            if (strcmp(name, "cppRuntimeLibrary") == 0)
            {
                return new StringExp(loc, const_cast<char*>("snn"));
            }
            return new StringExp(loc, const_cast<char*>(""));
    }

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/**
 * Functions and variables specific to interface with extern(C++) ABI.
 */

const char *TargetCPP::toMangle(Dsymbol *s)
{
    return toCppMangleItanium(s);
}

const char *TargetCPP::typeInfoMangle(ClassDeclaration *cd)
{
    return cppTypeInfoMangleItanium(cd);
}

const char *TargetCPP::thunkMangle(FuncDeclaration *, int)
{
    return nullptr;
}

/******************************
 * For a vendor-specific type, return a string containing the C++ mangling.
 * In all other cases, return null.
 */
const char* TargetCPP::typeMangle(Type *)
{
    return nullptr;
}

/**
 * Get the type that will really be used for passing the given argument
 * to an `extern(C++)` function.
 * Params:
 *      p = parameter to be passed.
 * Returns:
 *      `Type` to use for parameter `p`.
 */
Type *TargetCPP::parameterType(Parameter *p)
{
    Type *t = p->type->merge2();
    if (p->storageClass & (STCout | STCref))
        t = t->referenceTo();
    else if (p->storageClass & STClazy)
    {
        // Mangle as delegate
        Type *td = new TypeFunction(ParameterList(), t, LINKd);
        td = new TypeDelegate(td);
        t = t->merge();
    }
    return t;
}

/**
 * Checks whether type is a vendor-specific fundamental type.
 * Params:
 *      t = type to inspect
 *      isFundamental = where to store result
 * Returns:
 *      true if isFundamental was set by function
 */
bool TargetCPP::fundamentalType(const Type *, bool &)
{
    return false;
}

/**
 * Get the starting offset position for fields of an `extern(C++)` class
 * that is derived from the given base class.
 * Params:
 *      baseClass = base class with C++ linkage
 * Returns:
 *      starting offset to lay out derived class fields
 */
unsigned TargetCPP::derivedClassOffset(ClassDeclaration *baseClass)
{
    return baseClass->structsize;
}
