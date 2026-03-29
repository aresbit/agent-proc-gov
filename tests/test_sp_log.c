/* test_sp_log.c - Test SP_LOG formatting */

#include "sp.h"

int main() {
    const char* version = "2.0 (modern C with sp.h)";
    int pid = 12345;
    const char* progname = "test";

    SP_LOG("Testing SP_LOG with string: {}", SP_FMT_CSTR(version));
    SP_LOG("Testing SP_LOG with integer: {}", SP_FMT_S32(pid));
    SP_LOG("Testing SP_LOG with C string: {}", SP_FMT_CSTR(progname));

    return 0;
}