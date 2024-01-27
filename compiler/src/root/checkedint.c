
/**********************************************
 * This module implements integral arithmetic primitives that check
 * for out-of-range results.
 * This is a translation to C++ of D's core.checkedint
 *
 * Integral arithmetic operators operate on fixed width types.
 * Results that are not representable in those fixed widths are silently
 * truncated to fit.
 * This module offers integral arithmetic primitives that produce the
 * same results, but set an 'overflow' flag when such truncation occurs.
 * The setting is sticky, meaning that numerous operations can be cascaded
 * and then the flag need only be checked at the end.
 * Whether the operation is signed or unsigned is indicated by an 's' or 'u'
 * suffix, respectively. While this could be achieved without such suffixes by
 * using overloading on the signedness of the types, the suffix makes it clear
 * which is happening without needing to examine the types.
 *
 * While the generic versions of these functions are computationally expensive
 * relative to the cost of the operation itself, compiler implementations are free
 * to recognize them and generate equivalent and faster code.
 *
 * References: $(LINK2 http://blog.regehr.org/archives/1139, Fast Integer Overflow Checks)
 * Copyright: Copyright (C) 2014-2021 by The D Language Foundation, All Rights Reserved
 * License:   $(LINK2 http://www.boost.org/LICENSE_1_0.txt, Boost License 1.0)
 * Authors:   Walter Bright
 * Source:    https://github.com/D-Programming-Language/dmd/blob/master/src/root/checkedint.c
 */

#include "dsystem.h"
#include "checkedint.h"

/*******************************
 * Multiply two unsigned integers, checking for overflow (aka carry).
 *
 * The overflow is sticky, meaning a sequence of operations can
 * be done and overflow need only be checked at the end.
 * Params:
 *      x = left operand
 *      y = right operand
 *      overflow = set if an overflow occurs, is not affected otherwise
 * Returns:
 *      the sum
 */

uint64_t mulu(uint64_t x, uint64_t y, bool& overflow)
{
    uint64_t r = x * y;
    if (x && (r / x) != y)
        overflow = true;
    return r;
}
