#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "core.h"
#include "mem.h"
#include "mem_dev.h"
#include "ram.h"
#include "readmemh.h"
#include "serial.h"

static int do_region(mem_t *mem, uint32_t base, uint32_t size, char *file);

int config_parse_args(config_t *cfg, int argc, char *argv[])
{
    int i;
    int saw_dump_file = 0;

    for (i = 1; i < argc; ) {
        if (!strcmp(argv[i], "--region") || !strcmp(argv[i], "-r")) {
            uint32_t base, size;
            char *end;

            if (argc - i < 4) {
                fprintf(stderr, "--region: expected <base> <size> <readmemh-file>\n");
                return 1;
            }
            base = strtoul(argv[i + 1], &end, 16);
            if (*end != '\0') {
                fprintf(stderr, "--region: invalid base \"%s\"\n", argv[i + 1]);
                return 1;
            }
            size = strtoul(argv[i + 2], &end, 16);
            if (*end != '\0') {
                fprintf(stderr, "--region: invalid size \"%s\"\n", argv[i + 2]);
                return 1;
            }
            if (do_region(cfg->mem, base, size, argv[i + 3])) {
                return 1;
            }
            i += 4;
        } else if (!strcmp(argv[i], "--pc")) {
            uint32_t pc;
            char *end;

            if (argc - i < 2) {
                fprintf(stderr, "--pc: expected <initial-pc>\n");
                return 1;
            }
            pc = strtoul(argv[i + 1], &end, 16);
            if (*end != '\0') {
                fprintf(stderr, "--pc: invalid pc \"%s\"\n", argv[i + 1]);
                return 1;
            }
            cfg->pc = pc;
            i += 2;
        } else if (!strcmp(argv[i], "--dump")) {
            if (argc - i < 1) {
                fprintf(stderr, "--dump: expected <dump-file>\n");
                return 1;
            }
            if (saw_dump_file) {
                fprintf(stderr, "--dump: may not be specified multiple times\n");
                return 1;
            }
            cfg->dump_file = fopen(argv[i + 1], "w");
            if (!cfg->dump_file) {
                fprintf(stderr, "%s: %s\n", argv[i + 1], strerror(errno));
                return 1;
            }
            saw_dump_file = 1;
            i += 2;
        } else if (!strcmp(argv[i], "--console")) {
            uint32_t addr;
            char *end;

            if (argc - i < 2) {
                fprintf(stderr, "--console: expected <addr>\n");
                return 1;
            }
            addr = strtoul(argv[i + 1], &end, 16);
            if (*end != '\0') {
                fprintf(stderr, "--console: invalid addr \"%s\"\n", argv[i + 1]);
                return 1;
            }
            mem_map(cfg->mem, addr, serial_create(0, 1));
            i += 2;
        } else {
            fprintf(stderr, "Invalid argument \"%s\"\n", argv[i]);
            return 1;
        }
    }

    return 0;
}

static int do_region(mem_t *mem, uint32_t base, uint32_t size, char *file)
{
    mem_map(mem, base, ram_create(size));
    return readmemh_load(mem, base, file);
}
