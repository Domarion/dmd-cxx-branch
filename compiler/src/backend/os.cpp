// Copyright (C) 1994-1998 by Symantec
// Copyright (C) 2000-2021 by The D Language Foundation, All Rights Reserved
// http://www.digitalmars.com
// Written by Walter Bright
/*
 * This source file is made available for personal use
 * only. The license is in backendlicense.txt
 * For any other uses, please contact Digital Mars.
 */

/*
 * Operating system specific routines.
 * Placed here to avoid cluttering
 * up code with OS .h files.
 */

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#define GetLastError() errno


#if __GNUC__
static char __file__[] = __FILE__;      /* for tassert.h                */
#include        "tassert.hpp"
#else
#include        <assert.h>
#endif

#define dbg_printf printf

int file_createdirs(char *name);

/***********************************
 * Called when there is an error returned by the operating system.
 * This function does not return.
 */

void os_error(int line)
{
    local_assert(line);
}

#undef dbg_printf
#define dbg_printf      (void)

#define os_error() os_error(__LINE__)

/*******************************************
 * Return !=0 if file exists.
 *      0:      file doesn't exist
 *      1:      normal file
 *      2:      directory
 */

int os_file_exists(const char *name)
{
    struct stat buf;

    return stat(name,&buf) == 0;        /* file exists if stat succeeded */
}

/**************************************
 * Get file size of open file. Return -1L on error.
 */

long os_file_size(int fd)
{
    struct stat buf;

    return (fstat(fd,&buf)) ? -1L : buf.st_size;
}

/**********************************************
 * Write a file.
 * Returns:
 *      0       success
 */

int file_write(char *name, void *buffer, unsigned len)
{
    int fd;
    ssize_t numwritten;

    fd = open(name, O_CREAT | O_WRONLY | O_TRUNC,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd == -1)
        goto err;

    numwritten = ::write(fd, buffer, len);
    if (len != numwritten)
        goto err2;

    if (close(fd) == -1)
        goto err;

    return 0;

err2:
    close(fd);
err:
    return 1;
}

/********************************
 * Create directories up to filename.
 * Input:
 *      name    path/filename
 * Returns:
 *      0       success
 *      !=0     failure
 */

int file_createdirs(char *)
{
    return 1;
}
