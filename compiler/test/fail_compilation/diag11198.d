/*
TEST_OUTPUT:
---
fail_compilation/diag11198.d(17): Error: version `blah` declaration must be at module level
fail_compilation/diag11198.d(18): Error: debug `blah` declaration must be at module level
fail_compilation/diag11198.d(19): Error: `version = <integer>` is obsolete, use version identifiers instead
fail_compilation/diag11198.d(19): Error: version `1` level declaration must be at module level
fail_compilation/diag11198.d(20): Error: `debug = <integer>` is obsolete, use debug identifiers instead
fail_compilation/diag11198.d(20): Error: debug `2` level declaration must be at module level
fail_compilation/diag11198.d(21): Error: identifier or integer expected, not `""`
fail_compilation/diag11198.d(22): Error: identifier or integer expected, not `""`
---
*/

void main()
{
    version = blah;
    debug = blah;
    version = 1;
    debug = 2;
    version = "";
    debug = "";
}
