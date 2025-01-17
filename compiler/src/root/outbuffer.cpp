
/* Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
 * https://github.com/D-Programming-Language/dmd/blob/master/src/root/outbuffer.c
 */

#include "dsystem.hpp"
#include "outbuffer.hpp"
#include "object.hpp"

char *OutBuffer::extractData()
{
    char *p;

    p = (char *)data.ptr;
    data = DArray<unsigned char>();
    offset = 0;
    return p;
}

void OutBuffer::reserve(size_t nbytes)
{
    if (data.length - offset < nbytes)
    {
        data.length = (offset + nbytes) * 2;
        data.length = (data.length + 15) & ~15;
        data.ptr = (unsigned char *)mem.xrealloc(data.ptr, data.length);
    }
}

void OutBuffer::reset()
{
    offset = 0;
}

void OutBuffer::setsize(size_t size)
{
    offset = size;
}

void OutBuffer::write(const void *data, size_t nbytes)
{
    if (doindent && !notlinehead)
    {
        if (level)
        {
            reserve(level);
            for (int i = 0; i < level; i++)
            {
                this->data.ptr[offset] = '\t';
                offset++;
            }
        }
        notlinehead = 1;
    }
    reserve(nbytes);
    memcpy(this->data.ptr + offset, data, nbytes);
    offset += nbytes;
}

void OutBuffer::writestring(const char *string)
{
    write(string,strlen(string));
}

void OutBuffer::prependstring(const char *string)
{
    size_t len = strlen(string);
    reserve(len);
    memmove(data.ptr + len, data.ptr, offset);
    memcpy(data.ptr, string, len);
    offset += len;
}

void OutBuffer::writenl()
{
    writeByte('\n');
    if (doindent)
        notlinehead = 0;
}

void OutBuffer::writeByte(unsigned b)
{
    if (doindent && !notlinehead
        && b != '\n')
    {
        if (level)
        {
            reserve(level);
            for (int i = 0; i < level; i++)
            {
                this->data.ptr[offset] = '\t';
                offset++;
            }
        }
        notlinehead = 1;
    }
    reserve(1);
    this->data.ptr[offset] = (unsigned char)b;
    offset++;
}

void OutBuffer::writeUTF8(unsigned b)
{
    reserve(6);
    if (b <= 0x7F)
    {
        this->data.ptr[offset] = (unsigned char)b;
        offset++;
    }
    else if (b <= 0x7FF)
    {
        this->data.ptr[offset + 0] = (unsigned char)((b >> 6) | 0xC0);
        this->data.ptr[offset + 1] = (unsigned char)((b & 0x3F) | 0x80);
        offset += 2;
    }
    else if (b <= 0xFFFF)
    {
        this->data.ptr[offset + 0] = (unsigned char)((b >> 12) | 0xE0);
        this->data.ptr[offset + 1] = (unsigned char)(((b >> 6) & 0x3F) | 0x80);
        this->data.ptr[offset + 2] = (unsigned char)((b & 0x3F) | 0x80);
        offset += 3;
    }
    else if (b <= 0x1FFFFF)
    {
        this->data.ptr[offset + 0] = (unsigned char)((b >> 18) | 0xF0);
        this->data.ptr[offset + 1] = (unsigned char)(((b >> 12) & 0x3F) | 0x80);
        this->data.ptr[offset + 2] = (unsigned char)(((b >> 6) & 0x3F) | 0x80);
        this->data.ptr[offset + 3] = (unsigned char)((b & 0x3F) | 0x80);
        offset += 4;
    }
    else if (b <= 0x3FFFFFF)
    {
        this->data.ptr[offset + 0] = (unsigned char)((b >> 24) | 0xF8);
        this->data.ptr[offset + 1] = (unsigned char)(((b >> 18) & 0x3F) | 0x80);
        this->data.ptr[offset + 2] = (unsigned char)(((b >> 12) & 0x3F) | 0x80);
        this->data.ptr[offset + 3] = (unsigned char)(((b >> 6) & 0x3F) | 0x80);
        this->data.ptr[offset + 4] = (unsigned char)((b & 0x3F) | 0x80);
        offset += 5;
    }
    else if (b <= 0x7FFFFFFF)
    {
        this->data.ptr[offset + 0] = (unsigned char)((b >> 30) | 0xFC);
        this->data.ptr[offset + 1] = (unsigned char)(((b >> 24) & 0x3F) | 0x80);
        this->data.ptr[offset + 2] = (unsigned char)(((b >> 18) & 0x3F) | 0x80);
        this->data.ptr[offset + 3] = (unsigned char)(((b >> 12) & 0x3F) | 0x80);
        this->data.ptr[offset + 4] = (unsigned char)(((b >> 6) & 0x3F) | 0x80);
        this->data.ptr[offset + 5] = (unsigned char)((b & 0x3F) | 0x80);
        offset += 6;
    }
    else
        assert(0);
}

void OutBuffer::prependbyte(unsigned b)
{
    reserve(1);
    memmove(data.ptr + 1, data.ptr, offset);
    data.ptr[0] = (unsigned char)b;
    offset++;
}

void OutBuffer::writewchar(unsigned w)
{
    write4(w);
}

