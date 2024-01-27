/*_ mem.c       */
/* Memory management package    */
/* Written by Walter Bright     */

#include        <stdio.h>
#include        <sys/time.h>
#include        <sys/resource.h>
#include        <unistd.h>

#include        <stdarg.h>
#include        <stddef.h>

#include        <new>

#ifndef malloc
#include        <stdlib.h>
#endif

#ifndef MEM_H
#include        "mem.h"
#endif

#ifndef MEM_NOMEMCOUNT
#define MEM_NOMEMCOUNT  0
#endif

#if !MEM_NONE

#ifndef assert
#include        <assert.h>
#endif

#include <string.h>

int mem_inited = 0;             /* != 0 if initialized                  */

static int mem_behavior = MEM_ABORTMSG;
static int (*oom_fp)(void) = NULL;  /* out-of-memory handler                */
static int mem_count;           /* # of allocs that haven't been free'd */
static int mem_scount;          /* # of sallocs that haven't been free'd */

/* Determine where to send error messages       */
#define ferr    stderr
#define PRINT   fprintf(ferr,

/*******************************/

void mem_setexception(enum MEM_E flag,...)
{   va_list ap;
    typedef int (*fp_t)(void);

    mem_behavior = flag;
    va_start(ap,flag);
    oom_fp = (mem_behavior == MEM_CALLFP) ? va_arg(ap,fp_t) : 0;
    va_end(ap);
}

/*************************
 * This is called when we're out of memory.
 * Returns:
 *      1:      try again to allocate the memory
 *      0:      give up and return NULL
 */

int mem_exception()
{   int behavior;

    behavior = mem_behavior;
    while (1)
    {
        switch (behavior)
        {
            case MEM_ABORTMSG:
                PRINT "Fatal error: out of memory\n");
                /* FALL-THROUGH */
            case MEM_ABORT:
                exit(EXIT_FAILURE);
                /* NOTREACHED */
            case MEM_CALLFP:
                assert(oom_fp);
                behavior = (*oom_fp)();
                break;
            case MEM_RETNULL:
                return 0;
            case MEM_RETRY:
                return 1;
            default:
                assert(0);
        }
    }
}

/****************************/


char *mem_strdup(const char *s)
{
        if (s)
        {   size_t len = strlen(s) + 1;
            char *p = (char *) mem_malloc(len);
            if (p)
                return (char *)memcpy(p,s,len);
        }
        return NULL;
}

/************* C++ Implementation ***************/

#if !MEM_NONE
extern "C++"
{

/* Cause initialization and termination functions to be called  */

int __mem_line;
char *__mem_file;

/********************
 */

#if __GNUC__
int (*_new_handler)(void);
#else
void (*_new_handler)(void);
#endif

/*****************************
 * Replacement for the standard C++ library operator new().
 */

#if !MEM_NONEW

#if __GNUC__
void * operator new(size_t size)
#else
#undef new
void * __cdecl operator new(size_t size)
#endif
{   void *p;

    while (1)
    {
        if (size == 0)
            size++;
        p = mem_malloc((unsigned)size);
        if (p != NULL || _new_handler == NULL)
            break;
        (*_new_handler)();
    }
    return p;
}

#if __GNUC__
void * operator new[](size_t size)
#else
void * __cdecl operator new[](size_t size)
#endif
{   void *p;

    while (1)
    {
        if (size == 0)
            size++;
        p = mem_malloc((unsigned)size);
        if (p != NULL || _new_handler == NULL)
            break;
        (*_new_handler)();
    }
    return p;
}

/***********************
 * Replacement for the standard C++ library operator delete().
 */

#undef delete
void __cdecl operator delete(void *p)
{
        mem_free(p);
}

void __cdecl operator delete[](void *p)
{
        mem_free(p);
}
#endif
}
#endif

/***************************/

void *mem_malloc(size_t numbytes)
{       void *p;

        if (numbytes == 0)
                return NULL;
        while (1)
        {
                p = malloc(numbytes);
                if (p == NULL)
                {       if (mem_exception())
                                continue;
                }
#if !MEM_NOMEMCOUNT
                else
                        mem_count++;
#endif
                break;
        }
        /*printf("malloc(%d) = x%lx, mem_count = %d\n",numbytes,p,mem_count);*/
        return p;
}

