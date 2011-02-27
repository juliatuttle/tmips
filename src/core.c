#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "error.h"
#include "opcode.h"
#include "util.h"

#define NUM_REGS 32

struct core {
    mem_t *mem;
    uint32_t r[NUM_REGS];
    uint32_t hi;
    uint32_t lo;
    uint32_t pc;
};

static int rdb(core_t *c, uint32_t addr, uint8_t *out);
static int rdh(core_t *c, uint32_t addr, uint16_t *out);
static int rdw(core_t *c, uint32_t addr, uint32_t *out);
static int wrb(core_t *c, uint32_t addr, uint8_t in);
static int wrh(core_t *c, uint32_t addr, uint16_t in);
static int wrw(core_t *c, uint32_t addr, uint32_t in);
static void set_hilo(core_t *c, uint64_t val);

static int add_overflows(uint32_t a, uint32_t b);
static int sub_overflows(uint32_t a, uint32_t b);

core_t *core_create(mem_t *m)
{
    core_t *c = xmalloc(sizeof(*c));
    memset(c, 0, sizeof(*c));
    c->mem = m;
    return c;
}

void core_destroy(core_t *c)
{
    free(c);
}

void core_set_pc(core_t *c, uint32_t pc)
{
    c->pc = pc;
}

#define SE8(b) ((uint32_t)((int32_t)((int8_t)(b))))
#define SE16(hw) ((uint32_t)((int32_t)((int16_t)(hw))))
#define SIMMED(ins) ((int32_t)((int16_t)IMMED(ins)))

#define ADDR(c, ins) ((c)->r[RS(ins)] + SIMMED(ins))

#define SRA(val, shift) ((uint32_t)(((int32_t)(val)) >> (shift)))

#define JUMP_TARGET(c, ins) (((c)->pc & 0xF0000000) | (TARGET(ins) << 2))
#define BRANCH_TARGET(c, ins) ((c)->pc + (SIMMED(ins) << 2) + 4)
#define LINK(c) (c->r[31] = c->pc + 4)

