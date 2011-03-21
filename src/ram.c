#include <assert.h>
#include <stdlib.h>

#include "debug.h"
#include "ram.h"
#include "util.h"

#define RAM_INIT_VALUE 0xDEADBEEF

static int ram_read(mem_dev_t *ram, uint32_t offset, uint32_t *val_out);
static int ram_write(mem_dev_t *ram, uint32_t offset, uint32_t val, uint8_t we);

static uint32_t we_to_mask(uint8_t we);

typedef struct ram_dev ram_dev_t;
struct ram_dev {
    mem_dev_t dev;
    void *data;
};

mem_dev_t *ram_create(uint32_t size)
{
    ram_dev_t *d;
    uint32_t i, *p;

    assert(!(size & 0x3));

    debug_printf(RAM, DETAIL, "Creating RAM (size=%08x)\n", size);

    d = xmalloc(sizeof(*d));
    d->dev.size = size;
    d->dev.read = &ram_read;
    d->dev.write = &ram_write;
    d->data = xmalloc(size);

    p = (uint32_t *)d->data;
    for (i = 0; i < size / 4; i++) {
    	p[i] = RAM_INIT_VALUE;
    }

    return (mem_dev_t *)d;
}

void ram_destroy(mem_dev_t *dev)
{
    ram_dev_t *ram = (ram_dev_t *)dev;
    assert(ram->dev.read == &ram_read);

    free(ram->data);
    free(ram);
}

static int ram_read(mem_dev_t *dev, uint32_t offset, uint32_t *val_out)
{
    ram_dev_t *ram = (ram_dev_t *)dev;
    uint32_t *w;

    w = (uint32_t *)((uint8_t *)ram->data + offset);
    *val_out = *w;

    return 0;
}

static int ram_write(mem_dev_t *dev, uint32_t offset, uint32_t val, uint8_t we)
{
    ram_dev_t *ram = (ram_dev_t *)dev;
    uint32_t *w;
    uint32_t mask;

    w = (uint32_t *)((uint8_t *)ram->data + offset);
    mask = we_to_mask(we);
    *w = (*w & ~mask) | (val & mask);

    return 0;
}

static uint32_t we_to_mask(uint8_t we)
{
    return ((we & 8) ? 0xFF000000 : 0) |
           ((we & 4) ? 0x00FF0000 : 0) |
           ((we & 2) ? 0x0000FF00 : 0) |
           ((we & 1) ? 0x000000FF : 0);
}
