#ifndef HAVE_EXC_H
#define HAVE_EXC_H

/* All numbered, since they are all defined in the R4400 manual. */
enum {
    EXC_NONE  = -1,
    EXC_INT   =  0,
    EXC_MOD   =  1,
    EXC_TLBL  =  2,
    EXC_TLBS  =  3,
    EXC_ADEL  =  4,
    EXC_ADES  =  5,
    EXC_IBE   =  6,
    EXC_DBE   =  7,
    EXC_SYS   =  8,
    EXC_BP    =  9,
    EXC_RI    = 10,
    EXC_CPU   = 11,
    EXC_OV    = 12,
    EXC_TR    = 13,
    EXC_VCEI  = 14,
    EXC_FPE   = 15,
    EXC_WATCH = 23,
    EXC_VCED  = 31,
    NUM_EXCS
};

const char *exc_text[NUM_EXCS];

#endif
