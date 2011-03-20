#include "err.h"

const char *err_text[] = {
    [ERR_TESTDONE] = "TESTDONE called",
    [ERR_EXC] = "Unhandled exception",
    [ERR_EXC_FLOOD] = "Exception flood",
};
