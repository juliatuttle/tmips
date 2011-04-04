#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "core_priv.h"
#include "core_cp0.h"
#include "debug.h"
#include "err.h"
#include "exc.h"
#include "filter.h"
#include "opcode.h"
#include "util.h"

#define NUM_REGS 32

struct core {
    mem_t *mem;
    filter_t *filter;
    uint32_t r[NUM_REGS];
    uint32_t hi;
    uint32_t lo;
    uint32_t pc;

    core_cp0_t cp0;

    int exc_count;
};

static int __core_step(core_t *c);

static int add_overflows(uint32_t a, uint32_t b);
static int sub_overflows(uint32_t a, uint32_t b);
static void set_hilo(core_t *c, uint64_t val);

static int except(core_t *c, uint8_t exc_code);
static int except_vm(core_t *c, uint8_t exc_code, uint32_t badvaddr);
static int user_mode(core_t *c);
static int translate(core_t *c, uint32_t va, uint32_t *pa_out, int write);

static int rdb(core_t *c, uint32_t addr, uint8_t *out);
static int rdh(core_t *c, uint32_t addr, uint16_t *out);
static int rdw(core_t *c, uint32_t addr, uint32_t *out);
static int rdiw(core_t *c, uint32_t addr, uint32_t *out);
static int wrb(core_t *c, uint32_t addr, uint8_t in);
static int wrh(core_t *c, uint32_t addr, uint16_t in);
static int wrw(core_t *c, uint32_t addr, uint32_t in);
static int _rdw(core_t *c, uint32_t va, uint32_t *out, int ins);
static int _wrw(core_t *c, uint32_t va, uint32_t in, uint8_t we);

core_t *core_create(mem_t *m)
{
    core_t *c = xmalloc(sizeof(*c));
    c->mem = m;
    c->filter = NULL;
    return c;
}

void core_reset(core_t *c)
{
    memset(c->r, 0, sizeof(c->r));
    c->hi = c->lo = c->pc = 0;
    core_cp0_reset(c, &c->cp0);
    c->exc_count = 0;
}

void core_destroy(core_t *c)
{
    free(c);
}

uint32_t core_get_pc(core_t *c)
{
    return c->pc;
}

void core_set_pc(core_t *c, uint32_t pc)
{
    c->pc = pc;
}

void core_set_filter(core_t *c, filter_t *f)
{
    c->filter = f;
}

#define SE8(b) ((uint32_t)((int32_t)((int8_t)(b))))
#define SE16(hw) ((uint32_t)((int32_t)((int16_t)(hw))))
#define SIMMED(ins) ((int32_t)((int16_t)IMMED(ins)))

#define ADDR(c, ins) ((c)->r[RS(ins)] + SIMMED(ins))

#define SRA(val, shift) ((uint32_t)(((int32_t)(val)) >> (shift)))

#define JUMP_TARGET(c, ins) (((c)->pc & 0xF0000000) | (TARGET(ins) << 2))
#define BRANCH_TARGET(c, ins) ((c)->pc + (SIMMED(ins) << 2) + 4)
#define LINK(c) (c->r[31] = c->pc + 4)

#define MAX_EXCS 10

int core_step(core_t *c)
{
    int ret;

    ret = __core_step(c);
    if (ret == EXCEPTED) {
        ret = 0;
        c->exc_count++;
        if (c->exc_count >= MAX_EXCS) {
            ret = ERR_EXC_FLOOD;
        }
    } else {
        c->exc_count = 0;
    }
    return ret;
}

