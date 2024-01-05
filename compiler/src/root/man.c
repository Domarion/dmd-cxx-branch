
/* Copyright (C) 2008-2021 by The D Language Foundation, All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
 * https://github.com/D-Programming-Language/dmd/blob/master/src/root/man.c
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "object.h"

#if POSIX

#include        <sys/types.h>
#include        <sys/wait.h>
#include        <unistd.h>

void browse(const char *url)
{
    pid_t childpid;
    const char *args[3];

    const char *browser = getenv("BROWSER");
    if (browser)
        browser = strdup(browser);
    else
        browser = "x-www-browser";

    args[0] = browser;
    args[1] = url;
    args[2] = NULL;

    childpid = fork();
    if (childpid == 0)
    {
        execvp(args[0], (char**)args);
        perror(args[0]);                // failed to execute
        return;
    }
}

#endif


