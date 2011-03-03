#ifndef HAVE_CP0_H
#define HAVE_CP0_H

#include <stdint.h>

enum {
    CP0_INDEX = 0,
    CP0_RANDOM,
    CP0_ENTRYLO0,
    CP0_ENTRYLO1,
    CP0_CONTEXT,
    CP0_PAGE_MASK,
    CP0_WIRED,

    CP0_BADVADDR = 8,
    CP0_COUNT,
    CP0_ENTRYHI,
    CP0_COMPARE,
    CP0_STATUS,
    CP0_CAUSE,
    CP0_EPC,
    CP0_PRID,
    CP0_CONFIG,
    CP0_LLADDR,
    CP0_WATCHLO,
    CP0_WATCHHI,
    CP0_XCONTEXT,

    CP0_ECC = 26,
    CP0_CACHEERR,
    CP0_TAGLO,
    CP0_TAGHI,
    CP0_ERROREPC,

    CP0_NUM_REGS
};

enum {
    STATUS_EXL = 0x00000002
};

typedef struct core_cp0 core_cp0_t;
struct core_cp0 {
    uint32_t r[CP0_NUM_REGS];
};

#include "core.h"

void core_cp0_reset(core_t *c, core_cp0_t *cp0);
int core_cp0_step(core_t *c, core_cp0_t *cp0);
int core_cp0_except(core_t *c, core_cp0_t *cp0, uint8_t exc_code);

#endif
