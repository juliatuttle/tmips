#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "exc.h"
#include "opcode.h"

#define OP_OFFSET 0
#define FUNCT_OFFSET (OP_OFFSET + NUM_OPS)
#define REGIMM_OFFSET (FUNCT_OFFSET + NUM_FUNCTS)
#define COP0_OFFSET (REGIMM_OFFSET + NUM_REGIMMS)
#define CP0_FUNCT_OFFSET (COP0_OFFSET + NUM_COPS)
#define EXC_OFFSET (CP0_FUNCT_OFFSET + NUM_CP0_FUNCTS)
#define NUM_SLOTS (EXC_OFFSET + NUM_EXCS)

#define ALLOW_OP(op) [ OP_ ## op ] = 1
#define ALLOW_FUNCT(funct) [ FUNCT_OFFSET + FUNCT_ ## funct ] = 1
#define ALLOW_REGIMM(regimm) [ REGIMM_OFFSET + REGIMM_ ## regimm ] = 1
#define ALLOW_COP0(cop) [ COP0_OFFSET + COP_ ## cop ] = 1
#define ALLOW_CP0_FUNCT(funct) [ CP0_FUNCT_OFFSET + CP0_FUNCT_ ## funct ] = 1
#define ALLOW_EXC(exc) [ EXC_OFFSET + EXC_ ## exc ] = 1

typedef struct filter filter_t;
struct filter {
    char *name;
    filter_t *parent;
    char allowed[NUM_SLOTS];
};

static int check(filter_t *filter, unsigned offset);

static filter_t lab2 = {
    .name = "lab2",
    .parent = NULL,
    .allowed = {
        ALLOW_OP(ADDI),
        ALLOW_OP(ADDIU),
        ALLOW_OP(SLTI),
        ALLOW_OP(SLTIU),
        ALLOW_OP(ANDI),
        ALLOW_OP(ORI),
        ALLOW_OP(XORI),
        ALLOW_OP(LUI),
        ALLOW_OP(LB),
        ALLOW_OP(LH),
        ALLOW_OP(LW),
        ALLOW_OP(LBU),
        ALLOW_OP(LHU),
        ALLOW_OP(SB),
        ALLOW_OP(SH),
        ALLOW_OP(SW),
        ALLOW_FUNCT(SLL),
        ALLOW_FUNCT(SRL),
        ALLOW_FUNCT(SRA),
        ALLOW_FUNCT(SLLV),
        ALLOW_FUNCT(SRLV),
        ALLOW_FUNCT(SRAV),
        ALLOW_FUNCT(SYSCALL),
        ALLOW_FUNCT(ADD),
        ALLOW_FUNCT(ADDU),
        ALLOW_FUNCT(SUB),
        ALLOW_FUNCT(SUBU),
        ALLOW_FUNCT(AND),
        ALLOW_FUNCT(OR),
        ALLOW_FUNCT(XOR),
        ALLOW_FUNCT(NOR),
        ALLOW_FUNCT(SLT),
        ALLOW_FUNCT(SLTU),
        ALLOW_FUNCT(MULT),
        ALLOW_FUNCT(MFHI),
        ALLOW_FUNCT(MFLO),
        ALLOW_FUNCT(MTHI),
        ALLOW_FUNCT(MTLO),
        ALLOW_FUNCT(MULTU),
        ALLOW_FUNCT(DIV),
        ALLOW_FUNCT(DIVU)
    }
};

static filter_t lab1 = {
    .name = "lab1",
    .parent = &lab2,
    .allowed = {
        ALLOW_OP(J),
        ALLOW_OP(JAL),
        ALLOW_OP(BEQ),
        ALLOW_OP(BNE),
        ALLOW_OP(BLEZ),
        ALLOW_OP(BGTZ),
        ALLOW_FUNCT(JR),
        ALLOW_FUNCT(JALR),
        ALLOW_REGIMM(BLTZ),
        ALLOW_REGIMM(BGEZ),
        ALLOW_REGIMM(BLTZAL),
        ALLOW_REGIMM(BGEZAL)
    }
};

static filter_t lab3 = {
    .name = "lab3",
    .parent = &lab1,
    .allowed = { }
};

static filter_t *filters[] = { &lab1, &lab2, &lab3, NULL };

filter_t *filter_find(char *name)
{
    int i;

    for (i = 0; filters[i]; i++) {
        if (!strcmp(filters[i]->name, name)) {
            return filters[i];
        }
    }

    return NULL;
}

int filter_ins_allowed(filter_t *filter, uint32_t ins)
{
    switch (OP(ins)) {
    case OP_SPECIAL:
        return check(filter, FUNCT_OFFSET + FUNCT(ins));
    case OP_REGIMM:
        return check(filter, REGIMM_OFFSET + RT(ins));
    case OP_COP0:
        if (RS(ins) & 020) {
            return check(filter, CP0_FUNCT_OFFSET + FUNCT(ins));
        } else {
            return check(filter, COP0_OFFSET + RS(ins));
        }
    default:
        return check(filter, OP(ins));
    }
}

int filter_exc_allowed(filter_t *filter, uint8_t exc_code)
{
    assert(exc_code < NUM_EXCS);

    return check(filter, EXC_OFFSET + exc_code);
}

static int check(filter_t *filter, unsigned offset)
{
    return filter->allowed[offset] ||
           (filter->parent && (check(filter->parent, offset)));
}
