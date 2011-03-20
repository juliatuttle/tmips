#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core.h"
#include "core_cp0.h"
#include "core_priv.h"
#include "exc.h"

#define EXC_VECTOR 0x80000180

void core_cp0_reset(core_t *c, core_cp0_t *cp0)
{
    memset(cp0->r, 0, sizeof(cp0->r));
}

int core_cp0_step(core_t *c, core_cp0_t *cp0)
{
    return 0;
}

int core_cp0_except(core_t *c, core_cp0_t *cp0, uint8_t exc_code)
{
    uint32_t epc;
    assert(!(exc_code & ~0x1F));

    epc = core_get_pc(c);
    if (exc_code == EXC_SYS) { epc += 4; }
    cp0->r[CP0_EPC] = epc;
    cp0->r[CP0_CAUSE] = exc_code << 2;
    cp0->r[CP0_STATUS] &= ~STATUS_UM;
    fprintf(stderr, "core_cp0_except: exc=%s pc=%08x\n", exc_text[exc_code], cp0->r[CP0_EPC]);
    core_set_pc(c, EXC_VECTOR);

    return EXCEPTED;
}

int core_cp0_eret(core_t *c, core_cp0_t *cp0, uint32_t *new_pc)
{
    assert(new_pc);

    cp0->r[CP0_STATUS] |= STATUS_UM;
    *new_pc = cp0->r[CP0_EPC];
    fprintf(stderr, "core_cp0_eret: returning to %08x\n", cp0->r[CP0_EPC]);

    return 0;
}

int core_cp0_user_mode(core_t *c, core_cp0_t *cp0)
{
    return !!(cp0->r[CP0_STATUS] & STATUS_UM);
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