int __core_step(core_t *c)
{
    uint32_t ins;
    uint32_t newpc;
    uint8_t b; uint16_t h; uint32_t w;
    int ret;

    ret = core_cp0_step(c, &c->cp0);
    if (ret) { return ret; }

    ret = rdiw(c, c->pc, &ins);
    if (ret) { return ret; }

    if (c->filter) {
        if (!filter_ins_allowed(c->filter, ins)) {
            debug_printf(CORE, INFO,
                    "Unsupported instruction: %08x (PC=%08x)\n",
                    ins, c->pc);
            return except(c, EXC_RI);
        }
    }

    debug_printf(CORE, TRACE,
            "Fetched %08x (OP=%03o RS=%02d RT=%02d RD=%02d "
            "SA=%d FUNCT=%03o IMMED=%04x TARGET=%08x) "
            "from %08x (in %s mode)\n",
            ins, OP(ins), RS(ins), RT(ins), RD(ins),
            SA(ins), FUNCT(ins), IMMED(ins), TARGET(ins),
            c->pc, user_mode(c) ? "user" : "kernel");

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
            if (SA(ins) != 0) { return except(c, EXC_RI); }
            c->r[RD(ins)] = c->r[RT(ins)] << (c->r[RS(ins)] & 0x1F);
            break;
        case FUNCT_SRLV:
            if (SA(ins) != 0) { return except(c, EXC_RI); }
            c->r[RD(ins)] = c->r[RT(ins)] >> (c->r[RS(ins)] & 0x1F);
            break;
        case FUNCT_SRAV:
            if (SA(ins) != 0) { return except(c, EXC_RI); }
            c->r[RD(ins)] = SRA(c->r[RT(ins)], c->r[RS(ins)] & 0x1F);
            break;
        case FUNCT_JR:
            if ((RT(ins) != 0) || (RD(ins) != 0) || (SA(ins) != 0)) {
                return except(c, EXC_RI);
            }
            newpc = c->r[RS(ins)];
            break;
        case FUNCT_JALR:
            if ((RT(ins) != 0) || (SA(ins) != 0)) { return except(c, EXC_RI); }
            if (RS(ins) == RD(ins)) {
                debug_printf(CORE, WARNING,
                        "Undefined behavior: JAL at %08x has rs = rd\n",
                        c->pc);
            }
            newpc = c->r[RS(ins)];
            c->r[RD(ins)] = c->pc + 4;
            break;
        case FUNCT_SYSCALL:
            return except(c, EXC_SYS);
        case FUNCT_TESTDONE:
            if (user_mode(c)) { return except(c, EXC_RI); }
            return ERR_TESTDONE;
        case FUNCT_ADD:
            if (add_overflows(c->r[RS(ins)], c->r[RT(ins)])) {
                return except(c, EXC_OV);
            }
            /* Fall through. */
        case FUNCT_ADDU:
            if (SA(ins) != 0) { return except(c, EXC_RI); }
            c->r[RD(ins)] = c->r[RS(ins)] + c->r[RT(ins)];
            break;
        case FUNCT_SUB:
            if (sub_overflows(c->r[RS(ins)], c->r[RT(ins)])) {
                return except(c, EXC_OV);
            }
            /* Fall through. */
        case FUNCT_SUBU:
            if (SA(ins) != 0) { return except(c, EXC_RI); }
            c->r[RD(ins)] = c->r[RS(ins)] - c->r[RT(ins)];
            break;
        case FUNCT_AND:
            if (SA(ins) != 0) { return except(c, EXC_RI); }
            c->r[RD(ins)] = c->r[RS(ins)] & c->r[RT(ins)];
            break;
        case FUNCT_OR:
            if (SA(ins) != 0) { return except(c, EXC_RI); }
            c->r[RD(ins)] = c->r[RS(ins)] | c->r[RT(ins)];
            break;
        case FUNCT_XOR:
            if (SA(ins) != 0) { return except(c, EXC_RI); }
            c->r[RD(ins)] = c->r[RS(ins)] ^ c->r[RT(ins)];
            break;
        case FUNCT_NOR:
            if (SA(ins) != 0) { return except(c, EXC_RI); }
            c->r[RD(ins)] = ~(c->r[RS(ins)] | c->r[RT(ins)]);
            break;
        case FUNCT_SLT:
            if (SA(ins) != 0) { return except(c, EXC_RI); }
            {
                int32_t a = (int32_t)c->r[RS(ins)];
                int32_t b = (int32_t)c->r[RT(ins)];
                c->r[RD(ins)] = (a < b) ? 1 : 0;
            }
            break;
        case FUNCT_SLTU:
            if (SA(ins) != 0) { return except(c, EXC_RI); }
            c->r[RD(ins)] = (c->r[RS(ins)] < c->r[RT(ins)]) ? 1 : 0;
            break;
        case FUNCT_MULT:
            if ((RD(ins) != 0) || (SA(ins) != 0)) {
                return except(c, EXC_RI);
            }
            {
                int64_t s = (int64_t)((int32_t)c->r[RS(ins)]);
                int64_t t = (int64_t)((int32_t)c->r[RT(ins)]);
                set_hilo(c, (uint64_t)(s * t));
            }
            break;
        case FUNCT_MFHI:
            if ((RS(ins) != 0) || (RT(ins) != 0) || (SA(ins) != 0)) {
                return except(c, EXC_RI);
            }
            c->r[RD(ins)] = c->hi;
            break;
        case FUNCT_MFLO:
            if ((RS(ins) != 0) || (RT(ins) != 0) || (SA(ins) != 0)) {
                return except(c, EXC_RI);
            }
            c->r[RD(ins)] = c->lo;
            break;
        case FUNCT_MTHI:
            if ((RD(ins) != 0) || (RT(ins) != 0) || (SA(ins) != 0)) {
                return except(c, EXC_RI);
            }
            c->hi = c->r[RS(ins)];
            break;
        case FUNCT_MTLO:
            if ((RD(ins) != 0) || (RT(ins) != 0) || (SA(ins) != 0)) {
                return except(c, EXC_RI);
            }
            c->lo = c->r[RS(ins)];
            break;
        case FUNCT_MULTU:
            if ((RD(ins) != 0) || (SA(ins) != 0)) {
                return except(c, EXC_RI);
            }
            set_hilo(c, (uint64_t)c->r[RS(ins)] * (uint64_t)c->r[RT(ins)]);
            break;
        case FUNCT_DIV:
            if ((RD(ins) != 0) || (SA(ins) != 0)) {
                return except(c, EXC_RI);
            }
            if (c->r[RT(ins)] != 0) {
                {
                    int32_t a = (int32_t)c->r[RS(ins)];
                    int32_t b = (int32_t)c->r[RT(ins)];
                    c->lo = (uint32_t)(a / b);
                    c->hi = (uint32_t)(a % b);
                }
            } else {
                c->lo = 0xDEADBEEF;
                c->hi = 0xFEEDFACE;
            }
            break;
        case FUNCT_DIVU:
            if ((RD(ins) != 0) || (SA(ins) != 0)) { return EXC_RI; }
            if (c->r[RT(ins)] != 0) {
                c->lo = c->r[RS(ins)] / c->r[RT(ins)];
                c->hi = c->r[RS(ins)] % c->r[RT(ins)];
            } else {
                c->lo = 0xDEADBEEF;
                c->hi = 0xFEEDFACE;
            }
            break;
        default:
            debug_printf(CORE, DETAIL,
                    "Unimplemented SPECIAL function %03o\n", FUNCT(ins));
            return except(c, EXC_RI);
        }
        break;
    case OP_REGIMM:
        switch (RT(ins)) {
        case REGIMM_BLTZAL:
            if (RS(ins) == 31) {
                debug_printf(CORE, WARNING,
                        "Undefined behavior: BLTZAL at %08x has rs = 31\n",
                        c->pc);
            }
            if ((int32_t)c->r[RS(ins)] < 0) {
                newpc = BRANCH_TARGET(c, ins);
            }
            LINK(c);
            break;
        case REGIMM_BLTZ:
            if ((int32_t)c->r[RS(ins)] < 0) {
                newpc = BRANCH_TARGET(c, ins);
            }
            break;
        case REGIMM_BGEZAL:
            if (RS(ins) == 31) {
                debug_printf(CORE, WARNING,
                        "Undefined behavior: BGEZAL at %08x has rs = 31\n",
                        c->pc);
            }
            if ((int32_t)c->r[RS(ins)] >= 0) {
                newpc = BRANCH_TARGET(c, ins);
            }
            LINK(c);
            break;
        case REGIMM_BGEZ:
            if ((int32_t)c->r[RS(ins)] >= 0) {
                newpc = BRANCH_TARGET(c, ins);
            }
            break;
        default:
            debug_printf(CORE, DETAIL,
                    "Unimplemented REGIMM rt %03o\n", RT(ins));
            return except(c, EXC_RI);
        }
        break;
    case OP_JAL:
        newpc = JUMP_TARGET(c, ins);
        LINK(c);
        break;
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
        if (RT(ins) != 0) { return except(c, EXC_RI); }
        if ((int32_t)c->r[RS(ins)] <= 0) {
            newpc = BRANCH_TARGET(c, ins);
        }
        break;
    case OP_BGTZ:
        if (RT(ins) != 0) { return except(c, EXC_RI); }
        if ((int32_t)c->r[RS(ins)] > 0) {
            newpc = BRANCH_TARGET(c, ins);
        }
        break;
    case OP_ADDI:
        if (add_overflows(c->r[RS(ins)], SIMMED(ins))) {
            return except(c, EXC_OV);
        }
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
        if (RS(ins) != 0) return EXC_RI;
        c->r[RT(ins)] = IMMED(ins) << 16;
        break;
    case OP_LB:
        ret = rdb(c, ADDR(c, ins), &b);
        if (ret) return ret;
        c->r[RT(ins)] = SE8(b);
        break;
    case OP_LH:
        ret = rdh(c, ADDR(c, ins), &h);
        if (ret) { return ret; }
        c->r[RT(ins)] = SE16(h);
        break;
    case OP_LW:
        ret = rdw(c, ADDR(c, ins), &w);
        if (ret) return ret;
        c->r[RT(ins)] = w;
        break;
    case OP_LBU:
        ret = rdb(c, ADDR(c, ins), &b);
        if (ret) return ret;
        c->r[RT(ins)] = (uint32_t)b;
        break;
    case OP_LHU:
        ret = rdh(c, ADDR(c, ins), &h);
        if (ret) return ret;
        c->r[RT(ins)] = (uint32_t)h;
        break;
    case OP_SB:
        b = (uint8_t)c->r[RT(ins)];
        ret = wrb(c, ADDR(c, ins), b);
        if (ret) return ret;
        break;
    case OP_SH:
        h = (uint16_t)c->r[RT(ins)];
        ret = wrh(c, ADDR(c, ins), h);
        if (ret) return ret;
        break;
    case OP_SW:
        w = c->r[RT(ins)];
        ret = wrw(c, ADDR(c, ins), w);
        if (ret) return ret;
        break;
    case OP_COP0:
        switch (RS(ins)) {
        case COP_MF:
            if (user_mode(c)) { return except(c, EXC_RI); }
            ret = core_cp0_move_from(c, &c->cp0, RD(ins), &c->r[RT(ins)]);
            if (ret) return ret;
            break;
        case COP_MT:
            if (user_mode(c)) { return except(c, EXC_RI); }
            ret = core_cp0_move_to(c, &c->cp0, RD(ins), c->r[RT(ins)]);
            if (ret) return ret;
            break;
        case 020:
            if ((RT(ins) != 0) || (RD(ins) != 0) || (SA(ins) != 0)) {
                return except(c, EXC_RI);
            }
            switch (FUNCT(ins)) {
            case CP0_FUNCT_TLBWI:
                ret = core_cp0_tlbwi(c, &c->cp0);
                if (ret) return ret;
                break;
            case CP0_FUNCT_TLBWR:
                ret = core_cp0_tlbwr(c, &c->cp0);
                if (ret) return ret;
                break;
            case CP0_FUNCT_ERET:
                if (user_mode(c)) { return except(c, EXC_RI); }
                ret = core_cp0_eret(c, &c->cp0, &newpc);
                if (ret) return ret;
                break;
            default:
                debug_printf(CORE, DETAIL, "Unimplemented CP0 funct %03o\n",
                        FUNCT(ins));
                return except(c, EXC_RI);
            }
            break;
        default:
            debug_printf(CORE, DETAIL,
                    "Unimplemented COP0 rs %03o\n", RS(ins));
            return except(c, EXC_RI);
        }
        break;
    default:
        debug_printf(CORE, DETAIL, "Unimplemented OP %03o\n", OP(ins));
        return except(c, EXC_RI);
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

    core_cp0_dump_regs(c, &c->cp0, out);
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

