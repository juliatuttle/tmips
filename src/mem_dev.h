#ifndef MEM_DEV_H
#define MEM_DEV_H

#include <stdint.h>

typedef struct mem_dev mem_dev_t;

struct mem_dev {
    uint32_t size;
    int (*read)(mem_dev_t *dev, uint32_t offset, uint32_t *val_out);
    int (*write)(mem_dev_t *dev, uint32_t offset, uint32_t val, uint8_t we);
};

#endif
