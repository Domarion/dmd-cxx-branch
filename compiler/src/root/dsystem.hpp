
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

#define __USE_ISOC99 1          // so signbit() gets defined

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

#include <cassert>
#include <ctype.h>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <ctime>

#include <new>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h> // getcwd()


