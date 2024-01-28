
/* Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
 * https://github.com/D-Programming-Language/dmd/blob/master/src/root/filename.c
 */

#include "dsystem.hpp"
#include "filename.hpp"
#include "port.hpp"
#include "outbuffer.hpp"
#include "array.hpp"
#include "file.hpp"
#include "rmem.hpp"

#include <utime.h>

/****************************** FileName ********************************/

FileName::FileName(const char *str)
    : str(mem.xstrdup(str))
{
}

const char *FileName::combine(const char *path, const char *name)
{   char *f;
    size_t pathlen;
    size_t namelen;

    if (!path || !*path)
        return name;
    pathlen = strlen(path);
    namelen = strlen(name);
    f = (char *)mem.xmalloc(pathlen + 1 + namelen + 1);
    memcpy(f, path, pathlen);

    if (path[pathlen - 1] != '/')
    {   f[pathlen] = '/';
        pathlen++;
    }

    memcpy(f + pathlen, name, namelen + 1);
    return f;
}

// Split a path into an Array of paths
Strings *FileName::splitPath(const char *path)
{
    char c = 0;                         // unnecessary initializer is for VC /W4
    const char *p;
    OutBuffer buf;
    Strings *array;

    array = new Strings();
    if (path)
    {
        p = path;
        do
        {   char instring = 0;

            while (isspace((utf8_t)*p))         // skip leading whitespace
                p++;
            buf.reserve(strlen(p) + 1); // guess size of path
            for (; ; p++)
            {
                c = *p;
                switch (c)
                {
                    case '"':
                        instring ^= 1;  // toggle inside/outside of string
                        continue;

                    case ':':
                        p++;
                        break;          // note that ; cannot appear as part
                                        // of a path, quotes won't protect it

                    case 0x1A:          // ^Z means end of file
                    case 0:
                        break;

                    case '\r':
                        continue;       // ignore carriage returns

                    case '~':
                    {
                        char *home = getenv("HOME");
                        // Expand ~ only if it is prefixing the rest of the path.
                        if (!buf.length() && p[1] == '/' && home)
                            buf.writestring(home);
                        else
                            buf.writestring("~");
                        continue;
                    }

                    default:
                        buf.writeByte(c);
                        continue;
                }
                break;
            }
            if (buf.length())             // if path is not empty
            {
                array->push(buf.extractChars());
            }
        } while (c);
    }
    return array;
}

int FileName::compare(RootObject *obj)
{
    return compare(str, ((FileName *)obj)->str);
}

int FileName::compare(const char *name1, const char *name2)
{
    return strcmp(name1, name2);
}

bool FileName::equals(RootObject *obj)
{
    return compare(obj) == 0;
}

bool FileName::equals(const char *name1, const char *name2)
{
    return compare(name1, name2) == 0;
}

/************************************
 * Return !=0 if absolute path name.
 */

bool FileName::absolute(const char *name)
{
    return (*name == '/');
}

/**
Return the given name as an absolute path

Params:
    name = path
    base = the absolute base to prefix name with if it is relative

Returns: name as an absolute path relative to base
*/
const char *FileName::toAbsolute(const char *name, const char *base)
{
    return absolute(name) ? name : combine(base ? base : getcwd(nullptr, 0), name);
}

/********************************
 * Return filename extension (read-only).
 * Points past '.' of extension.
 * If there isn't one, return nullptr.
 */

const char *FileName::ext(const char *str)
{
    size_t len = strlen(str);

    const char *e = str + len;
    for (;;)
    {
        switch (*e)
        {   case '.':
                return e + 1;
            case '/':
                break;
            default:
                if (e == str)
                    break;
                e--;
                continue;
        }
        return nullptr;
    }
}

const char *FileName::ext()
{
    return ext(str);
}

/********************************
 * Return mem.xmalloc'd filename with extension removed.
 */

const char *FileName::removeExt(const char *str)
{
    const char *e = ext(str);
    if (e)
    {   size_t len = (e - str) - 1;
        char *n = (char *)mem.xmalloc(len + 1);
        memcpy(n, str, len);
        n[len] = 0;
        return n;
    }
    return mem.xstrdup(str);
}

/********************************
 * Return filename name excluding path (read-only).
 */

const char *FileName::name(const char *str)
{
    size_t len = strlen(str);

    const char *e = str + len;
    for (;;)
    {
        switch (*e)
        {
            case '/':
               return e + 1;
                /* falls through */
            default:
                if (e == str)
                    break;
                e--;
                continue;
        }
        return e;
    }
}

const char *FileName::name()
{
    return name(str);
}

/**************************************
 * Return path portion of str.
 * Path will does not include trailing path separator.
 */

const char *FileName::path(const char *str)
{
    const char *n = name(str);
    size_t pathlen;

    if (n > str)
    {
        if (n[-1] == '/')
            n--;
    }
    pathlen = n - str;
    char *path = (char *)mem.xmalloc(pathlen + 1);
    memcpy(path, str, pathlen);
    path[pathlen] = 0;
    return path;
}

/**************************************
 * Replace filename portion of path.
 */

const char *FileName::replaceName(const char *path, const char *name)
{
    size_t pathlen;
    size_t namelen;

    if (absolute(name))
        return name;

    const char *n = FileName::name(path);
    if (n == path)
        return name;
    pathlen = n - path;
    namelen = strlen(name);
    char *f = (char *)mem.xmalloc(pathlen + 1 + namelen + 1);
    memcpy(f, path, pathlen);
    if (path[pathlen - 1] != '/')
    {   f[pathlen] = '/';
        pathlen++;
    }
    memcpy(f + pathlen, name, namelen + 1);
    return f;
}

/***************************
 * Free returned value with FileName::free()
 */

