#ifndef HAVE_FILTER_H
#define HAVE_FILTER_H

#include <stdint.h>

typedef struct filter filter_t;

enum {
    FILTER_MISC_VM,
    NUM_FILTER_MISCS
};

filter_t *filter_find(char *name);
int filter_ins_allowed(filter_t *filter, uint32_t ins);
int filter_exc_allowed(filter_t *filter, uint8_t exc_code);
int filter_misc(filter_t *filter, int misc);

#endif
