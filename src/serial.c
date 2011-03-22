#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "mem_dev.h"
#include "util.h"

typedef struct serial_dev serial_dev_t;
struct serial_dev {
    mem_dev_t dev;
    int infd;
    int outfd;
};

static int serial_read(mem_dev_t *dev, uint32_t offset, uint32_t *val_out);
static int serial_write(mem_dev_t *dev, uint32_t offset,
                        uint32_t val, uint8_t we);

mem_dev_t *serial_create(int infd, int outfd)
{
    serial_dev_t *ser;

    ser = xmalloc(sizeof(*ser));
    ser->dev.size = 0x4;
    ser->dev.read = &serial_read;
    ser->dev.write = &serial_write;
    ser->infd = infd;
    ser->outfd = outfd;

    return (mem_dev_t *)ser;
}

void serial_destroy(mem_dev_t *dev)
{
    serial_dev_t *ser = (serial_dev_t *)dev;

    close(ser->infd);
    if (ser->outfd != ser->infd) {
        close(ser->outfd);
    }
    free(ser);
}

static int serial_read(mem_dev_t *dev, uint32_t offset, uint32_t *val_out)
{
    serial_dev_t *ser = (serial_dev_t *)dev;
    char c;
    int ret;

    assert(offset == 0);

    ret = read(ser->infd, &c, 1);
    if (ret > 0) {
        *val_out = 0x100 | c;
    } else {
        *val_out = 0;
    }
    
    return 0;
}

static int serial_write(mem_dev_t *dev, uint32_t offset,
                        uint32_t val, uint8_t we)
{
    serial_dev_t *ser = (serial_dev_t *)dev;
    char c;
    int ret;

    assert(offset == 0);

    if (!(we & 1)) {
        return 0;
    }

    c = val & 0xFF;

    ret = write(ser->outfd, &c, 1);
    if (ret == 0) {
        debug_print(SERIAL, ERROR, "serial_write: short write\n");
    } else if (ret < 0) {
        debug_printf(SERIAL, ERROR, "serial_write: %s\n", strerror(errno));
    }

    return 0;
}
