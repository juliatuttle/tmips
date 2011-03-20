#ifndef HAVE_ERR_H
#define HAVE_ERR_H

enum {
    ERR_TESTDONE = 1,
    ERR_EXC,
    ERR_EXC_FLOOD,
    NUM_ERRS
};

const char *err_text[NUM_ERRS];

#endif
