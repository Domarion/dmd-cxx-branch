/*
 * Some portions copyright (c) 1984-1993 by Symantec
 * Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * http://www.digitalmars.com
 * Written by Walter Bright
 *
 * This source file is made available for personal use
 * only. The license is in backendlicense.txt
 * For any other uses, please contact Digital Mars.
 */

// Utility subroutines

#include        <stdio.h>
#include        <ctype.h>
#include        <string.h>
#include        <stdlib.h>
#include        <time.h>

#include        "cc.h"
#include        "global.h"
#include        "mem.h"
#include        "token.h"
#if MARS
#include        "el.h"
#endif

static char __file__[] = __FILE__;      /* for tassert.h                */
#include        "tassert.h"

void *ph_malloc(size_t nbytes);
void *ph_calloc(size_t nbytes);
void ph_free(void *p);
void *ph_realloc(void *p , size_t nbytes);


void util_exit(int exitcode);

void file_progress()
{
}

/*******************************
 * Alternative assert failure.
 */

void util_assert(const char *file, int line)
{
    fflush(stdout);
    printf("Internal error: %s %d\n",file,line);
    err_exit();
#if __clang__
    __builtin_unreachable();
#endif
}

/****************************
 * Clean up and exit program.
 */

void err_exit()
{
    util_exit(EXIT_FAILURE);
}

/********************************
 * Clean up and exit program.
 */

void err_break()
{
    util_exit(255);
}


/****************************
 * Clean up and exit program.
 */

void util_exit(int exitcode)
{
    exit(exitcode);                     /* terminate abnormally         */
}

#if __linux__
void util_progress()
{
}

void util_progress(int linnum)
{
}
#endif

/**********************************
 * Binary string search.
 * Input:
 *      p ->    string of characters
 *      tab     array of pointers to strings
 *      n =     number of pointers in the array
 * Returns:
 *      index (0..n-1) into tab[] if we found a string match
 *      else -1
 */

int binary(const char *p, const char ** table, int high)
{
    int low = 0;
    char cp = *p;
    high--;
    p++;

    while (low <= high)
    {
        int mid = (low + high) >> 1;
        int cond = table[mid][0] - cp;

        if (cond == 0)
            cond = strcmp(table[mid] + 1,p);
        if (cond > 0)
            high = mid - 1;
        else if (cond < 0)
            low = mid + 1;
        else
            return mid;                 /* match index                  */
    }
    return -1;
}

// search table[0 .. high] for p[0 .. len] (where p.length not necessairily equal to len)
int binary(const char *p, size_t len, const char ** table, int high)
{
    int low = 0;
    char cp = *p;
    high--;
    p++;
    len--;

    while (low <= high)
    {
        int mid = (low + high) >> 1;
        int cond = table[mid][0] - cp;

        if (cond == 0)
        {
            cond = strncmp(table[mid] + 1, p, len);
            if (cond == 0)
                cond = table[mid][len+1]; // same as: if (table[mid][len+1] != '\0') cond = 1;
        }

        if (cond > 0)
            high = mid - 1;
        else if (cond < 0)
            low = mid + 1;
        else
            return mid;                 /* match index                  */
    }
    return -1;
}

/**********************
 * If c is a power of 2, return that power else -1.
 */

int ispow2(unsigned long long c)
{       int i;

        if (c == 0 || (c & (c - 1)))
            i = -1;
        else
            for (i = 0; c >>= 1; i++)
                ;
        return i;
}

