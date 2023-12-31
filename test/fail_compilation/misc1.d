/*
TEST_OUTPUT:
---
fail_compilation/misc1.d(108): Error: long has no effect in expression (5)
fail_compilation/misc1.d(109): Error: + has no effect in expression (1 + 2)
fail_compilation/misc1.d(115): Error: * has no effect in expression (1 * 1)
fail_compilation/misc1.d(116): Error: function has no effect in expression (__lambda1)
fail_compilation/misc1.d(122): Error: long has no effect in expression (false)
fail_compilation/misc1.d(125): Error: * has no effect in expression (*sp++)
fail_compilation/misc1.d(126): Error: var has no effect in expression (j)
---
*/

#line 100

/***************************************************/
//https://issues.dlang.org/show_bug.cgi?id=12490

void hasSideEffect12490(){}

void issue12490()
{
    5, hasSideEffect12490();
    1 + 2, hasSideEffect12490();
}

void issue23480()
{
    int j;
    for({} j; 1*1) {}
    for({j=2; int d = 3;} j+d<7; {j++; d++;}) {}
    for (
        if (true)        // (o_O)
            assert(78);
        else
            assert(79);
        false; false
    ) {}
    // unnecessary deref
    for (ubyte* sp; 0; *sp++) {}
    for (;; j) {}
}
