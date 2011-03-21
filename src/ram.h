#ifndef RAM_H
#define RAM_H

#include <stdint.h>

#include "mem_dev.h"

mem_dev_t *ram_create(uint32_t size);
void ram_destroy(mem_dev_t *ram);

#endif