static void set_hilo(core_t *c, uint64_t val)
{
    c->lo = (uint32_t)val;
    c->hi = (uint32_t)(val >> 32);
}

static int except(core_t *c, uint8_t exc_code)
{
    if (c->filter) {
        if (!filter_exc_allowed(c->filter, exc_code)) {
            debug_printf(CORE, INFO,
                    "Unsupported exception: %s\n", exc_text[exc_code]);
            return ERR_EXC;
        }
    }

    return core_cp0_except(c, &c->cp0, exc_code);
}

static int except_vm(core_t *c, uint8_t exc_code, uint32_t badvaddr)
{
    int ret;

    ret = core_cp0_move_to(c, &c->cp0, CP0_BADVADDR, badvaddr);
    if (ret) { return ret; }
    return except(c, exc_code);
}

static int user_mode(core_t *c)
{
    return core_cp0_user_mode(c, &c->cp0);
}

static int translate(core_t *c, uint32_t va, uint32_t *pa_out, int write)
{
    if (filter_misc(c->filter, FILTER_MISC_VM)) {
        return core_cp0_translate(c, &c->cp0, va, pa_out, write);
    } else {
        *pa_out = va;
        return 0;
    }
}

static int rdb(core_t *c, uint32_t addr, uint8_t *out)
{
    uint32_t w;
    int ret;

    ret = _rdw(c, addr & ~0x3, &w, 0);
    if (ret) { return ret; }

    *out = (uint8_t)(w >> (8 * (addr & 0x3)));
    return 0;
}