/***************************/

void *mem_calloc(size_t numbytes)
{       void *p;

        if (numbytes == 0)
            return NULL;
        while (1)
        {
                p = calloc(numbytes,1);
                if (p == NULL)
                {       if (mem_exception())
                                continue;
                }
#if !MEM_NOMEMCOUNT
                else
                        mem_count++;
#endif
                break;
        }
        /*printf("calloc(%d) = x%lx, mem_count = %d\n",numbytes,p,mem_count);*/
        return p;
}

/***************************/

void *mem_realloc(void *oldmem_ptr,size_t newnumbytes)
{   void *p;

    if (oldmem_ptr == NULL)
        p = mem_malloc(newnumbytes);
    else if (newnumbytes == 0)
    {   mem_free(oldmem_ptr);
        p = NULL;
    }
    else
    {
        do
            p = realloc(oldmem_ptr,newnumbytes);
        while (p == NULL && mem_exception());
    }
    /*printf("realloc(x%lx,%d) = x%lx, mem_count = %d\n",oldmem_ptr,newnumbytes,p,mem_count);*/
    return p;
}

/***************************/

void mem_free(void *ptr)
{
    /*printf("free(x%lx) mem_count=%d\n",ptr,mem_count);*/
    if (ptr != NULL)
    {
#if !MEM_NOMEMCOUNT
        assert(mem_count != 0);
        mem_count--;
#endif
        free(ptr);
    }
}

/***************************/
/* This is our low-rent fast storage allocator  */

static char *heap;
static size_t heapleft;

/***************************/

void *mem_fmalloc(size_t numbytes)
{   void *p;

    //printf("fmalloc(%d)\n",numbytes);
#if defined(__GNUC__) || defined(__clang__)
    // GCC and Clang assume some types, notably elem (see DMD issue 6215),
    // to be 16-byte aligned. Because we do not have any type information
    // available here, we have to 16 byte-align everything.
    numbytes = (numbytes + 0xF) & ~0xF;
#else
    if (sizeof(size_t) == 2)
        numbytes = (numbytes + 1) & ~1;         /* word align   */
    else
        numbytes = (numbytes + 3) & ~3;         /* dword align  */
#endif

    /* This ugly flow-of-control is so that the most common case
       drops straight through.
     */

    if (!numbytes)
        return NULL;

    if (numbytes <= heapleft)
    {
     L2:
        p = (void *)heap;
        heap += numbytes;
        heapleft -= numbytes;
        return p;
    }

    heapleft = numbytes + 0x3C00;
    if (heapleft >= 16372)
        heapleft = numbytes;
L1:
    heap = (char *)malloc(heapleft);
    if (!heap)
    {   if (mem_exception())
            goto L1;
        return NULL;
    }
    goto L2;
}

/***************************/

void *mem_fcalloc(size_t numbytes)
{   void *p;

    p = mem_fmalloc(numbytes);
    return p ? memset(p,0,numbytes) : p;
}

/***************************/

char *mem_fstrdup(const char *s)
{
        if (s)
        {   size_t len = strlen(s) + 1;
            char *p = (char *) mem_fmalloc(len);
            if (p)
                return (char *)memcpy(p,s,len);
        }
        return NULL;
}

/***************************/

void mem_init()
{
        if (mem_inited == 0)
        {       mem_count = 0;
                mem_scount = 0;
                oom_fp = NULL;
                mem_behavior = MEM_ABORTMSG;
        }
        mem_inited++;
}

/***************************/

void mem_term()
{
        if (mem_inited)
        {
                if (mem_count)
                        PRINT "%d unfreed items\n",mem_count);
                if (mem_scount)
                        PRINT "%d unfreed s items\n",mem_scount);
                assert(mem_count == 0 && mem_scount == 0);
        }
        mem_inited = 0;
}

#endif /* !MEM_NONE */
