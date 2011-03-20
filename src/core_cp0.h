#ifndef HAVE_CP0_H
#define HAVE_CP0_H

#include <stdint.h>
#include <stdio.h>

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
    STATUS_UM  = 1 << 4,
    STATUS_EXL = 1 << 1,
};

typedef struct core_cp0 core_cp0_t;
struct core_cp0 {
    uint32_t r[CP0_NUM_REGS];
};

#include "core.h"

void core_cp0_reset(core_t *c, core_cp0_t *cp0);
int core_cp0_step(core_t *c, core_cp0_t *cp0);
int core_cp0_except(core_t *c, core_cp0_t *cp0, uint8_t exc_code);
int core_cp0_eret(core_t *c, core_cp0_t *cp0, uint32_t *newpc);
int core_cp0_user_mode(core_t *c, core_cp0_t *cp0);
int core_cp0_move_from(core_t *c, core_cp0_t *cp0, uint8_t reg, uint32_t *val_out);
int core_cp0_move_to(core_t *c, core_cp0_t *cp0, uint8_t reg, uint32_t val);
void core_cp0_dump_regs(core_t *c, core_cp0_t *cp0, FILE *out);

#endif
