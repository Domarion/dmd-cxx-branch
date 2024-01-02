
/* Compiler implementation of the D programming language
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/dlang/dmd/blob/master/src/dmd/root/dsystem.h
 */

#pragma once

// Get common system includes from the host.

#define POSIX (__linux__ || __GLIBC__)

#define __C99FEATURES__ 1       // Needed on Solaris for NaN and more
#define __USE_ISOC99 1          // so signbit() gets defined

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <new>

#if POSIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

// For getcwd()
#if POSIX
#include <unistd.h>
#endif

