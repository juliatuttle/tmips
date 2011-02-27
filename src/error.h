#ifndef ERROR_H
#define ERROR_H

enum {
    ERR_INS = 1,
    ERR_MEM,
    ERR_OFLOW,
    ERR_SYSCALL,
    NUM_ERRS
};

const char *err_text[NUM_ERRS];

#endif
