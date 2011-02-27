#include <assert.h>
#include <stdlib.h>

#include "ram.h"
#include "util.h"

static int ram_read(mem_dev_t *ram, uint32_t offset, uint32_t *val_out);
static int ram_write(mem_dev_t *ram, uint32_t offset, uint32_t val, uint8_t we);

static uint32_t we_to_mask(uint8_t we);

mem_dev_t *ram_create(uint32_t size)
{
    mem_dev_t *d;

    d = xmalloc(sizeof(*d));
    d->size = size;
    d->read = &ram_read;
    d->write = &ram_write;
    d->priv = xmalloc(size);

    return d;
}

void ram_destroy(mem_dev_t *ram)
{
    assert(ram->read == &ram_read);

    free(ram->priv);
    free(ram);
}

static int ram_read(mem_dev_t *ram, uint32_t offset, uint32_t *val_out)
{
    uint32_t *w;

    w = (uint32_t*)((uint8_t*)ram->priv + offset);
    *val_out = *w;

    return 0;
}

static int ram_write(mem_dev_t *ram, uint32_t offset, uint32_t val, uint8_t we)
{
    uint32_t *w;
    uint32_t mask;

    w = (uint32_t*)((uint8_t*)ram->priv + offset);
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
