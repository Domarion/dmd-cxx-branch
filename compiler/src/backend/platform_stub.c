// Copyright (C) 2011-2021 by The D Language Foundation, All Rights Reserved
// http://www.digitalmars.com
// Written by Brad Roberts
/*
 * This file is licensed under the Boost 1.0 license.
 */

// this file stubs out all the apis that are platform specific

#include <stdio.h>

#include "cc.h"
#include "code.h"

static char __file__[] = __FILE__;
#include "tassert.h"

int clib_inited = 0;
const unsigned dblreg[] = { -1 };

code* nteh_epilog()                               { assert(0); return nullptr; }
code* nteh_filter(block* b)                       { assert(0); return nullptr; }
void  nteh_framehandler(symbol* scopetable)       { assert(0); }
code *nteh_patchindex(code* c, int sindex)        { assert(0); return nullptr; }
code* nteh_gensindex(int sindex)                  { assert(0); return nullptr; }
code* nteh_monitor_epilog(regm_t retregs)         { assert(0); return nullptr; }
code* nteh_monitor_prolog(Symbol* shandle)        { assert(0); return nullptr; }
code* nteh_prolog()                               { assert(0); return nullptr; }
code* nteh_setsp(int op)                          { assert(0); return nullptr; }
code* nteh_unwind(regm_t retregs, unsigned index) { assert(0); return nullptr; }

code* REGSAVE::restore(code* c, int reg, unsigned idx) { assert(0); return nullptr; }
code* REGSAVE::save(code* c, int reg, unsigned* pidx) { assert(0); return nullptr; }

FuncParamRegs::FuncParamRegs(tym_t tyf) { assert(0); }
int FuncParamRegs::alloc(type *t, tym_t ty, unsigned char *preg1, unsigned char *preg2) { assert(0); return 0; }

int dwarf_regno(int reg) { assert(0); return 0; }

code* prolog_ifunc(tym_t* tyf) { assert(0); return nullptr; }
code* prolog_ifunc2(tym_t tyf, tym_t tym, bool pushds) { assert(0); return nullptr; }
code* prolog_16bit_windows_farfunc(tym_t* tyf, bool* pushds) { assert(0); return nullptr; }
code* prolog_frame(unsigned farfunc, unsigned* xlocalsize, bool* enter) { assert(0); return nullptr; }
code* prolog_frameadj(tym_t tyf, unsigned xlocalsize, bool enter, bool* pushalloc) { assert(0); return nullptr; }
code* prolog_frameadj2(tym_t tyf, unsigned xlocalsize, bool* pushalloc) { assert(0); return nullptr; }
code* prolog_setupalloca() { assert(0); return nullptr; }
code* prolog_trace(bool farfunc, unsigned* regsaved) { assert(0); return nullptr; }
code* prolog_genvarargs(symbol* sv, regm_t* namedargs) { assert(0); return nullptr; }
code* prolog_gen_win64_varargs() { assert(0); return nullptr; }
code* prolog_loadparams(tym_t tyf, bool pushalloc, regm_t* namedargs) { assert(0); return nullptr; }
code* prolog_saveregs(code *c, regm_t topush) { assert(0); return nullptr; }
targ_size_t cod3_spoff() { assert(0); }

void  epilog(block* b) { assert(0); }
unsigned calcblksize(code* c) { assert(0); return 0; }
unsigned calccodsize(code* c) { assert(0); return 0; }
unsigned codout(code* c) { assert(0); return 0; }

void assignaddr(block* bl) { assert(0); }
int  branch(block* bl, int flag) { assert(0); return 0; }
void cgsched_block(block* b) { assert(0); }
void doswitch(block* b) { assert(0); }
void jmpaddr(code* c) { assert(0); }
void outblkexitcode(block* bl, code*& c, int& anyspill, const char* sflsave, symbol** retsym, const regm_t mfuncregsave) { assert(0); }
void outjmptab(block* b) { assert(0); }
void outswitab(block* b) { assert(0); }
void pinholeopt(code* c, block* bn) { assert(0); }

