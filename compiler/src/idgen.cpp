
/* Compiler implementation of the D programming language
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/D-Programming-Language/dmd/blob/master/src/idgen.c
 */

// Program to generate string files in d data structures.
// Saves much tedious typing, and eliminates typo problems.
// Generates:
//      id.h
//      id.c

#include "root/dsystem.hpp"

struct Msgtable
{
    const char* ident;          // name to use in DMD source
    const char* name = nullptr; // name in D executable
};

Msgtable msgtable[] =
{
    { "IUnknown"},
    { "Object"},
    { "object"},
    { "string"},
    { "wstring"},
    { "dstring"},
    { "max"},
    { "min"},
    { "This", "this" },
    { "_super", "super" },
    { "ctor", "__ctor" },
    { "dtor", "__dtor" },
    { "__xdtor", "__xdtor" },
    { "__fieldDtor", "__fieldDtor" },
    { "__aggrDtor", "__aggrDtor" },
    { "postblit", "__postblit" },
    { "__xpostblit", "__xpostblit" },
    { "__fieldPostblit", "__fieldPostblit" },
    { "__aggrPostblit", "__aggrPostblit" },
    { "classInvariant", "__invariant" },
    { "unitTest", "__unitTest" },
    { "require", "__require" },
    { "ensure", "__ensure" },
    { "_init", "init" },
    { "__sizeof", "sizeof" },
    { "__xalignof", "alignof" },
    { "_mangleof", "mangleof" },
    { "stringof"},
    { "_tupleof", "tupleof" },
    { "length"},
    { "remove"},
    { "ptr"},
    { "array"},
    { "funcptr"},
    { "dollar", "__dollar" },
    { "ctfe", "__ctfe" },
    { "offset"},
    { "offsetof"},
    { "ModuleInfo"},
    { "ClassInfo"},
    { "classinfo"},
    { "typeinfo"},
    { "outer"},
    { "Exception"},
    { "RTInfo"},
    { "Throwable"},
    { "Error"},
    { "withSym", "__withSym" },
    { "result", "__result" },
    { "returnLabel", "__returnLabel" },
    { "line"},
    { "empty", "" },
    { "p"},
    { "q"},
    { "__vptr"},
    { "__monitor"},
    { "gate", "__gate" },
    { "__c_long"},
    { "__c_ulong"},
    { "__c_longlong"},
    { "__c_ulonglong"},
    { "__c_long_double"},
    { "__c_wchar_t"},
    { "__c_complex_float"},
    { "__c_complex_double"},
    { "__c_complex_real"},
    { "cpp_type_info_ptr", "__cpp_type_info_ptr" },
    { "_assert", "assert" },
    { "_unittest", "unittest" },
    { "printf"},
    { "scanf"},

    { "TypeInfo"},
    { "TypeInfo_Class"},
    { "TypeInfo_Interface"},
    { "TypeInfo_Struct"},
    { "TypeInfo_Enum"},
    { "TypeInfo_Pointer"},
    { "TypeInfo_Vector"},
    { "TypeInfo_Array"},
    { "TypeInfo_StaticArray"},
    { "TypeInfo_AssociativeArray"},
    { "TypeInfo_Function"},
    { "TypeInfo_Delegate"},
    { "TypeInfo_Tuple"},
    { "TypeInfo_Const"},
    { "TypeInfo_Invariant"},
    { "TypeInfo_Shared"},
    { "TypeInfo_Wild", "TypeInfo_Inout" },
    { "elements"},
    { "_arguments_typeinfo"},
    { "_arguments"},
    { "_argptr"},
    { "destroy"},
    { "xopEquals", "__xopEquals" },
    { "xopCmp", "__xopCmp" },
    { "xtoHash", "__xtoHash" },

    { "LINE", "__LINE__" },
    { "FILE", "__FILE__" },
    { "MODULE", "__MODULE__" },
    { "FUNCTION", "__FUNCTION__" },
    { "PRETTY_FUNCTION", "__PRETTY_FUNCTION__" },
    { "DATE", "__DATE__" },
    { "TIME", "__TIME__" },
    { "TIMESTAMP", "__TIMESTAMP__" },
    { "VENDOR", "__VENDOR__" },
    { "VERSIONX", "__VERSION__" },
    { "EOFX", "__EOF__" },

    { "nan"},
    { "infinity"},
    { "dig"},
    { "epsilon"},
    { "mant_dig"},
    { "max_10_exp"},
    { "max_exp"},
    { "min_10_exp"},
    { "min_exp"},
    { "min_normal"},
    { "re"},
    { "im"},

    { "C"},
    { "D"},
    { "System"},

    { "exit"},
    { "success"},
    { "failure"},

    { "keys"},
    { "values"},
    { "rehash"},

    { "future", "__future" },
    { "property"},
    { "nogc"},
    { "safe"},
    { "trusted"},
    { "system"},
    { "disable"},

    // For inline assembler
    { "___out", "out" },
    { "___in", "in" },
    { "__int", "int" },
    { "_dollar", "$" },
    { "__LOCAL_SIZE"},

    // For operator overloads
    { "uadd",    "opPos" },
    { "neg",     "opNeg" },
    { "com",     "opCom" },
    { "add",     "opAdd" },
    { "add_r",   "opAdd_r" },
    { "sub",     "opSub" },
    { "sub_r",   "opSub_r" },
    { "mul",     "opMul" },
    { "mul_r",   "opMul_r" },
    { "div",     "opDiv" },
    { "div_r",   "opDiv_r" },
    { "mod",     "opMod" },
    { "mod_r",   "opMod_r" },
    { "eq",      "opEquals" },
    { "cmp",     "opCmp" },
    { "iand",    "opAnd" },
    { "iand_r",  "opAnd_r" },
    { "ior",     "opOr" },
    { "ior_r",   "opOr_r" },
    { "ixor",    "opXor" },
    { "ixor_r",  "opXor_r" },
    { "shl",     "opShl" },
    { "shl_r",   "opShl_r" },
    { "shr",     "opShr" },
    { "shr_r",   "opShr_r" },
    { "ushr",    "opUShr" },
    { "ushr_r",  "opUShr_r" },
    { "cat",     "opCat" },
    { "cat_r",   "opCat_r" },
    { "assign",  "opAssign" },
    { "addass",  "opAddAssign" },
    { "subass",  "opSubAssign" },
    { "mulass",  "opMulAssign" },
    { "divass",  "opDivAssign" },
    { "modass",  "opModAssign" },
    { "andass",  "opAndAssign" },
    { "orass",   "opOrAssign" },
    { "xorass",  "opXorAssign" },
    { "shlass",  "opShlAssign" },
    { "shrass",  "opShrAssign" },
    { "ushrass", "opUShrAssign" },
    { "catass",  "opCatAssign" },
    { "postinc", "opPostInc" },
    { "postdec", "opPostDec" },
    { "index",   "opIndex" },
    { "indexass", "opIndexAssign" },
    { "slice",   "opSlice" },
    { "sliceass", "opSliceAssign" },
    { "call",    "opCall" },
    { "_cast",    "opCast" },
    { "opIn"},
    { "opIn_r"},
    { "opStar"},
    { "opDot"},
    { "opDispatch"},
    { "opDollar"},
    { "opUnary"},
    { "opIndexUnary"},
    { "opSliceUnary"},
    { "opBinary"},
    { "opBinaryRight"},
    { "opOpAssign"},
    { "opIndexOpAssign"},
    { "opSliceOpAssign"},
    { "pow", "opPow" },
    { "pow_r", "opPow_r" },
    { "powass", "opPowAssign" },

    { "classNew", "new" },
    { "classDelete", "delete" },

    // For foreach
    { "apply", "opApply" },
    { "applyReverse", "opApplyReverse" },

    // Ranges
    { "Fempty", "empty" },
    { "Ffront", "front" },
    { "Fback", "back" },
    { "FpopFront", "popFront" },
    { "FpopBack", "popBack" },

    // For internal functions
    { "aaLen", "_aaLen" },
    { "aaKeys", "_aaKeys" },
    { "aaValues", "_aaValues" },
    { "aaRehash", "_aaRehash" },
    { "monitorenter", "_d_monitorenter" },
    { "monitorexit", "_d_monitorexit" },
    { "criticalenter", "_d_criticalenter2" },
    { "criticalexit", "_d_criticalexit" },
    { "__ArrayEq"},
    { "__ArrayPostblit"},
    { "__ArrayDtor"},

    // TODO: backport _d_*
    { "_d_delThrowable"},
    { "_d_newThrowable"},
    { "_d_newclassT"},
    { "_d_newitemT"},
    { "_d_newarrayT"},
    { "_d_newarraymTX"},
    { "_d_assert_fail"},

    { "dup"},
    { "_aaApply"},
    { "_aaApply2"},

    // TODO: backport _d_*
    { "_d_arrayctor"},
    { "_d_arraysetctor"},
    { "_d_arraysetassign"},
    { "_d_arrayassign_l"},
    { "_d_arrayassign_r"},

    // For pragma's
    { "Pinline", "inline" },
    { "lib"},
    { "mangle"},
    { "msg"},
    { "startaddress"},

    // For special functions
    { "tohash", "toHash" },
    { "tostring", "toString" },
    { "getmembers", "getMembers" },

    // Special functions
    { "__alloca", "alloca" },
    { "main"},
    { "DllMain"},
    { "tls_get_addr", "___tls_get_addr" },
    { "entrypoint", "__entrypoint" },

    // TODO: backport _d_*
    { "_d_arraysetlengthTImpl"},
    { "_d_arraysetlengthT"},
    { "_d_arrayappendT" },
    { "_d_arrayappendcTX" },
    { "_d_arraycatnTX" },

    // varargs implementation
    { "stdc"},
    { "stdarg"},
    { "va_start"},

    // Builtin functions
    { "std"},
    { "core"},

    // TODO: backport? Skipped c_complex_float, double, real
    { "config"},
    { "etc"},

    { "attribute"},

    // TODO: backport?
    { "atomic"},
    { "atomicOp"},

    { "math"},
    { "sin"},
    { "cos"},
    { "tan"},
    { "_sqrt", "sqrt" },
    { "_pow", "pow" },
    { "atan2"},
    { "rint"},
    { "ldexp"},
    { "rndtol"},
    { "exp"},
    { "expm1"},
    { "exp2"},
    { "yl2x"},
    { "yl2xp1"},
    { "log"},
    { "log2"},
    { "log10"},
    { "round"},
    { "floor"},
    { "trunc"},
    { "fmax"},
    { "fmin"},
    { "fma"},
    { "isnan"},
    { "isInfinity"},
    { "isfinite"},
    { "ceil"},
    { "copysign"},
    { "fabs"},
    { "toPrec"},
    { "simd"},
    { "__prefetch"},
    { "__simd_sto"},
    { "__simd"},
    { "__simd_ib"},
    { "bitop"},
    { "bsf"},
    { "bsr"},
    { "btc"},
    { "btr"},
    { "bts"},
    { "bswap"},
    { "_volatile", "volatile" },
    { "volatileLoad"},
    { "volatileStore"},
    { "_popcnt"},
    { "inp"},
    { "inpl"},
    { "inpw"},
    { "outp"},
    { "outpl"},
    { "outpw"},

    // Traits
    { "isAbstractClass"},
    { "isArithmetic"},
    { "isAssociativeArray"},
    { "isFinalClass"},
    { "isTemplate"},
    { "isPOD"},
    { "isDeprecated"},
    { "isDisabled"},
    { "isFuture" },
    { "isNested"},
    { "isFloating"},
    { "isIntegral"},
    { "isScalar"},
    { "isStaticArray"},
    { "isUnsigned"},
    { "isVirtualMethod"},
    { "isAbstractFunction"},
    { "isFinalFunction"},
    { "isOverrideFunction"},
    { "isStaticFunction"},
    { "isModule"},
    { "isPackage"},
    { "isRef"},
    { "isOut"},
    { "isLazy"},
    { "hasMember"},
    { "identifier"},

    // TODO: backport?
    { "fullyQualifiedName"},

    { "getProtection"},
    { "getVisibility"},
    { "parent"},
    { "child"},
    { "getMember"},
    { "getOverloads"},
    { "getVirtualMethods"},
    { "classInstanceSize"},

    // TODO: backport?
    { "classInstanceAlignment"},

    { "allMembers"},
    { "derivedMembers"},
    { "isSame"},
    { "compiles"},
    { "getAliasThis"},
    { "getAttributes"},
    { "getFunctionAttributes"},
    { "getFunctionVariadicStyle"},
    { "getParameterStorageClasses"},
    { "getLinkage"},
    { "getUnitTests"},
    { "getVirtualIndex"},
    { "getPointerBitmap"},

    // TODO: backport?
    { "initSymbol"},
    { "getCppNamespaces"},

    { "isReturnOnStack"},
    { "isZeroInit"},
    { "getTargetInfo"},
    { "getLocation"},
    { "hasPostblit"},

    // TODO: backport?
    { "hasCopyConstructor"},

    { "isCopyable"},
    { "toType"},

    // TODO: backport?
    { "parameters"},

    // For C++ mangling
    { "allocator"},
    { "basic_string"},
    { "basic_istream"},
    { "basic_ostream"},
    { "basic_iostream"},
    { "char_traits"},

    // Compiler recognized UDA's
    { "udaSelector", "selector" },

    // C names, for undefined identifier error messages
    { "C_NULL", "nullptr" },
    { "C_TRUE", "TRUE" },
    { "C_FALSE", "FALSE" },
    { "C_unsigned", "unsigned" },
    { "C_wchar_t", "wchar_t" },
};