int core_step(core_t *c)
{
    uint32_t ins;
    uint32_t newpc;
    uint8_t b; uint16_t h; uint32_t w;

    if (rdw(c, c->pc, &ins)) { return ERR_MEM; }

#ifdef DEBUG_TRACE_STEP
    fprintf(stderr, "core_step: fetched %08x (OP=%03o RS=%02d RT=%02d RD=%02d SA=%d FUNCT=%03o IMMED=%04x TARGET=%08x) from %08x\n",
        ins, OP(ins), RS(ins), RT(ins), RD(ins), SA(ins), FUNCT(ins), IMMED(ins), TARGET(ins), c->pc);
#endif

    newpc = c->pc + 4;

    switch (OP(ins)) {
    case OP_SPECIAL:
        switch (FUNCT(ins)) {
        case FUNCT_SLL:
            c->r[RD(ins)] = c->r[RT(ins)] << SA(ins);
            break;
        case FUNCT_SRL:
            c->r[RD(ins)] = c->r[RT(ins)] >> SA(ins);
            break;
        case FUNCT_SRA:
            c->r[RD(ins)] = SRA(c->r[RT(ins)], SA(ins));
            break;
        case FUNCT_SLLV:
            if (SA(ins) != 0) { return ERR_INS; }
            c->r[RD(ins)] = c->r[RT(ins)] << (c->r[RS(ins)] & 0x1F);
            break;
        case FUNCT_SRLV:
            if (SA(ins) != 0) { return ERR_INS; }
            c->r[RD(ins)] = c->r[RT(ins)] >> (c->r[RS(ins)] & 0x1F);
            break;
        case FUNCT_SRAV:
            if (SA(ins) != 0) { return ERR_INS; }
            c->r[RD(ins)] = SRA(c->r[RT(ins)], c->r[RS(ins)] & 0x1F);
            break;
        case FUNCT_JR:
            if ((RT(ins) != 0) || (RD(ins) != 0) || (SA(ins) != 0)) { return ERR_INS; }
            newpc = c->r[RS(ins)];
            break;
        case FUNCT_JALR:
            if ((RT(ins) != 0) || (SA(ins) != 0)) { return ERR_INS; }
            c->r[RD(ins)] = c->pc + 4;
            newpc = c->r[RS(ins)];
            break;
        case FUNCT_SYSCALL:
            return ERR_SYSCALL;
            break;
        case FUNCT_ADD:
            if (add_overflows(c->r[RS(ins)], c->r[RT(ins)])) { return ERR_OFLOW; }
            /* Fall through. */
        case FUNCT_ADDU:
            if (SA(ins) != 0) { return ERR_INS; }
            c->r[RD(ins)] = c->r[RS(ins)] + c->r[RT(ins)];
            break;
        case FUNCT_SUB:
            if (sub_overflows(c->r[RS(ins)], c->r[RT(ins)])) { return ERR_OFLOW; }
            /* Fall through. */
        case FUNCT_SUBU:
            if (SA(ins) != 0) { return ERR_INS; }
            c->r[RD(ins)] = c->r[RS(ins)] - c->r[RT(ins)];
            break;
        case FUNCT_AND:
            if (SA(ins) != 0) { return ERR_INS; }
            c->r[RD(ins)] = c->r[RS(ins)] & c->r[RT(ins)];
            break;
        case FUNCT_OR:
            if (SA(ins) != 0) { return ERR_INS; }
            c->r[RD(ins)] = c->r[RS(ins)] | c->r[RT(ins)];
            break;
        case FUNCT_XOR:
            if (SA(ins) != 0) { return ERR_INS; }
            c->r[RD(ins)] = c->r[RS(ins)] ^ c->r[RT(ins)];
            break;
        case FUNCT_NOR:
            if (SA(ins) != 0) { return ERR_INS; }
            c->r[RD(ins)] = ~(c->r[RS(ins)] | c->r[RT(ins)]);
            break;
        case FUNCT_SLT:
            if (SA(ins) != 0) { return ERR_INS; }
            c->r[RD(ins)] = (((int32_t)c->r[RS(ins)]) < ((int32_t)c->r[RT(ins)])) ? 1 : 0;
            break;
        case FUNCT_SLTU:
            if (SA(ins) != 0) { return ERR_INS; }
            c->r[RD(ins)] = (c->r[RS(ins)] < c->r[RT(ins)]) ? 1 : 0;
            break;
        case FUNCT_MULT:
            if ((RD(ins) != 0) || (SA(ins) != 0)) { return ERR_INS; }
            {
                int64_t s = (int64_t)((int32_t)c->r[RS(ins)]);
                int64_t t = (int64_t)((int32_t)c->r[RT(ins)]);
                set_hilo(c, (uint64_t)(s * t));
            }
            break;
        case FUNCT_MFHI:
            if ((RS(ins) != 0) || (RT(ins) != 0) || (SA(ins) != 0)) { return ERR_INS; }
            c->r[RD(ins)] = c->hi;
            break;
        case FUNCT_MFLO:
            if ((RS(ins) != 0) || (RT(ins) != 0) || (SA(ins) != 0)) { return ERR_INS; }
            c->r[RD(ins)] = c->lo;
            break;
        case FUNCT_MTHI:
            if ((RD(ins) != 0) || (RT(ins) != 0) || (SA(ins) != 0)) { return ERR_INS; }
            c->hi = c->r[RS(ins)];
            break;
        case FUNCT_MTLO:
            if ((RD(ins) != 0) || (RT(ins) != 0) || (SA(ins) != 0)) { return ERR_INS; }
            c->lo = c->r[RS(ins)];
            break;
        case FUNCT_MULTU:
            if ((RD(ins) != 0) || (SA(ins) != 0)) { return ERR_INS; }
            set_hilo(c, (uint64_t)c->r[RS(ins)] * (uint64_t)c->r[RT(ins)]);
            break;
        case FUNCT_DIV:
            if ((RD(ins) != 0) || (SA(ins) != 0)) { return ERR_INS; }
            if (c->r[RT(ins)] != 0) {
                c->lo = (uint32_t)(((int32_t)c->r[RS(ins)]) / ((int32_t)c->r[RT(ins)]));
                c->hi = (uint32_t)(((int32_t)c->r[RS(ins)]) % ((int32_t)c->r[RT(ins)]));
            } else {
                c->lo = 0xDEADBEEF;
                c->hi = 0xFEEDFACE;
            }
            break;
        case FUNCT_DIVU:
            if ((RD(ins) != 0) || (SA(ins) != 0)) { return ERR_INS; }
            if (c->r[RT(ins)] != 0) {
                c->lo = c->r[RS(ins)] / c->r[RT(ins)];
                c->hi = c->r[RS(ins)] % c->r[RT(ins)];
            } else {
                c->lo = 0xDEADBEEF;
                c->hi = 0xFEEDFACE;
            }
        default:
            fprintf(stderr, "core_step: unimplemented SPECIAL function %03o\n", FUNCT(ins));
            return ERR_INS;
        }
        break;
    case OP_REGIMM:
        switch (RT(ins)) {
        case REGIMM_BLTZAL:
            LINK(c);
            /* Fall through. */
        case REGIMM_BLTZ:
            if ((int32_t)c->r[RS(ins)] < 0) {
                newpc = BRANCH_TARGET(c, ins);
            }
            break;
        case REGIMM_BGEZAL:
            LINK(c);
            /* Fall through. */
        case REGIMM_BGEZ:
            if ((int32_t)c->r[RS(ins)] >= 0) {
                newpc = BRANCH_TARGET(c, ins);
            }
            break;
        default:
            fprintf(stderr, "core_step: unimplemented REGIMM rt %03o\n", RT(ins));
            return ERR_INS;
        }
        break;
    case OP_JAL:
        LINK(c);
        /* Fall through. */
    case OP_J:
        newpc = JUMP_TARGET(c, ins);
        break;
    case OP_BEQ:
        if (c->r[RS(ins)] == c->r[RT(ins)]) {
            newpc = BRANCH_TARGET(c, ins);
        }
        break;
    case OP_BNE:
        if (c->r[RS(ins)] != c->r[RT(ins)]) {
            newpc = BRANCH_TARGET(c, ins);
        }
        break;
    case OP_BLEZ:
        if (RT(ins) != 0) { return ERR_INS; }
        if ((int32_t)c->r[RS(ins)] <= 0) {
            newpc = BRANCH_TARGET(c, ins);
        }
        break;
    case OP_BGTZ:
        if (RT(ins) != 0) { return ERR_INS; }
        if ((int32_t)c->r[RS(ins)] > 0) {
            newpc = BRANCH_TARGET(c, ins);
        }
        break;
    case OP_ADDI:
        if (add_overflows(c->r[RS(ins)], SIMMED(ins))) { return ERR_OFLOW; }
        /* Fall through. */
    case OP_ADDIU:
        c->r[RT(ins)] = c->r[RS(ins)] + SIMMED(ins);
        break;
    case OP_SLTI:
        c->r[RT(ins)] = ((int32_t)(c->r[RS(ins)]) < SIMMED(ins)) ? 1 : 0;
        break;
    case OP_SLTIU:
        c->r[RT(ins)] = (c->r[RS(ins)] < (uint32_t)SIMMED(ins)) ? 1 : 0;
        break;
    case OP_ANDI:
        c->r[RT(ins)] = c->r[RS(ins)] & IMMED(ins);
        break;
    case OP_ORI:
        c->r[RT(ins)] = c->r[RS(ins)] | IMMED(ins);
        break;
    case OP_XORI:
        c->r[RT(ins)] = c->r[RS(ins)] ^ IMMED(ins);
        break;
    case OP_LUI:
        if (RS(ins) != 0) return ERR_INS;
        c->r[RT(ins)] = IMMED(ins) << 16;
        break;
    case OP_LB:
        if (rdb(c, ADDR(c, ins), &b)) { return ERR_MEM; }
        c->r[RT(ins)] = SE8(b);
        break;
    case OP_LH:
        if (rdh(c, ADDR(c, ins), &h)) { return ERR_MEM; }
        c->r[RT(ins)] = SE16(h);
        break;
    case OP_LW:
        if (rdw(c, ADDR(c, ins), &w)) { return ERR_MEM; }
        c->r[RT(ins)] = w;
        break;
     case OP_LBU:
        if (rdb(c, ADDR(c, ins), &b)) { return ERR_MEM; }
        c->r[RT(ins)] = (uint32_t)b;
        break;
    case OP_LHU:
        if (rdh(c, ADDR(c, ins), &h)) { return ERR_MEM; }
        c->r[RT(ins)] = (uint32_t)h;
        break;
    case OP_SB:
        b = (uint8_t)c->r[RT(ins)];
        if (wrb(c, ADDR(c, ins), b)) { return ERR_MEM; }
        break;
    case OP_SH:
        h = (uint16_t)c->r[RT(ins)];
        if (wrh(c, ADDR(c, ins), h)) { return ERR_MEM; }
        break;
    case OP_SW:
        w = c->r[RT(ins)];
        if (wrw(c, ADDR(c, ins), w)) { return ERR_MEM; }
        break;
    default:
        fprintf(stderr, "core_step: unhandled OP %03o\n", OP(ins));
        return ERR_INS;
    }

    c->pc = newpc;
    c->r[0] = 0; /* ...damnit! */

    return 0;
}