bool cse_simple(code* c, elem* e) { assert(0); return false; }
code* comsub87(elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* fixresult(elem* e, regm_t retregs, regm_t* pretregs) { assert(0); return nullptr; }
code* genmovreg(code* c, unsigned to, unsigned from) { assert(0); return nullptr; }
code* genpush(code *c , unsigned reg) { assert(0); return nullptr; }
code* gensavereg(unsigned& reg, targ_uns slot) { assert(0); return nullptr; }
code* gen_spill_reg(Symbol* s, bool toreg) { assert(0); return nullptr; }
code* genstackclean(code* c, unsigned numpara, regm_t keepmsk) { assert(0); return nullptr; }
code* loaddata(elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* longcmp(elem* e, bool jcond, unsigned fltarg, code* targ) { assert(0); return nullptr; }
code* movregconst(code* c, unsigned reg, targ_size_t value, regm_t flags) { assert(0); return nullptr; }
code* params(elem* e, unsigned stackalign) { assert(0); return nullptr; }
code* save87() { assert(0); return nullptr; }
code* tstresult(regm_t regm, tym_t tym, unsigned saveflag) { assert(0); return nullptr; }
int cod3_EA(code* c) { assert(0); return 0; }
regm_t cod3_useBP() { assert(0); return 0; }
regm_t regmask(tym_t tym, tym_t tyf) { assert(0); return 0; }
targ_size_t cod3_bpoffset(symbol* s) { assert(0); return 0; }
unsigned char loadconst(elem* e, int im) { assert(0); return 0; }
void cod3_adjSymOffsets() { assert(0); }
void cod3_align() { assert(0); }
void cod3_align_bytes(size_t nbytes) { assert(0); }
void cod3_initregs() { assert(0); }
void cod3_setdefault() { assert(0); }
void cod3_set32() { assert(0); }
void cod3_set64() { assert(0); }
void cod3_thunk(symbol* sthunk, symbol* sfunc, unsigned p, tym_t thisty, targ_size_t d, int i, targ_size_t d2) { assert(0); }
void genEEcode() { assert(0); }
unsigned gensaverestore(regm_t regm, code** csave, code** crestore) { assert(0); return 0; }
unsigned gensaverestore2(regm_t regm, code** csave, code** crestore) { assert(0); return 0; }

bool isXMMstore(unsigned op) { assert(0); return false; }
const unsigned char* getintegerparamsreglist(tym_t tyf, size_t* num) { assert(0); *num = 0; return nullptr; }
const unsigned char* getfloatparamsreglist(tym_t tyf, size_t* num) { assert(0); *num = 0; return nullptr; }
void cod3_buildmodulector(Outbuffer* buf, int codeOffset, int refOffset) { assert(0); }
code* gen_testcse(code *c, unsigned sz, targ_uns i) { assert(0); return nullptr; }
code* gen_loadcse(code *c, unsigned reg, targ_uns i) { assert(0); return nullptr; }
code* cod3_stackadj(code* c, int nbytes) { assert(0); return nullptr; }
void simplify_code(code *c) { assert(0); }
void cgreg_dst_regs(unsigned *dst_integer_reg, unsigned *dst_float_reg) { assert(0); }
void cgreg_set_priorities(tym_t ty, char **pseq, char **pseqmsw) { assert(0); }

code* cdabs      (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdaddass   (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdasm      (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdbscan    (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdbtst     (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdbswap    (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdbt       (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdbyteint  (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdcmp      (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdcnvt     (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdcom      (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdcomma    (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdcond     (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdconvt87  (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdctor     (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cddctor    (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdddtor    (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cddtor     (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdeq       (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cderr      (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdframeptr (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdfunc     (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdgot      (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdhalt     (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdind      (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdinfo     (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdlngsht   (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdloglog   (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdmark     (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdmemcmp   (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdmemcpy   (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdmemset   (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdmsw      (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdmul      (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdmulass   (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdneg      (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdnot      (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdorth     (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdpair     (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdpopcnt   (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdport     (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdpost     (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdrelconst (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdrndtol   (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdscale    (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdsetjmp   (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdshass    (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdshift    (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdshtlng   (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdstrcmp   (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdstrcpy   (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdstreq    (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdstrlen   (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdstrthis  (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdvecsto   (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdvector   (elem* e, regm_t* pretregs) { assert(0); return nullptr; }
code* cdvoid     (elem* e, regm_t* pretregs) { assert(0); return nullptr; }

