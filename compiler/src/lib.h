
/* Compiler implementation of the D programming language
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/D-Programming-Language/dmd/blob/master/src/lib.h
 */

#pragma once

Library *LibElf_factory();

class Library
{
  public:
    static Library *factory()
    {
#if TARGET_LINUX
        return LibElf_factory();
#else
        assert(0); // unsupported system
#endif
    }

    virtual void setFilename(const char *dir, const char *filename) = 0;
    virtual void addObject(const char *module_name, void *buf, size_t buflen) = 0;
    virtual void addLibrary(void *buf, size_t buflen) = 0;
    virtual void write() = 0;
};
