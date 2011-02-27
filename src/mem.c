#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"
#include "util.h"

struct mem {
    mem_region_t *regions;
};

struct mem_region {
    uint32_t base;

    struct mem_dev *dev;

    mem_region_t *prev;
    mem_region_t *next;

    mem_t *mem;
};

static mem_region_t *find_region(mem_t *m, uint32_t addr);

mem_t *mem_create(void)
{
    mem_t *m = xmalloc(sizeof(*m));
    m->regions = NULL;
    return m;
}

void mem_destroy(mem_t *m)
{
    while (m->regions) {
        mem_unmap(m, m->regions);
    }
    free(m);
}

mem_region_t *mem_map(mem_t *m, uint32_t base, mem_dev_t *d)
{
    mem_region_t *r;

    r = xmalloc(sizeof(*r));
    r->base = base;
    r->dev = d;
    r->mem = m;

    r->prev = NULL;
    r->next = m->regions;
    m->regions = r;

    return r;
}

void mem_unmap(mem_t *m, mem_region_t *r)
{
    assert(r->mem == m);

    if (r->prev) {
        r->prev->next = r->next;
    } else {
        m->regions = r->next;
    }
    if (r->next) {
        r->next->prev = r->prev;
    }
    r->mem = NULL;
    free(r);
}

int mem_read(mem_t *m, uint32_t addr, uint32_t *val_out)
{
    mem_region_t *r;

    assert(!(addr & 0x3));

    r = find_region(m, addr);
    if (!r) {
        return 1;
    } else if (r->dev->read) {
        return (r->dev->read)(r->dev, addr - r->base, val_out);
    } else {
        /* Region is write-only. */
        return 1;
    }
}

int mem_write(mem_t *m, uint32_t addr, uint32_t val, uint8_t we)
{
    mem_region_t *r;

    assert(!(addr & 0x3));
    assert(!(we & ~0xF));

    r = find_region(m, addr);
    if (!r) {
        return 1;
    } else if (r->dev->write) {
        return (r->dev->write)(r->dev, addr - r->base, val, we);
    } else {
        /* Region discards writes. */
        return 0;
    }
}



static mem_region_t *find_region(mem_t *m, uint32_t addr)
{
    mem_region_t *r;

    for (r = m->regions; r; r = r->next) {
        if ((r->base <= addr) &&
            (addr + 3 < r->base + r->dev->size)) {
            return r;
        }
    }

    return NULL;
}

