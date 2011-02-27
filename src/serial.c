#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mem_dev.h"
#include "util.h"

typedef struct serial_dev serial_dev_t;
struct serial_dev {
    mem_dev_t mem;
    int infd;
    int outfd;
};

static int serial_read(mem_dev_t *dev, uint32_t offset, uint32_t *val_out);
static int serial_write(mem_dev_t *dev, uint32_t offset, uint32_t val, uint8_t we);

mem_dev_t *serial_create(int infd, int outfd)
{
    serial_dev_t *dev;

    dev = xmalloc(sizeof(*dev));
    dev->mem.size = 0x4;
    dev->mem.read = &serial_read;
    dev->mem.write = &serial_write;
    dev->infd = infd;
    dev->outfd = outfd;

    return (mem_dev_t *)dev;
}

void serial_destroy(mem_dev_t *_dev)
{
    serial_dev_t *dev = (serial_dev_t *)_dev;

    close(dev->infd);
    if (dev->outfd != dev->infd) {
        close(dev->outfd);
    }
    free(dev);
}

static int serial_read(mem_dev_t *_dev, uint32_t offset, uint32_t *val_out)
{
    serial_dev_t *dev = (serial_dev_t *)_dev;
    char c;
    int ret;

    assert(offset == 0);

    ret = read(dev->infd, &c, 1);
    if (ret > 0) {
        *val_out = 0x100 | c;
    } else {
        *val_out = 0;
    }
    
    return 0;
}

static int serial_write(mem_dev_t *_dev, uint32_t offset, uint32_t val, uint8_t we)
{
    serial_dev_t *dev = (serial_dev_t *)_dev;
    char c;
    int ret;

    assert(offset == 0);

    if (!(we & 1)) {
        return 0;
    }

    c = val & 0xFF;

    ret = write(dev->outfd, &c, 1);
    if (ret == 0) {
        fprintf(stderr, "serial_write: short write\n");
    } else if (ret < 0) {
        fprintf(stderr, "serial_write: %s\n", strerror(errno));
    }

    return 0;
}
