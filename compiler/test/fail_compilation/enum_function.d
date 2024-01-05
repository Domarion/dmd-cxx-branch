/*
TEST_OUTPUT:
---
fail_compilation/enum_function.d(10): Error: function cannot have enum storage class
fail_compilation/enum_function.d(11): Error: function cannot have enum storage class
fail_compilation/enum_function.d(12): Error: function cannot have enum storage class
---
*/
enum void f1() { return; }
enum f2() { return 5; }
enum int f4()() { return 5; }
