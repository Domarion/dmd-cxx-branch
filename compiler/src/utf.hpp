
/* Compiler implementation of the D programming language
 * Copyright (C) 2003-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/D-Programming-Language/dmd/blob/master/src/utf.h
 */

#pragma once

#include "root/dsystem.hpp"

/// A UTF-8 code unit
using utf8_t = uint8_t;
/// A UTF-16 code unit
using utf16_t = uint16_t;
/// A UTF-32 code unit
using utf32_t = uint32_t;
using dchar_t = utf32_t;

/// \return true if \a c is a valid, non-private UTF-32 code point
bool utf_isValidDchar(dchar_t c);

bool isUniAlpha(dchar_t c);

int utf_codeLengthChar(dchar_t c);
int utf_codeLengthWchar(dchar_t c);
int utf_codeLength(int sz, dchar_t c);

void utf_encodeChar(utf8_t *s, dchar_t c);
void utf_encodeWchar(utf16_t *s, dchar_t c);
void utf_encode(int sz, void *s, dchar_t c);

const char *utf_decodeChar(utf8_t const *s, size_t len, size_t *pidx, dchar_t *presult);
const char *utf_decodeWchar(utf16_t const *s, size_t len, size_t *pidx, dchar_t *presult);
bool isBidiControl(dchar_t c);
