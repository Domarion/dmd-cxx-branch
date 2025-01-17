// Copyright (C) 1990-1998 by Symantec
// Copyright (C) 2000-2021 by The D Language Foundation, All Rights Reserved
// http://www.digitalmars.com
// Written by Walter Bright
/*
 * This source file is made available for personal use
 * only. The license is in backendlicense.txt
 * For any other uses, please contact Digital Mars.
 */

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <utime.h>

#include "file.hpp"
#include "filename.hpp"

/*********************************
 * #include <stdlib.h>
 * int response_expand(int *pargc,char ***pargv);
 *
 * Expand any response files in command line.
 * Response files are arguments that look like:
 *   @NAME
 * The name is first searched for in the environment. If it is not
 * there, it is searched for as a file name.
 * Arguments are separated by spaces, tabs, or newlines. These can be
 * imbedded within arguments by enclosing the argument in '' or "".
 * Recursively expands nested response files.
 *
 * To use, put the line:
 *   response_expand(&argc,&argv);
 * as the first executable statement in main(int argc, char **argv).
 * argc and argv are adjusted to be the new command line arguments
 * after response file expansion.
 *
 * Digital Mars's MAKE program can be notified that a program can accept
 * long command lines via environment variables by preceding the rule
 * line for the program with a *.
 *
 * Returns:
 *   0   success
 *   !=0   failure (argc, argv unchanged)
 */

bool response_expand(Strings *args)
{
    const char *cp;
    int recurse = 0;

    for (size_t i = 0; i < args->length; )
    {
        cp = (*args)[i];
        if (*cp != '@')
        {
            ++i;
            continue;
        }

        args->remove(i);

        char *buffer;
        char *bufend;

        cp++;
        char *p = getenv(cp);
        if (p)
        {
            buffer = strdup(p);
            if (!buffer)
                goto noexpand;
            bufend = buffer + strlen(buffer);
        }
        else
        {
            File f(cp);
            if (f.read())
                goto noexpand;
            f.ref = 1;

            buffer = (char *)f.buffer;
            bufend = buffer + f.len;
        }

        // The logic of this should match that in setargv()

        int comment = 0;
        for (p = buffer; p < bufend; p++)
        {
            char *d;
            char c,lastc;
            unsigned char instring;
            int num_slashes,non_slashes;

            switch (*p)
            {
                case 26:      /* ^Z marks end of file      */
                    goto L2;

                case 0xD:
                case '\n':
                    if (comment)
                    {
                        comment = 0;
                    }
                case 0:
                case ' ':
                case '\t':
                    continue;   // scan to start of argument

                case '#':
                    comment = 1;
                    continue;

                case '@':
                    if (comment)
                    {
                        continue;
                    }
                    recurse = 1;

                /* fall through */
                default:      /* start of new argument   */
                    if (comment)
                    {
                        continue;
                    }
                    args->insert(i, p);
                    ++i;
                    instring = 0;
                    c = 0;
                    num_slashes = 0;
                    for (d = p; 1; p++)
                    {
                        lastc = c;
                        if (p >= bufend)
                        {
                            *d = 0;
                            goto L2;
                        }
                        c = *p;
                        switch (c)
                        {
                            case '"':
                                /*
                                    Yes this looks strange,but this is so that we are
                                    MS Compatible, tests have shown that:
                                    \\\\"foo bar"  gets passed as \\foo bar
                                    \\\\foo  gets passed as \\\\foo
                                    \\\"foo gets passed as \"foo
                                    and \"foo gets passed as "foo in VC!
                                 */
                                non_slashes = num_slashes % 2;
                                num_slashes = num_slashes / 2;
                                for (; num_slashes > 0; num_slashes--)
                                {
                                    d--;
                                    *d = '\0';
                                }

                                if (non_slashes)
                                {
                                    *(d-1) = c;
                                }
                                else
                                {
                                    instring ^= 1;
                                }
                                break;
                            case 26:
                                *d = 0;      // terminate argument
                                goto L2;

                            case 0xD:      // CR
                                c = lastc;
                                continue;      // ignore

                            case '@':
                                recurse = 1;
                                goto Ladd;

                            case ' ':
                            case '\t':
                                if (!instring)
                                {
                            case '\n':
                            case 0:
                                    *d = 0;      // terminate argument
                                    goto Lnextarg;
                                }

                            /* fall through */
                            default:
                            Ladd:
                                if (c == '\\')
                                    num_slashes++;
                                else
                                    num_slashes = 0;
                                *d++ = c;
                                break;
                        }
                    }
                break;
            }
        Lnextarg:
            ;
        }
    L2:
        ;
    }
    if (recurse)
    {
        /* Recursively expand @filename   */
        if (response_expand(args))
            goto noexpand;
    }
    return false;            /* success         */

noexpand:            /* error         */
    /* BUG: any file buffers are not free'd   */
    return true;
}
