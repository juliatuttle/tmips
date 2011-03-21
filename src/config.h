#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include <stdlib.h>

#include "core.h"
#include "debug.h"
#include "filter.h"
#include "mem.h"

typedef struct config config_t;

struct config {
    core_t *core;
    mem_t *mem;
    uint32_t pc;
    FILE *dump_file;
    filter_t *filter;
    debug_level_t debug;
    int step;
};

int config_parse_args(config_t *cfg, int argc, char *argv[]);

#endif
