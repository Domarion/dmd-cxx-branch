
/* Compiler implementation of the D programming language
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/dlang/dmd/blob/master/src/dmd/mangle.h
 */

#pragma once

class Dsymbol;
class Expression;
class FuncDeclaration;
class TemplateInstance;
class Type;
struct OutBuffer;

// In cppmangle.c
const char *toCppMangleItanium(Dsymbol *s);
const char *cppTypeInfoMangleItanium(Dsymbol *s);
const char *cppThunkMangleItanium(FuncDeclaration *fd, int offset);

// In dmangle.c
const char *mangleExact(FuncDeclaration *fd);
void mangleToBuffer(Type *s, OutBuffer *buf);
void mangleToBuffer(Expression *s, OutBuffer *buf);
void mangleToBuffer(Dsymbol *s, OutBuffer *buf);
void mangleToBuffer(TemplateInstance *s, OutBuffer *buf);