void core_dump_regs(core_t *c, FILE *out)
{
    int i;

    fprintf(out, "PC =%08x  HI =%08x  LO =%08x\n", c->pc, c->hi, c->lo);
    for (i = 0; i < NUM_REGS; i++) {
        fprintf(out, "R%-2d=%08x  ", i, c->r[i]);
        if (i % 4 == 3) {
            fprintf(out, "\n");
        }
    }
}



static int rdb(core_t *c, uint32_t addr, uint8_t *out)
{
    uint32_t w_addr;
    uint32_t w;
    int ret;

    w_addr = addr & ~0x3;

    ret = mem_read(c->mem, w_addr, &w);
    *out = (uint8_t)(w >> (8 * (addr - w_addr)));
    return ret;
}

static int rdh(core_t *c, uint32_t addr, uint16_t *out)
{
    uint32_t w_addr;
    uint32_t w;
    int ret;

    if (addr & 0x1) { return 1; }

    w_addr = addr & ~0x3;

    ret = mem_read(c->mem, w_addr, &w);
    *out = (uint16_t)(w >> (8 * (addr - w_addr)));
    return ret;
}

static int rdw(core_t *c, uint32_t addr, uint32_t *out)
{
    if (addr & 0x3) { return 1; }

    return mem_read(c->mem, addr, out);
}

