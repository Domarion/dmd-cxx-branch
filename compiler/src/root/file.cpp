
/* Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
 * https://github.com/D-Programming-Language/dmd/blob/master/src/root/file.c
 */

#include "dsystem.hpp"
#include "file.hpp"

#include <utime.h>

#include "filename.hpp"
#include "array.hpp"
#include "rmem.hpp"

/****************************** File ********************************/

File::File(const FileName *n)
{
    ref = 0;
    buffer = nullptr;
    len = 0;
    name = const_cast<FileName *>(n);
}

File *File::create(const char *n)
{
    return new File(n);
}

File::File(const char *n)
{
    ref = 0;
    buffer = nullptr;
    len = 0;
    name = new FileName(n);
}

File::~File()
{
    if (buffer)
    {
        if (ref == 0)
            mem.xfree(buffer);
    }
}

/*************************************
 */

bool File::read()
{
    if (len)
        return false;               // already read the file
    size_t size;
    struct stat buf;
    ssize_t numread;

    const char *name = this->name->toChars();
    //printf("File::read('%s')\n",name);
    int fd = open(name, O_RDONLY);
    if (fd == -1)
    {
        //printf("\topen error, errno = %d\n",errno);
        goto err1;
    }

    if (!ref)
        ::free(buffer);
    ref = 0;       // we own the buffer now

    //printf("\tfile opened\n");
    if (fstat(fd, &buf))
    {
        printf("\tfstat error, errno = %d\n",errno);
        goto err2;
    }
    size = (size_t)buf.st_size;
    buffer = (unsigned char *) ::malloc(size + 2);

    if (!buffer)
    {
        printf("\tmalloc error, errno = %d\n",errno);
        goto err2;
    }

    numread = ::read(fd, buffer, size);
    if (numread != (ssize_t)size)
    {
        printf("\tread error, errno = %d\n",errno);
        goto err2;
    }

    if (close(fd) == -1)
    {
        printf("\tclose error, errno = %d\n",errno);
        goto err;
    }

    len = size;

    // Always store a wchar ^Z past end of buffer so scanner has a sentinel
    buffer[size] = 0;           // ^Z is obsolete, use 0
    buffer[size + 1] = 0;
    return false;

err2:
    close(fd);
err:
    ::free(buffer);
    buffer = nullptr;
    len = 0;

err1:
    return true;
}

/*********************************************
 * Write a file.
 * Returns:
 *      false       success
 */

bool File::write()
{
    ssize_t numwritten;

    const char *name = this->name->toChars();
    int fd = open(name, O_CREAT | O_WRONLY | O_TRUNC, (6 << 6) | (4 << 3) | 4);
    if (fd == -1)
        goto err;

    numwritten = ::write(fd, buffer, len);
    if ((ssize_t)len != numwritten)
        goto err2;

    if (close(fd) == -1)
        goto err;

    return false;

err2:
    close(fd);
    ::remove(name);
err:
    return true;
}

void File::remove()
{
    ::remove(this->name->toChars());
}

const char *File::toChars()
{
    return name->toChars();
}