int main()
{
    {
        FILE *fp = fopen("id.hpp","wb");
        if (!fp)
        {
            printf("can't open id.hpp\n");
            exit(EXIT_FAILURE);
        }

        fprintf(fp, "#pragma once\n");
        fprintf(fp, "// File generated by idgen.cpp\n\n");
        fprintf(fp, "class Identifier;\n\n");
        fprintf(fp, "struct Id\n");
        fprintf(fp, "{\n");

        for (unsigned i = 0; i < sizeof(msgtable) / sizeof(msgtable[0]); i++)
        {
            const char *id = msgtable[i].ident;
            fprintf(fp,"    static Identifier *%s;\n", id);
        }

        fprintf(fp, "    static void initialize();\n");
        fprintf(fp, "};\n");
        fclose(fp);
    }

    {
        FILE *fp = fopen("id.cpp","wb");
        if (!fp)
        {
            printf("can't open id.cpp\n");
            exit(EXIT_FAILURE);
        }

        fprintf(fp, "// File generated by idgen.cpp\n");
        fprintf(fp, "#include \"identifier.hpp\"\n");
        fprintf(fp, "#include \"id.hpp\"\n");
        fprintf(fp, "#include \"mars.hpp\"\n");

        for (unsigned i = 0; i < sizeof(msgtable) / sizeof(msgtable[0]); i++)
        {
            const char *id = msgtable[i].ident;
            const char *p = msgtable[i].name;

            if (!p)
                p = id;
            fprintf(fp,"Identifier *Id::%s;\n", id);
        }

        fprintf(fp, "void Id::initialize()\n");
        fprintf(fp, "{\n");

        for (unsigned i = 0; i < sizeof(msgtable) / sizeof(msgtable[0]); i++)
        {
            const char *id = msgtable[i].ident;
            const char *p = msgtable[i].name;

            if (!p)
                p = id;
            fprintf(fp,"    %s = Identifier::idPool(\"%s\");\n", id, p);
        }

        fprintf(fp, "}\n");

        fclose(fp);
    }

    {
        FILE *fp = fopen("id.d","wb");
        if (!fp)
        {
            printf("can't open id.d\n");
            exit(EXIT_FAILURE);
        }

        fprintf(fp, "// File generated by idgen.cpp\n\n");
        fprintf(fp, "module ddmd.id;\n\n");
        fprintf(fp, "import ddmd.identifier, ddmd.tokens;\n\n");
        fprintf(fp, "struct Id\n");
        fprintf(fp, "{\n");

        for (unsigned i = 0; i < sizeof(msgtable) / sizeof(msgtable[0]); i++)
        {
            const char *id = msgtable[i].ident;
            const char *p = msgtable[i].name;

            if (!p)
                p = id;
            fprintf(fp, "    extern (C++) static __gshared Identifier %s;\n", id);
        }

        fprintf(fp, "\n");
        fprintf(fp, "    extern (C++) static void initialize()\n");
        fprintf(fp, "    {\n");

        for (unsigned i = 0; i < sizeof(msgtable) / sizeof(msgtable[0]); i++)
        {
            const char *id = msgtable[i].ident;
            const char *p = msgtable[i].name;

            if (!p)
                p = id;
            fprintf(fp,"        %s = Identifier.idPool(\"%s\");\n", id, p);
        }

        fprintf(fp, "    }\n");
        fprintf(fp, "}\n");

        fclose(fp);
    }

    return EXIT_SUCCESS;
}
