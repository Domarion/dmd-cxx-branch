/*
 * Some portions copyright (c) 1994-1995 by Symantec
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * http://www.digitalmars.com
 * Written by Walter Bright
 *
 * This source file is made available for personal use
 * only. The license is in backendlicense.txt
 * For any other uses, please contact Digital Mars.
 */

#include        <stdio.h>
#include        <string.h>
#include        <stdlib.h>
#include        <ctype.h>

#include        "root.hpp"
#include        "rmem.hpp"
#include        "port.hpp"
#include        "stringtable.hpp"

char *skipspace(char *p);

/*****************************
 * Find the config file
 * Input:
 *      argv0           program name (argv[0])
 *      inifile         .ini file name
 * Returns:
 *      file path of the config file or nullptr
 *      Note: this is a memory leak
 */
const char *findConfFile(const char *argv0, const char *inifile)
{
    if (FileName::absolute(inifile))
        return inifile;
    if (FileName::exists(inifile))
        return inifile;

    /* Look for inifile in the following sequence of places:
     *      o current directory
     *      o home directory
     *      o exe directory (windows)
     *      o directory off of argv0
     *      o SYSCONFDIR (default=/etc/) (non-windows)
     */
    const char *filename = FileName::combine(getenv("HOME"), inifile);
    if (FileName::exists(filename))
        return filename;

    filename = FileName::replaceName(argv0, inifile);
    if (FileName::exists(filename))
        return filename;

    // Search PATH for argv0
    const char *p = getenv("PATH");
    Strings *paths = FileName::splitPath(p);
    const char *abspath = FileName::searchPath(paths, argv0, false);
    if (abspath)
    {
        const char *absname = FileName::replaceName(abspath, inifile);
        if (FileName::exists(absname))
            return absname;
    }

    // Resolve symbolic links
    filename = FileName::canonicalName(abspath ? abspath : argv0);
    if (filename)
    {
        filename = FileName::replaceName(filename, inifile);
        if (FileName::exists(filename))
            return filename;
    }

    // Search /etc/ for inifile
#ifndef SYSCONFDIR
# error SYSCONFDIR not defined
#endif
    assert(SYSCONFDIR != nullptr && strlen(SYSCONFDIR));
    filename = FileName::combine(SYSCONFDIR, inifile);

    return filename;
}

/**********************************
 * Read from environment, looking for cached value first.
 */

const char *readFromEnv(StringTable *environment, const char *name)
{
    size_t len = strlen(name);
    StringValue *sv = environment->lookup(name, len);
    if (sv)
        return (const char *)sv->ptrvalue;   // get cached value

    return getenv(name);
}

/*********************************
 * Write to our copy of the environment, not the real environment
 */

static void writeToEnv(StringTable *environment, char *nameEqValue)
{
    char *p = strchr(nameEqValue, '=');
    assert(p);
    StringValue *sv = environment->update(nameEqValue, p - nameEqValue);
    sv->ptrvalue = (void *)(p + 1);
}

/************************************
 * Update real enviroment with our copy.
 */

static int envput(StringValue *sv)
{
    const char *name = sv->toDchars();
    size_t namelen = strlen(name);
    const char *value = (const char *)sv->ptrvalue;
    size_t valuelen = strlen(value);
    char *s = (char *)malloc(namelen + 1 + valuelen + 1);
    assert(s);
    memcpy(s, name, namelen);
    s[namelen] = '=';
    memcpy(s + namelen + 1, value, valuelen);
    s[namelen + 1 + valuelen] = 0;
    //printf("envput('%s')\n", s);
    putenv(s);

    return 0;           // do all of them
}

void updateRealEnvironment(StringTable *environment)
{
    environment->apply(&envput);
}

/*****************************
 * Read and analyze .ini file.
 * Write the entries into environment as
 * well as any entries in one of the specified section(s).
 *
 * Params:
 *      environment = our own cache of the program environment
 *      path = what @P will expand to
 *      buffer[len] = contents of configuration file
 *      sections[] = section namesdimension of array of section names
 */
