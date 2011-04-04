#ifndef HAVE_CP0_H
#define HAVE_CP0_H

#include <stdint.h>
#include <stdio.h>

enum {
    CP0_INDEX    =  0,
    CP0_RANDOM   =  1,
    CP0_ENTRYLO  =  2,

    CP0_BADVADDR =  8,

    CP0_ENTRYHI  = 10,

    CP0_STATUS   = 12,
    CP0_CAUSE    = 13,
    CP0_EPC      = 14,

    CP0_NUM_REGS = 31
};

enum {
    STATUS_UM  = 1 << 4,
    STATUS_EXL = 1 << 1,
};

#define CP0_TLB_SIZE 32

typedef struct core_cp0 core_cp0_t;
typedef struct tlb_entry tlb_entry_t;

struct tlb_entry {
    uint32_t tag;
    uint32_t data;
};

struct core_cp0 {
    uint32_t r[CP0_NUM_REGS];
    tlb_entry_t tlb[CP0_TLB_SIZE];
};

#include "core.h"

void core_cp0_reset(core_t *c, core_cp0_t *cp0);
int core_cp0_step(core_t *c, core_cp0_t *cp0);
int core_cp0_translate(core_t *c, core_cp0_t *cp0, uint32_t va,
                       uint32_t *pa_out, int write);
int core_cp0_tlbwi(core_t *c, core_cp0_t *cp0);
int core_cp0_tlbwr(core_t *c, core_cp0_t *cp0);
int core_cp0_except(core_t *c, core_cp0_t *cp0, uint8_t exc_code);
int core_cp0_eret(core_t *c, core_cp0_t *cp0, uint32_t *newpc);
int core_cp0_user_mode(core_t *c, core_cp0_t *cp0);
int core_cp0_move_from(core_t *c, core_cp0_t *cp0, uint8_t reg,
                       uint32_t *val_out);
int core_cp0_move_to(core_t *c, core_cp0_t *cp0, uint8_t reg, uint32_t val);
void core_cp0_dump_regs(core_t *c, core_cp0_t *cp0, FILE *out);

#endif
