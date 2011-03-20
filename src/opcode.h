#ifndef OPCODE_H
#define OPCODE_H

#define BITS_(ins, off, len) (((ins) >> (off)) & ((1U << (len)) - 1))
#define BITS(ins, top, bot) BITS_(ins, bot, top - bot + 1)

#define OP(ins)     BITS(ins, 31, 26)
#define RS(ins)     BITS(ins, 25, 21)
#define RT(ins)     BITS(ins, 20, 16)
#define RD(ins)     BITS(ins, 15, 11)
#define SA(ins)     BITS(ins, 10,  6)
#define FUNCT(ins)  BITS(ins,  5,  0)
#define IMMED(ins)  BITS(ins, 15,  0)
#define TARGET(ins) BITS(ins, 25,  0)

/* Specified in octal, as used on p. A-181 of the MIPS R4000 User's Manual */

enum {
    OP_SPECIAL = 000,
    OP_REGIMM  = 001,
    OP_J       = 002,
    OP_JAL     = 003,
    OP_BEQ     = 004,
    OP_BNE     = 005,
    OP_BLEZ    = 006,
    OP_BGTZ    = 007,

    OP_ADDI    = 010,
    OP_ADDIU   = 011,
    OP_SLTI    = 012,
    OP_SLTIU   = 013,
    OP_ANDI    = 014,
    OP_ORI     = 015,
    OP_XORI    = 016,
    OP_LUI     = 017,

    OP_COP0    = 020,

    OP_LB      = 040,
    OP_LH      = 041,

    OP_LW      = 043,
    OP_LBU     = 044,
    OP_LHU     = 045,

    OP_SB      = 050,
    OP_SH      = 051,

    OP_SW      = 053,

    NUM_OPS   = 0100
};

enum {
    FUNCT_SLL      = 000,
    FUNCT_SRL      = 002,
    FUNCT_SRA      = 003,
    FUNCT_SLLV     = 004,
    FUNCT_SRLV     = 006,
    FUNCT_SRAV     = 007,

    FUNCT_JR       = 010,
    FUNCT_JALR     = 011,
    FUNCT_SYSCALL  = 014,

    FUNCT_TESTDONE = 016,

    FUNCT_MFHI     = 020,
    FUNCT_MTHI     = 021,
    FUNCT_MFLO     = 022,
    FUNCT_MTLO     = 023,

    FUNCT_MULT     = 030,
    FUNCT_MULTU    = 031,
    FUNCT_DIV      = 032,
    FUNCT_DIVU     = 033,

    FUNCT_ADD      = 040,
    FUNCT_ADDU     = 041,
    FUNCT_SUB      = 042,
    FUNCT_SUBU     = 043,
    FUNCT_AND      = 044,
    FUNCT_OR       = 045,
    FUNCT_XOR      = 046,
    FUNCT_NOR      = 047,

    FUNCT_SLT      = 052,
    FUNCT_SLTU     = 053,

    NUM_FUNCTS    = 0100
};

enum {
    REGIMM_BLTZ   = 000,
    REGIMM_BGEZ   = 001,

    REGIMM_BLTZAL = 020,
    REGIMM_BGEZAL = 021,

    NUM_REGIMMS   = 040
};

enum {
    COP_MF   = 000,

    COP_MT   = 004,

    NUM_COPS = 040
};

enum {
    CP0_FUNCT_ERET  = 030,

    NUM_CP0_FUNCTS = 0100
};

#endif