void OutBuffer::writeword(unsigned w)
{
    unsigned newline = '\n';

    if (doindent && !notlinehead
        && w != newline)
    {
        if (level)
        {
            reserve(level);
            for (int i = 0; i < level; i++)
            {
                this->data.ptr[offset] = '\t';
                offset++;
            }
        }
        notlinehead = 1;
    }
    reserve(2);
    *(unsigned short *)(this->data.ptr + offset) = (unsigned short)w;
    offset += 2;
}

void OutBuffer::writeUTF16(unsigned w)
{
    reserve(4);
    if (w <= 0xFFFF)
    {
        *(unsigned short *)(this->data.ptr + offset) = (unsigned short)w;
        offset += 2;
    }
    else if (w <= 0x10FFFF)
    {
        *(unsigned short *)(this->data.ptr + offset) = (unsigned short)((w >> 10) + 0xD7C0);
        *(unsigned short *)(this->data.ptr + offset + 2) = (unsigned short)((w & 0x3FF) | 0xDC00);
        offset += 4;
    }
    else
        assert(0);
}

void OutBuffer::write4(unsigned w)
{
    bool notnewline = true;
    if (doindent && !notlinehead && notnewline)
    {
        if (level)
        {
            reserve(level);
            for (int i = 0; i < level; i++)
            {
                this->data.ptr[offset] = '\t';
                offset++;
            }
        }
        notlinehead = 1;
    }
    reserve(4);
    *(unsigned *)(this->data.ptr + offset) = w;
    offset += 4;
}

void OutBuffer::write(OutBuffer *buf)
{
    if (buf)
    {   reserve(buf->offset);
        memcpy(data.ptr + offset, buf->data.ptr, buf->offset);
        offset += buf->offset;
    }
}

void OutBuffer::write(RootObject *obj)
{
    if (obj)
    {
        writestring(obj->toChars());
    }
}

void OutBuffer::fill0(size_t nbytes)
{
    reserve(nbytes);
    memset(data.ptr + offset,0,nbytes);
    offset += nbytes;
}

void OutBuffer::vprintf(const char *format, va_list args)
{
    int count;

    if (doindent)
        write(nullptr, 0); // perform indent
    int psize = 128;
    for (;;)
    {
        reserve(psize);
        va_list va;
        va_copy(va, args);
/*
  The functions vprintf(), vfprintf(), vsprintf(), vsnprintf()
  are equivalent to the functions printf(), fprintf(), sprintf(),
  snprintf(), respectively, except that they are called with a
  va_list instead of a variable number of arguments. These
  functions do not call the va_end macro. Consequently, the value
  of ap is undefined after the call. The application should call
  va_end(ap) itself afterwards.
 */
        count = vsnprintf((char *)data.ptr + offset,psize,format,va);
        va_end(va);
        if (count == -1)
            psize *= 2;
        else if (count >= psize)
            psize = count + 1;
        else
            break;
    }
    offset += count;
}

void OutBuffer::printf(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vprintf(format,ap);
    va_end(ap);
}

/**************************************
 * Convert `u` to a string and append it to the buffer.
 * Params:
 *  u = integral value to append
 */
void OutBuffer::print(unsigned long long u)
{
    unsigned long long value = u;
    char buf[20];
    const unsigned radix = 10;

    size_t i = sizeof(buf);
    do
    {
        if (value < radix)
        {
            unsigned char x = (unsigned char)value;
            buf[--i] = (char)(x + '0');
            break;
        }
        else
        {
            unsigned char x = (unsigned char)(value % radix);
            value = value / radix;
            buf[--i] = (char)(x + '0');
        }
    } while (value);

    write(buf + i, sizeof(buf) - i);
}

void OutBuffer::bracket(char left, char right)
{
    reserve(2);
    memmove(data.ptr + 1, data.ptr, offset);
    data.ptr[0] = left;
    data.ptr[offset + 1] = right;
    offset += 2;
}

/******************
 * Insert left at i, and right at j.
 * Return index just past right.
 */

size_t OutBuffer::bracket(size_t i, const char *left, size_t j, const char *right)
{
    size_t leftlen = strlen(left);
    size_t rightlen = strlen(right);
    reserve(leftlen + rightlen);
    insert(i, left, leftlen);
    insert(j + leftlen, right, rightlen);
    return j + leftlen + rightlen;
}

void OutBuffer::spread(size_t offset, size_t nbytes)
{
    reserve(nbytes);
    memmove(data.ptr + offset + nbytes, data.ptr + offset,
        this->offset - offset);
    this->offset += nbytes;
}

/****************************************
 * Returns: offset + nbytes
 */

size_t OutBuffer::insert(size_t offset, const void *p, size_t nbytes)
{
    spread(offset, nbytes);
    memmove(data.ptr + offset, p, nbytes);
    return offset + nbytes;
}

void OutBuffer::remove(size_t offset, size_t nbytes)
{
    memmove(data.ptr + offset, data.ptr + offset + nbytes, this->offset - (offset + nbytes));
    this->offset -= nbytes;
}

char *OutBuffer::peekChars()
{
    if (!offset || data.ptr[offset-1] != '\0')
    {
        writeByte(0);
        offset--; // allow appending more
    }
    return (char *)data.ptr;
}

char *OutBuffer::extractChars()
{
    if (!offset || data.ptr[offset-1] != '\0')
        writeByte(0);
    return extractData();
}
