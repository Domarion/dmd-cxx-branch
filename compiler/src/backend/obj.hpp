// Copyright (C) 2012-2021 by The D Language Foundation, All Rights Reserved
// http://www.digitalmars.com
// Written by Walter Bright
/*
 * This source file is made available for personal use
 * only. The license is in backendlicense.txt
 * For any other uses, please contact Digital Mars.
 */

/* Interface to object file format
 */

#pragma once

struct seg_data;

#define VIRTUAL static

struct Obj
{
    static Obj *init(Outbuffer *, const char *filename, const char *csegname);
    VIRTUAL void initfile(const char *filename, const char *csegname, const char *modname);
    VIRTUAL void termfile();
    VIRTUAL void term(const char *objfilename);

    VIRTUAL size_t mangle(Symbol *s,char *dest);
    VIRTUAL void import(elem *e);
    VIRTUAL void linnum(Srcpos srcpos, targ_size_t offset);
    VIRTUAL int codeseg(char *name,int suffix);
    VIRTUAL void dosseg(void);
    VIRTUAL void startaddress(Symbol *);
    VIRTUAL bool includelib(const char *);
    VIRTUAL bool allowZeroSize();
    VIRTUAL void exestr(const char *p);
    VIRTUAL void user(const char *p);
    VIRTUAL void compiler();
    VIRTUAL void wkext(Symbol *,Symbol *);
    VIRTUAL void lzext(Symbol *,Symbol *);
    VIRTUAL void alias(const char *n1,const char *n2);
    VIRTUAL void theadr(const char *modname);
    VIRTUAL void segment_group(targ_size_t codesize, targ_size_t datasize, targ_size_t cdatasize, targ_size_t udatasize);
    VIRTUAL void staticctor(Symbol *s,int dtor,int seg);
    VIRTUAL void staticdtor(Symbol *s);
    VIRTUAL void funcptr(Symbol *s);
    VIRTUAL void ehtables(Symbol *sfunc,targ_size_t size,Symbol *ehsym);
    VIRTUAL void ehsections();
    VIRTUAL void moduleinfo(Symbol *scc);
    virtual int  comdat(Symbol *);
    virtual int  comdatsize(Symbol *, targ_size_t symsize);
    VIRTUAL void setcodeseg(int seg);
    virtual seg_data *tlsseg();
    virtual seg_data *tlsseg_bss();
    VIRTUAL seg_data *tlsseg_data();
    static int  fardata(char *name, targ_size_t size, targ_size_t *poffset);
    VIRTUAL void export_symbol(Symbol *s, unsigned argsize);
    VIRTUAL void pubdef(int seg, Symbol *s, targ_size_t offset);
    VIRTUAL void pubdefsize(int seg, Symbol *s, targ_size_t offset, targ_size_t symsize);
    VIRTUAL int external_def(const char *);
    VIRTUAL int data_start(Symbol *sdata, targ_size_t datasize, int seg);
    VIRTUAL int external(Symbol *);
    VIRTUAL int common_block(Symbol *s, targ_size_t size, targ_size_t count);
    VIRTUAL int common_block(Symbol *s, int flag, targ_size_t size, targ_size_t count);
    VIRTUAL void lidata(int seg, targ_size_t offset, targ_size_t count);
    VIRTUAL void write_zeros(seg_data *pseg, targ_size_t count);
    VIRTUAL void write_byte(seg_data *pseg, unsigned byte);
    VIRTUAL void write_bytes(seg_data *pseg, unsigned nbytes, void *p);
    VIRTUAL void byte(int seg, targ_size_t offset, unsigned byte);
    VIRTUAL unsigned bytes(int seg, targ_size_t offset, unsigned nbytes, void *p);
    VIRTUAL void ledata(int seg, targ_size_t offset, targ_size_t data, unsigned lcfd, unsigned idx1, unsigned idx2);
    VIRTUAL void write_long(int seg, targ_size_t offset, unsigned long data, unsigned lcfd, unsigned idx1, unsigned idx2);
    VIRTUAL void reftodatseg(int seg, targ_size_t offset, targ_size_t val, unsigned targetdatum, int flags);
    VIRTUAL void reftofarseg(int seg, targ_size_t offset, targ_size_t val, int farseg, int flags);
    VIRTUAL void reftocodeseg(int seg, targ_size_t offset, targ_size_t val);
    VIRTUAL int reftoident(int seg, targ_size_t offset, Symbol *s, targ_size_t val, int flags);
    VIRTUAL void far16thunk(Symbol *s);
    VIRTUAL void fltused();
    VIRTUAL int data_readonly(char *p, int len, int *pseg);
    VIRTUAL int data_readonly(char *p, int len);
    VIRTUAL symbol *sym_cdata(tym_t, char *, int);
    VIRTUAL void func_start(Symbol *sfunc);
    VIRTUAL void func_term(Symbol *sfunc);

    VIRTUAL symbol *tlv_bootstrap();

    static unsigned addstr(Outbuffer *strtab, const char *);
    static void gotref(symbol *s);
    static symbol *getGOTsym();
    static void refGOTsym();
};

struct ElfObj : Obj
{
    static int getsegment(const char *name, const char *suffix,
        int type, int flags, int align);
    static void addrel(int seg, targ_size_t offset, unsigned type,
                       unsigned symidx, targ_size_t val);
    static size_t writerel(int targseg, size_t offset, unsigned type,
                           unsigned symidx, targ_size_t val);
};

#undef VIRTUAL

extern Obj *objmod;
