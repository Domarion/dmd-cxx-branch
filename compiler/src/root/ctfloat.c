/**
 * Compiler implementation of the
 * $(LINK2 http://www.dlang.org, D programming language).
 *
 * Copyright:   Copyright (C) 1999-2021 by The D Language Foundation, All Rights Reserved
 * Authors:     $(LINK2 http://www.digitalmars.com, Walter Bright)
 * License:     $(LINK2 http://www.boost.org/LICENSE_1_0.txt, Boost License 1.0)
 * Source:      $(DMDSRC root/_ctfloat.d)
 */

#include "ctfloat.h"
#include "hash.h"

#include <math.h>
#include <string.h>
#include <float.h>
#include <assert.h>
#include <errno.h>
#include <limits> // for std::numeric_limits

real_t CTFloat::zero = real_t(0);
real_t CTFloat::one = real_t(1);
real_t CTFloat::minusone = real_t(-1);
real_t CTFloat::half = real_t(0.5);

#if __i386 || __x86_64__
bool CTFloat::yl2x_supported = true;
bool CTFloat::yl2xp1_supported = true;
#endif

#if __i386 || __x86_64__
void CTFloat::yl2x(const real_t* x, const real_t* y, real_t* res)
{
    __asm__ volatile("fyl2x": "=t" (*res): "u" (*y), "0" (*x) : "st(1)" );
}

void CTFloat::yl2xp1(const real_t* x, const real_t* y, real_t* res)
{
    __asm__ volatile("fyl2xp1": "=t" (*res): "u" (*y), "0" (*x) : "st(1)" );
}
#endif

real_t CTFloat::parse(const char *literal, bool *isOutOfRange)
{
    real_t r = ::strtold(literal, NULL);
    if (isOutOfRange)
        *isOutOfRange = (errno == ERANGE);
    return r;
}

int CTFloat::sprint(char* str, char fmt, real_t x)
{
    if (((real_t)(unsigned long long)x) == x)
    {   // ((1.5 -> 1 -> 1.0) == 1.5) is false
        // ((1.0 -> 1 -> 1.0) == 1.0) is true
        // see http://en.cppreference.com/w/cpp/io/c/fprintf
        char sfmt[5] = "%#Lg";
        sfmt[3] = fmt;
        return sprintf(str, sfmt, x);
    }
    else
    {
        char sfmt[4] = "%Lg";
        sfmt[2] = fmt;
        return sprintf(str, sfmt, x);
    }
}

size_t CTFloat::hash(real_t a)
{
    if (isNaN(a))
        a = std::numeric_limits<real_t>::quiet_NaN();
    size_t sz = (std::numeric_limits<real_t>::digits == 64) ? 10 : sizeof(real_t);
    return calcHash((uint8_t *) &a, sz);
}

real_t CTFloat::sin(real_t x) { return sinl(x); }
real_t CTFloat::cos(real_t x) { return cosl(x); }
real_t CTFloat::tan(real_t x) { return tanl(x); }
real_t CTFloat::sqrt(real_t x) { return sqrtl(x); }
real_t CTFloat::fabs(real_t x) { return fabsl(x); }
real_t CTFloat::ldexp(real_t n, int exp) { return ldexpl(n, exp); }

real_t CTFloat::round(real_t x) { return roundl(x); }
real_t CTFloat::floor(real_t x) { return floorl(x); }
real_t CTFloat::ceil(real_t x) { return ceill(x); }
real_t CTFloat::trunc(real_t x) { return truncl(x); }
real_t CTFloat::log(real_t x) { return logl(x); }
real_t CTFloat::log2(real_t x) { return log2l(x); }
real_t CTFloat::log10(real_t x) { return log10l(x); }
real_t CTFloat::pow(real_t x, real_t y) { return powl(x, y); }
real_t CTFloat::exp(real_t x) { return expl(x); }
real_t CTFloat::expm1(real_t x) { return expm1l(x); }
real_t CTFloat::exp2(real_t x) { return exp2l(x); }
real_t CTFloat::copysign(real_t x, real_t s) { return copysignl(x, s); }

real_t CTFloat::fmin(real_t x, real_t y) { return x < y ? x : y; }
real_t CTFloat::fmax(real_t x, real_t y) { return x > y ? x : y; }

real_t CTFloat::fma(real_t x, real_t y, real_t z) { return (x * y) + z; }

bool CTFloat::isIdentical(real_t x, real_t y)
{
    /* In some cases, the REALPAD bytes get garbage in them,
     * so be sure and ignore them.
     */
    return memcmp(&x, &y, 10) == 0;
}

bool CTFloat::isNaN(real_t r)
{
    return !(r == r);
}

bool CTFloat::isSNaN(real_t r)
{
    /* A signalling NaN is a NaN with 0 as the most significant bit of
     * its significand, which is bit 62 of 0..79 for 80 bit reals.
     */
    return isNaN(r) && !((((unsigned char*)&r)[7]) & 0x40);
}

bool CTFloat::isInfinity(real_t r)
{
#if defined(__GNUC__) || defined(__clang__)
    return isIdentical(fabs(r), std::numeric_limits<real_t>::infinity());
#else
    return isIdentical(fabs(r), INFINITY);
#endif
}
