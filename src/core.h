#ifndef CORE_H
#define CORE_H

#include <stdint.h>
#include <stdio.h>

#include "filter.h"
#include "mem.h"

typedef struct core core_t;

core_t *core_create(mem_t *m);
void core_destroy(core_t *c);
void core_set_pc(core_t *c, uint32_t pc);
void core_set_filter(core_t *c, filter_t *f);
int core_step(core_t *c);

void core_dump_regs(core_t *c, FILE *f);

#endif