static int rdh(core_t *c, uint32_t addr, uint16_t *out)
{
    uint32_t w;
    int ret;

    if (addr & 0x1) { return except_vm(c, EXC_ADEL, addr); }

    ret = _rdw(c, addr & ~0x3, &w, 0);
    if (ret) { return ret; }

    *out = (uint16_t)(w >> (8 * (addr & 0x3)));
    return 0;
}

static int rdw(core_t *c, uint32_t addr, uint32_t *out)
{
    if (addr & 0x3) { return except_vm(c, EXC_ADEL, addr); }
    return _rdw(c, addr, out, 0);
}

static int rdiw(core_t *c, uint32_t addr, uint32_t *out)
{
    if (addr & 0x3) { return except_vm(c, EXC_ADEL, addr); }
    return _rdw(c, addr, out, 1);
}

static int wrb(core_t *c, uint32_t addr, uint8_t in)
{
    int off = addr & 0x3;
    return _wrw(c, addr & ~0x3, (uint32_t)in << (8 * off), 0x1 << off);
}

static int wrh(core_t *c, uint32_t addr, uint16_t in)
{
    int off = addr & 0x3;
    if (addr & 0x1) { return except_vm(c, EXC_ADES, addr); }
    return _wrw(c, addr & ~0x3, (uint32_t)in << (8 * off), 0x3 << off);
}

static int wrw(core_t *c, uint32_t addr, uint32_t in)
{
    if (addr & 0x3) { return except_vm(c, EXC_ADES, addr); }
    return _wrw(c, addr, in, 0xf);
}

static int _rdw(core_t *c, uint32_t va, uint32_t *out, int ins)
{
    uint32_t pa;
    int ret;

    assert(!(va & 0x3));

    ret = translate(c, va, &pa, 0);
    if (ret) { return ret; }

    ret = mem_read(c->mem, pa, out);
    if (ret) { return except(c, ins ? EXC_IBE : EXC_DBE); }

    return 0;
}

static int _wrw(core_t *c, uint32_t va, uint32_t in, uint8_t we)
{
    uint32_t pa;
    int ret;

    assert(!(va & 0x3));

    ret = translate(c, va, &pa, 1);
    if (ret) { return ret; }

    ret = mem_write(c->mem, pa, in, we);
    if (ret) { return except(c, EXC_DBE); }

    return 0;
}