void parseConfFile(StringTable *environment, const char *path, size_t length, unsigned char *buffer, Strings *sections)
{
    // Parse into lines
    bool envsection = true;     // default is to read

    OutBuffer buf;
    bool eof = false;
    for (size_t i = 0; i < length && !eof; i++)
    {
    Lstart:
        size_t linestart = i;

        for (; i < length; i++)
        {
            switch (buffer[i])
            {
                case '\r':
                    break;

                case '\n':
                    // Skip if it was preceded by '\r'
                    if (i && buffer[i - 1] == '\r')
                    {
                        i++;
                        goto Lstart;
                    }
                    break;

                case 0:
                case 0x1A:
                    eof = true;
                    break;

                default:
                    continue;
            }
            break;
        }

        buf.reset();

        // First, expand the macros.
        // Macros are bracketed by % characters.

        for (size_t k = 0; k < i - linestart; k++)
        {
            // The line is buffer[linestart..i]
            char *line = (char *)&buffer[linestart];
            if (line[k] == '%')
            {
                for (size_t j = k + 1; j < i - linestart; j++)
                {
                    if (line[j] != '%')
                        continue;

                    if (j - k == 3 && Port::memicmp(&line[k + 1], "@P", 2) == 0)
                    {
                        // %@P% is special meaning the path to the .ini file
                        const char *p = path;
                        if (!*p)
                            p = ".";
                        buf.writestring(p);
                    }
                    else
                    {
                        size_t len2 = j - k;
                        char *p = (char *)malloc(len2);
                        len2--;
                        memcpy(p, &line[k + 1], len2);
                        p[len2] = 0;
                        Port::strupr(p);
                        const char *penv = readFromEnv(environment, p);
                        if (penv)
                            buf.writestring(penv);
                        free(p);
                    }
                    k = j;
                    goto L1;
                }
            }
            buf.writeByte(line[k]);
         L1:
            ;
        }

        // Remove trailing spaces
        unsigned char *bufptr = buf.slice().ptr;
        size_t buflen = buf.length();
        while (buflen && isspace(bufptr[buflen - 1]))
            buflen--;
        buf.setsize(buflen);

        char *p = buf.peekChars();

        // The expanded line is in p.
        // Now parse it for meaning.
        p = skipspace(p);
        switch (*p)
        {
            case ';':           // comment
            case 0:             // blank
                break;

            case '[':           // look for [Environment]
            {
                p = skipspace(p + 1);
                char *pn;
                for (pn = p; isalnum((utf8_t)*pn); pn++)
                    ;

                if (*skipspace(pn) != ']')
                {
                    // malformed [sectionname], so just say we're not in a section
                    envsection = false;
                    break;
                }

                /* Seach sectionnamev[] for p..pn and set envsection to true if it's there
                 */
                for (size_t j = 0; 1; ++j)
                {
                    if (j == sections->length)
                    {
                        // Didn't find it
                        envsection = false;
                        break;
                    }
                    const char *sectionname = (*sections)[j];
                    size_t len = strlen(sectionname);
                    if ((size_t)(pn - p) == len &&
                        Port::memicmp(p, sectionname, len) == 0)
                    {
                        envsection = true;
                        break;
                    }
                }
                break;
            }
            default:
                if (envsection)
                {
                    char *pn = p;

                    // Convert name to upper case;
                    // remove spaces bracketing =
                    for (p = pn; *p; p++)
                    {
                        if (islower((utf8_t)*p))
                            *p &= ~0x20;
                        else if (isspace((utf8_t)*p))
                        {
                            memmove(p, p + 1, strlen(p));
                            p--;
                        }
                        else if (p[0] == '?' && p[1] == '=')
                        {
                            *p = '\0';
                            if (readFromEnv(environment, pn))
                            {
                                pn = nullptr;
                                break;
                            }
                            // remove the '?' and resume parsing starting from
                            // '=' again so the regular variable format is
                            // parsed
                            memmove(p, p + 1, strlen(p + 1) + 1);
                            p--;
                        }
                        else if (*p == '=')
                        {
                            p++;
                            while (isspace((utf8_t)*p))
                                memmove(p, p + 1, strlen(p));
                            break;
                        }
                    }

                    if (pn)
                    {
                        writeToEnv(environment, strdup(pn));
                    }
                }
                break;
        }
    }
}

/********************
 * Skip spaces.
 */

char *skipspace(char *p)
{
    while (isspace((utf8_t)*p))
        p++;
    return p;
}
