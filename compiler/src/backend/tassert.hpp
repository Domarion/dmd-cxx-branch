// Copyright (C) 1989-1998 by Symantec
// Copyright (C) 2000-2021 by The D Language Foundation, All Rights Reserved
// http://www.digitalmars.com
// Written by Walter Bright
/*
 * This source file is made available for personal use
 * only. The license is in backendlicense.txt
 * For any other uses, please contact Digital Mars.
 */

#pragma once

/*****************************
 * Define a local assert function.
 */

#undef assert
#define assert(e)       ((e) || (local_assert(__LINE__), 0))

#if __clang__

void util_assert(const char * , int) __attribute__((noreturn));

__attribute__((noreturn)) static void local_assert(int line)
{
    util_assert(__file__,line);
    __builtin_unreachable();
}

#else

void util_assert(const char *, int);

static void local_assert(int line)
{
    util_assert(__file__,line);
}

#endif
