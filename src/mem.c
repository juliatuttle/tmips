#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
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

    debug_printf(MEM, DETAIL, "Memory mapped at %08x-%08x (%08x)\n",
        base, base + d->size, d->size);

    return r;
}

void mem_unmap(mem_t *m, mem_region_t *r)
{
    assert(r->mem == m);

    debug_printf(MEM, DETAIL, "Memory unmapped at %08x-%08x (%08x)\n",
        r->base, r->base + r->dev->size, r->dev->size);

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
        debug_printf(MEM, DETAIL, "Attempt to read unmapped memory at %08x\n", addr);
        return 1;
    } else if (r->dev->read) {
        debug_printf(MEM, TRACE, "Reading %08x\n", addr);
        return (r->dev->read)(r->dev, addr - r->base, val_out);
    } else {
        debug_printf(MEM, DETAIL, "Attempt to read write-only memory (!) at %08x\n", addr);
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
        debug_printf(MEM, DETAIL, "Attempt to write to unmapped memory at %08x (val=%08x, we=%01x)\n", addr, val, we);
        return 1;
    } else if (r->dev->write) {
        debug_printf(MEM, TRACE, "Writing %08x (val=%08x, we=%01x)\n", addr, val, we);
        return (r->dev->write)(r->dev, addr - r->base, val, we);
    } else {
        debug_printf(MEM, DETAIL, "Attempt to write to read-only memory at %08x (val=%08x, we=%01x)\n", addr, val, we);
        return 1;
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

