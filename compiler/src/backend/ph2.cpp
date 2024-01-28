// Compiler implementation of the D programming language
// Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
// written by Walter Bright
// http://www.digitalmars.com
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// https://github.com/D-Programming-Language/dmd/blob/master/src/backend/ph2.c

#include        <stdio.h>
#include        <time.h>
#include        <string.h>
#include        <stdlib.h>

#include        <new>
#include        "cc.hpp"
#include        "global.hpp"

static char __file__[] = __FILE__;      /* for tassert.h                */
#include        "tassert.hpp"

/**********************************************
 * Do our own storage allocator, a replacement
 * for malloc/free.
 */

struct Heap
{
    Heap *prev;         // previous heap
    unsigned char *buf; // buffer
    unsigned char *p;   // high water mark
    unsigned nleft;     // number of bytes left
};

Heap *heap=nullptr;

void ph_init()
{
    if (!heap) {
        heap = (Heap *)calloc(1,sizeof(Heap));
    }
    assert(heap);
}



void ph_term()
{
    //printf("ph_term()\n");
#if DEBUG
    Heap *h;
    Heap *hprev;

    for (h = heap; h; h = hprev)
    {
        hprev = h->prev;
        free(h->buf);
        free(h);
    }
#endif
}

void ph_newheap(size_t nbytes)
{   unsigned newsize;
    Heap *h;

    h = (Heap *) malloc(sizeof(Heap));
    if (!h)
        err_nomem();

    newsize = (nbytes > 0xFF00) ? nbytes : 0xFF00;
    h->buf = (unsigned char *) malloc(newsize);
    if (!h->buf)
    {
        free(h);
        err_nomem();
    }
    h->nleft = newsize;
    h->p = h->buf;
    h->prev = heap;
    heap = h;
}

void *ph_malloc(size_t nbytes)
{   unsigned char *p;

#ifdef DEBUG
    util_progress();
#endif
    nbytes += sizeof(unsigned) * 2;
    nbytes &= ~(sizeof(unsigned) - 1);

    if (nbytes >= heap->nleft)
        ph_newheap(nbytes);
    p = heap->p;
    heap->p += nbytes;
    heap->nleft -= nbytes;
    *(unsigned *)p = nbytes - sizeof(unsigned);
    p += sizeof(unsigned);
    return p;
}

#if ASM86
__declspec(naked) void *ph_calloc(size_t nbytes)
{
    _asm
    {
        push    dword ptr 4[ESP]
        call    ph_malloc
        test    EAX,EAX
        je      L25
        push    dword ptr 4[ESP]
        push    0
        push    EAX
        call    memset
        add     ESP,0Ch
L25:    ret     4
    }
}
#else
void *ph_calloc(size_t nbytes)
{   void *p;

    p = ph_malloc(nbytes);
    return p ? memset(p,0,nbytes) : p;
}
#endif

void ph_free(void *p)
{
}

void *ph_realloc(void *p,size_t nbytes)
{
    //dbg_printf("ph_realloc(%p,%d)\n",p,nbytes);
    if (!p)
        return ph_malloc(nbytes);
    if (!nbytes)
    {   ph_free(p);
        return nullptr;
    }
    void *newp = ph_malloc(nbytes);
    if (newp)
    {   unsigned oldsize = ((unsigned *)p)[-1];
        memcpy(newp,p,oldsize);
        ph_free(p);
    }
    return newp;
}

void err_nomem()
{
    printf("Error: out of memory\n");
    err_exit();
}
