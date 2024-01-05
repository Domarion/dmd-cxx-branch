/*
TEST_OUTPUT:
---
fail_compilation/test13786.d(16): Error: `debug = <integer>` is obsolete, use debug identifiers instead
fail_compilation/test13786.d(18): Error: `version = <integer>` is obsolete, use version identifiers instead
fail_compilation/test13786.d(16): Error: debug `123` level declaration must be at module level
fail_compilation/test13786.d(17): Error: debug `abc` declaration must be at module level
fail_compilation/test13786.d(18): Error: version `123` level declaration must be at module level
fail_compilation/test13786.d(19): Error: version `abc` declaration must be at module level
fail_compilation/test13786.d(22): Error: template instance `test13786.T!()` error instantiating---
*/

template T()
{
    debug = 123;
    debug = abc;
    version = 123;
    version = abc;
}

alias X = T!();
