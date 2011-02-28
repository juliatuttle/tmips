#include "exc.h"

const char *exc_text[] = {
    [EXC_INT]   = "Interrupt",
    [EXC_MOD]   = "TLB modification exception",
    [EXC_TLBL]  = "TLB exception (load or instruction fetch)",
    [EXC_TLBS]  = "TLB exception (store)",
    [EXC_ADEL]  = "Address error exception (load or instruction fetch)",
    [EXC_ADES]  = "Address error exception (store)",
    [EXC_IBE]   = "Bus error exception (instruction fetch)",
    [EXC_DBE]   = "Bus error exception (data reference: load or store)",
    [EXC_SYS]   = "Syscall exception",
    [EXC_BP]    = "Breakpoint exception",
    [EXC_RI]    = "Reserved instruction exception",
    [EXC_CPU]   = "Coprocessor Unusable exception",
    [EXC_OV]    = "Arithmetic Overflow exception",
    [EXC_TR]    = "Trap exception",
    [EXC_VCEI]  = "Virtual Coherency Exception instruction",
    [EXC_FPE]   = "Floating-Point exception",
    [EXC_WATCH] = "Reference to WatchHi/WatchLo address",
    [EXC_VCED]  = "Virtual Coherency Exception data",
};