static int wrb(core_t *c, uint32_t addr, uint8_t in)
{
    uint32_t w_addr;
    int offset;

    w_addr = addr & ~0x3;
    offset = addr &  0x3;

    return mem_write(c->mem, w_addr,
                     (uint32_t)in << (8 * offset),
                     0x1 << offset);
}

static int wrh(core_t *c, uint32_t addr, uint16_t in)
{
    uint32_t w_addr;
    int offset;

    if (addr & 0x1) { return 1; }

    w_addr = addr & ~0x3;
    offset = addr &  0x3;

    return mem_write(c->mem, w_addr,
                     (uint32_t)in << (8 * offset),
                     0x3 << offset);
}

static int wrw(core_t *c, uint32_t addr, uint32_t in)
{
    if (addr & 0x3) { return ERR_MEM; }

    return mem_write(c->mem, addr, in, 0xF);
}

static void set_hilo(core_t *c, uint64_t val)
{
    c->lo = (uint32_t)val;
    c->hi = (uint32_t)(val >> 32);
}

static int add_overflows(uint32_t a, uint32_t b)
{
    uint32_t c = a + b;
    return ((a >> 31) == (b >> 31)) && ((a >> 31) != (c >> 31));
}

static int sub_overflows(uint32_t a, uint32_t b)
{
    uint32_t c = a - b;
    return ((a >> 31) != (b >> 31)) && ((a >> 31) != (c >> 31));
}
