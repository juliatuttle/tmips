#ifndef MEM_H
#define MEM_H

#include <stdint.h>

#include "mem_dev.h"

typedef struct mem mem_t;
typedef struct mem_region mem_region_t;

mem_t *mem_create(void);
void mem_destroy(mem_t *mem);
mem_region_t *mem_map(mem_t *mem, uint32_t base, mem_dev_t *dev);
void mem_unmap(mem_t *mem, mem_region_t *rgn);

int mem_read(mem_t *mem, uint32_t addr, uint32_t *val_out);
int mem_write(mem_t *mem, uint32_t addr, uint32_t val, uint8_t we);

#endif
