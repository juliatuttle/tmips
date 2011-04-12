#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core.h"
#include "core_cp0.h"
#include "core_priv.h"
#include "debug.h"
#include "exc.h"

#define EXC_BASE 0x80000000

#define PAGE_MASK 0xFFFFF000
#define OFF_MASK 0x00000FFF

enum {
    U_MODE   = 0x01,
    S_MODE   = 0x02,
    K_MODE   = 0x04,
    UNMAPPED = 0x08,
    UNCACHED = 0x10
};

struct segment {
    uint32_t base;
    uint32_t size;
    int flags;
    char *name;
};

struct segment segs[] = {
    { 0x00000000, 0x80000000, U_MODE, "useg" },
    { 0x00000000, 0x80000000, S_MODE, "suseg" },
    { 0x00000000, 0x80000000, K_MODE, "kuseg" },
    { 0x80000000, 0x20000000, K_MODE | UNMAPPED, "kseg0" },
    { 0xA0000000, 0x20000000, K_MODE | UNMAPPED | UNCACHED, "kseg1" },
    { 0xC0000000, 0x20000000, S_MODE, "sseg" },
    { 0xC0000000, 0x20000000, K_MODE, "ksseg" },
    { 0xE0000000, 0x20000000, K_MODE, "kseg3" }
};

#define NUM_SEGS ((sizeof(segs)) / (sizeof(segs[0])))

static void tlb_write(core_t *c, core_cp0_t *cp0, uint32_t idx,
                      uint32_t hi, uint32_t lo);
static int tlb_search(core_t *c, core_cp0_t *cp0, uint32_t tag,
                      uint32_t *data_out);
static struct segment *find_seg(uint32_t addr, int mode);
static int get_mode(core_t *c, core_cp0_t *cp0);



void core_cp0_reset(core_t *c, core_cp0_t *cp0)
{
    memset(cp0->r, 0, sizeof(cp0->r));
    cp0->r[CP0_STATUS] = STATUS_EXL;
}

int core_cp0_step(core_t *c, core_cp0_t *cp0)
{
    cp0->r[CP0_RANDOM] = (cp0->r[CP0_RANDOM] + 1) % 16;
    return 0;
}

int core_cp0_move_from(core_t *c, core_cp0_t *cp0, uint8_t reg,
                       uint32_t *val_out)
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

int core_cp0_except(core_t *c, core_cp0_t *cp0, uint8_t exc_code)
{
    uint32_t epc;
    uint32_t vector;

    assert(!(exc_code & ~0x1F));

    switch (exc_code) {
    case EXC_TLBL:
    case EXC_TLBS:
        vector = 0x000;
        break;
    default:
        vector = 0x180;
        break;
    }

    epc = core_get_pc(c);
    cp0->r[CP0_EPC] = epc;
    cp0->r[CP0_CAUSE] = exc_code << 2;
    cp0->r[CP0_STATUS] |= STATUS_EXL;
    debug_printf(EXC, DETAIL,
            "Took exception: epc=%08x exc_code=%d (%s)\n",
            epc, exc_code, exc_text[exc_code]);
    core_set_pc(c, EXC_BASE + vector);

    return EXCEPTED;
}

int core_cp0_eret(core_t *c, core_cp0_t *cp0, uint32_t *new_pc)
{
    assert(new_pc);

    cp0->r[CP0_STATUS] &= ~STATUS_EXL;
    *new_pc = cp0->r[CP0_EPC];
    debug_printf(EXC, DETAIL, "ERET returning to epc=%08x\n", cp0->r[CP0_EPC]);

    return 0;
}

int core_cp0_user_mode(core_t *c, core_cp0_t *cp0)
{
    return get_mode(c, cp0) == U_MODE;
}



int core_cp0_tlbwr(core_t *c, core_cp0_t *cp0)
{
    tlb_write(c, cp0, cp0->r[CP0_RANDOM], cp0->r[CP0_ENTRYHI], cp0->r[CP0_ENTRYLO]);

    return 0;
}

