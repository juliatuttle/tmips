#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core.h"
#include "core_cp0.h"
#include "core_priv.h"
#include "exc.h"

void core_cp0_reset(core_t *c, core_cp0_t *cp0)
{
    memset(cp0->r, 0, sizeof(cp0->r));
}

int core_cp0_step(core_t *c, core_cp0_t *cp0)
{
    return OK;
}

int core_cp0_except(core_t *c, core_cp0_t *cp0, uint8_t exc_code)
{
    uint32_t vector;

    assert(!(exc_code & ~0x1F));

    fprintf(stderr, "core_cp0_except: %s\n", exc_text[exc_code]);

    /* XXX:
       Does not handle Reset, Cache Error, Soft Reset, NMI.
       Does not handle TLBrefill and XTLBrefill.
       Does not handle EXL = 1.
       Does not handle BEV = 1.
    */

    cp0->r[CP0_EPC] = core_get_pc(c);
    cp0->r[CP0_CAUSE] = exc_code << 2;

    vector = 0x180;

    cp0->r[CP0_STATUS] |= STATUS_EXL;
    core_set_pc(c, 0x80000000 + vector);

    return EXCEPTED;
}

int core_cp0_move_from(core_t *c, core_cp0_t *cp0, uint8_t reg, uint32_t *val_out)
{
    *val_out = (reg < CP0_NUM_REGS) ? cp0->r[reg] : 0;
    return 0;
}

int core_cp0_move_to(core_t *c, core_cp0_t *cp0, uint8_t reg, uint32_t val)
{
    if (reg < CP0_NUM_REGS) {
        cp0->r[reg] = val;
    }
    return 0;
}