const char *FileName::defaultExt(const char *name, const char *ext)
{
    const char *e = FileName::ext(name);
    if (e)                              // if already has an extension
        return mem.xstrdup(name);

    size_t len = strlen(name);
    size_t extlen = strlen(ext);
    char *s = (char *)mem.xmalloc(len + 1 + extlen + 1);
    memcpy(s,name,len);
    s[len] = '.';
    memcpy(s + len + 1, ext, extlen + 1);
    return s;
}

/***************************
 * Free returned value with FileName::free()
 */

const char *FileName::forceExt(const char *name, const char *ext)
{
    const char *e = FileName::ext(name);
    if (e)                              // if already has an extension
    {
        size_t len = e - name;
        size_t extlen = strlen(ext);

        char *s = (char *)mem.xmalloc(len + extlen + 1);
        memcpy(s,name,len);
        memcpy(s + len, ext, extlen + 1);
        return s;
    }
    else
        return defaultExt(name, ext);   // doesn't have one
}

/******************************
 * Return !=0 if extensions match.
 */

bool FileName::equalsExt(const char *ext)
{
    return equalsExt(str, ext);
}

bool FileName::equalsExt(const char *name, const char *ext)
{
    const char *e = FileName::ext(name);
    if (!e && !ext)
        return true;
    if (!e || !ext)
        return false;
    return FileName::compare(e, ext) == 0;
}

/*************************************
 * Search Path for file.
 * Input:
 *      cwd     if true, search current directory before searching path
 */

const char *FileName::searchPath(Strings *path, const char *name, bool cwd)
{
    if (absolute(name))
    {
        return exists(name) ? name : nullptr;
    }
    if (cwd)
    {
        if (exists(name))
            return name;
    }
    if (path)
    {

        for (size_t i = 0; i < path->length; i++)
        {
            const char *p = (*path)[i];
            const char *n = combine(p, name);

            if (exists(n))
                return n;
        }
    }
    return nullptr;
}


/*************************************
 * Search Path for file in a safe manner.
 *
 * Be wary of CWE-22: Improper Limitation of a Pathname to a Restricted Directory
 * ('Path Traversal') attacks.
 *      http://cwe.mitre.org/data/definitions/22.html
 * More info:
 *      https://www.securecoding.cert.org/confluence/display/c/FIO02-C.+Canonicalize+path+names+originating+from+tainted+sources
 * Returns:
 *      nullptr    file not found
 *      !=nullptr  mem.xmalloc'd file name
 */

const char *FileName::safeSearchPath(Strings *path, const char *name)
{
    /* Even with realpath(), we must check for // and disallow it
     */
    for (const char *p = name; *p; p++)
    {
        char c = *p;
        if (c == '/' && p[1] == '/')
        {
            return nullptr;
        }
    }

    if (path)
    {
        /* Each path is converted to a cannonical name and then a check is done to see
         * that the searched name is really a child one of the the paths searched.
         */
        for (size_t i = 0; i < path->length; i++)
        {
            const char *cname = nullptr;
            const char *cpath = canonicalName((*path)[i]);
            //printf("FileName::safeSearchPath(): name=%s; path=%s; cpath=%s\n",
            //      name, (char *)path->data[i], cpath);
            if (cpath == nullptr)
                goto cont;
            cname = canonicalName(combine(cpath, name));
            //printf("FileName::safeSearchPath(): cname=%s\n", cname);
            if (cname == nullptr)
                goto cont;
            //printf("FileName::safeSearchPath(): exists=%i "
            //      "strncmp(cpath, cname, %i)=%i\n", exists(cname),
            //      strlen(cpath), strncmp(cpath, cname, strlen(cpath)));
            // exists and name is *really* a "child" of path
            if (exists(cname) && strncmp(cpath, cname, strlen(cpath)) == 0)
            {
                ::free(const_cast<char *>(cpath));
                const char *p = mem.xstrdup(cname);
                ::free(const_cast<char *>(cname));
                return p;
            }
cont:
            if (cpath)
                ::free(const_cast<char *>(cpath));
            if (cname)
                ::free(const_cast<char *>(cname));
        }
    }
    return nullptr;
}


int FileName::exists(const char *name)
{
    struct stat st;

    if (stat(name, &st) < 0)
        return 0;
    if (S_ISDIR(st.st_mode))
        return 2;
    return 1;
}

bool FileName::ensurePathExists(const char *path)
{
    //printf("FileName::ensurePathExists(%s)\n", path ? path : "");
    if (path && *path)
    {
        if (!exists(path))
        {
            const char *p = FileName::path(path);
            if (*p)
            {
                bool r = ensurePathExists(p);
                mem.xfree(const_cast<char *>(p));
                if (r)
                    return r;
            }
            char sep = '/';
            if (path[strlen(path) - 1] != sep)
            {
                int r = mkdir(path, (7 << 6) | (7 << 3) | 7);

                if (r)
                {
                    /* Don't error out if another instance of dmd just created
                     * this directory
                     */
                    if (errno != EEXIST)
                        return true;
                }
            }
        }
    }
    return false;
}

/******************************************
 * Return canonical version of name in a malloc'd buffer.
 * This code is high risk.
 */
const char *FileName::canonicalName(const char *name)
{
    // nullptr destination buffer is allowed and preferred
    return realpath(name, nullptr);
}

/********************************
 * Free memory allocated by FileName routines
 */
void FileName::free(const char *str)
{
    if (str)
    {   assert(str[0] != (char)0xAB);
        memset(const_cast<char *>(str), 0xAB, strlen(str) + 1);     // stomp
    }
    mem.xfree(const_cast<char *>(str));
}

const char *FileName::toChars() const
{
    return str;
}