int core_cp0_tlbwi(core_t *c, core_cp0_t *cp0)
{
    if (cp0->r[CP0_INDEX] >= CP0_TLB_SIZE) {
        debug_printf(VM, WARNING,
                "Undefined behavior: TLBWI with INDEX=%d (max is %d).\n",
                cp0->r[CP0_INDEX], CP0_TLB_SIZE);
        return 0;
    }

    tlb_write(c, cp0, cp0->r[CP0_INDEX], cp0->r[CP0_ENTRYHI], cp0->r[CP0_ENTRYLO]);

    return 0;
}



int core_cp0_translate(core_t *c, core_cp0_t *cp0, uint32_t va,
                       uint32_t *pa_out, int write) 
{
    struct segment *seg;
    uint32_t tlb_tag, tlb_data;

    seg = find_seg(va, get_mode(c, cp0));
    if (!seg) {
        cp0->r[CP0_BADVADDR] = va;
        debug_printf(VM, DETAIL,
                "translate: %08x => address error (no segment)\n", va);
        return core_cp0_except(c, cp0, write ? EXC_ADEL : EXC_ADES);
    }

    if (seg->flags & UNMAPPED) {
        *pa_out = va - seg->base;
        goto out;
    }

    tlb_tag = va & PAGE_MASK;
    if (tlb_search(c, cp0, tlb_tag, &tlb_data)) {
        cp0->r[CP0_BADVADDR] = va;
        debug_printf(VM, DETAIL,
                "translate: %08x => TLB refill (segment=%s)\n",
                va, seg->name);
        return core_cp0_except(c, cp0, write ? EXC_TLBL : EXC_TLBS);
    }

    *pa_out = (tlb_data & PAGE_MASK) | (va & OFF_MASK);

out:
    debug_printf(VM, DETAIL,
            "translate: %08x => %08x (segment=%s)\n",
            va, *pa_out, seg->name);
            
    return 0;
}

void core_cp0_dump_regs(core_t *c, core_cp0_t *cp0, FILE *out)
{
    unsigned i;

    fprintf(out, "STATUS=%08x  EPC=%08x  CAUSE=%08x  BADVADDR=%08x\n",
            cp0->r[CP0_STATUS],
            cp0->r[CP0_EPC],
            cp0->r[CP0_CAUSE],
            cp0->r[CP0_BADVADDR]);
    fprintf(out, "INDEX=%08x  RANDOM=%08x  ENTRYHI=%08x  ENTRYLO=%08x\n",
            cp0->r[CP0_INDEX],
            cp0->r[CP0_RANDOM],
            cp0->r[CP0_ENTRYHI],
            cp0->r[CP0_ENTRYLO]);

    for (i = 0; i < CP0_TLB_SIZE; i += 2) {
        fprintf(out, "TLB[%02d]=%08x:%08x  TLB[%02d]=%08x:%08x\n",
                i, cp0->tlb[i].tag, cp0->tlb[i].data,
                i + 1, cp0->tlb[i + 1].tag, cp0->tlb[i + 1].data);
    }
}


static void tlb_write(core_t *c, core_cp0_t *cp0, uint32_t idx,
                      uint32_t hi, uint32_t lo)
{
    assert(idx < CP0_TLB_SIZE);

    cp0->tlb[idx].tag = hi;
    cp0->tlb[idx].data = lo;

    /* TODO: Check for conflict? */
}

/*
    TODO:
     * Ignores ASID and G bit.
     * Ignores multiple matches.
*/

static int tlb_search(core_t *c, core_cp0_t *cp0, uint32_t tag,
                      uint32_t *data_out)
{
    int i;

    for (i = 0; i < CP0_TLB_SIZE; i++) {
        if ((tag & PAGE_MASK) == (cp0->tlb[i].tag & PAGE_MASK)) {
            *data_out = cp0->tlb[i].data;
            return 0;
        }
    }

    return 1;
}

static struct segment *find_seg(uint32_t addr, int mode)
{
    unsigned i;

    for (i = 0; i < NUM_SEGS; i++) {
        if ((addr >= segs[i].base)
            && (addr <= segs[i].base + segs[i].size - 1)
            && ((mode & segs[i].flags) == mode)) {
            return &segs[i];
        }
    }

    return NULL;
}

static int get_mode(core_t *c, core_cp0_t *cp0)
{
    uint32_t status = cp0->r[CP0_STATUS];
    if ((status & STATUS_UM) && !(status & STATUS_EXL)) {
        return U_MODE;
    } else {
        return K_MODE;
    }
}


