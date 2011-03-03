#ifndef HAVE_FILTER_H
#define HAVE_FILTER_H

#include <stdint.h>

typedef struct filter filter_t;

filter_t *filter_find(char *name);
int filter_ins_allowed(filter_t *filter, uint32_t ins);

#endif
