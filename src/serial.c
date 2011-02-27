#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mem_dev.h"
#include "util.h"

typedef struct serial_priv serial_priv_t;
struct serial_priv {
    int infd;
    int outfd;
};

static int serial_read(mem_dev_t *dev, uint32_t offset, uint32_t *val_out);
static int serial_write(mem_dev_t *dev, uint32_t offset, uint32_t val, uint8_t we);
static serial_priv_t *serial_priv(mem_dev_t *dev);

mem_dev_t *serial_create(int infd, int outfd)
{
    serial_priv_t *priv;
    mem_dev_t *dev;

    priv = xmalloc(sizeof(*priv));
    priv->infd = infd;
    priv->outfd = outfd;

    dev = xmalloc(sizeof(*dev));
    dev->size = 0x4;
    dev->read = &serial_read;
    dev->write = &serial_write;
    dev->priv = priv;

    return dev;
}

void serial_destroy(mem_dev_t *dev)
{
    serial_priv_t *priv;

    priv = serial_priv(dev);

    close(priv->infd);
    if (priv->outfd != priv->infd) {
        close(priv->outfd);
    }
    free(priv);
    free(dev);
}

static int serial_read(mem_dev_t *dev, uint32_t offset, uint32_t *val_out)
{
    serial_priv_t *priv;
    char c;
    int ret;

    assert(offset == 0);

    priv = serial_priv(dev);

    ret = read(priv->infd, &c, 1);
    if (ret > 0) {
        *val_out = 0x100 | c;
    } else {
        *val_out = 0;
    }
    
    return 0;
}

static int serial_write(mem_dev_t *dev, uint32_t offset, uint32_t val, uint8_t we)
{
    serial_priv_t *priv;
    char c;
    int ret;

    assert(offset == 0);

    if (!(we & 1)) {
        return 0;
    }

    priv = serial_priv(dev);

    c = val & 0xFF;

    ret = write(priv->outfd, &c, 1);
    if (ret == 0) {
        fprintf(stderr, "serial_write: short write\n");
    } else if (ret < 0) {
        fprintf(stderr, "serial_write: %s\n", strerror(errno));
    }

    return 0;
}

static serial_priv_t *serial_priv(mem_dev_t *dev)
{
    return (serial_priv_t*)dev->priv;
}
