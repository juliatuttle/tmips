#ifndef SERIAL_H
#define SERIAL_H

#include "mem_dev.h"

mem_dev_t *serial_create(int infd, int outfd);
void serial_destroy(mem_dev_t *dev);

#endif
