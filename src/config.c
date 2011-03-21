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
#include "debug.h"
#include "filter.h"
#include "mem.h"
#include "mem_dev.h"
#include "ram.h"
#include "readmemh.h"
#include "serial.h"

static void version(void);
static void usage(char *progn);
static int do_region(mem_t *mem, uint32_t base, uint32_t size, char *file);

int config_parse_args(config_t *cfg, int argc, char *argv[])
{
    int i;
    int saw_dump_file = 0;

    if (argc < 2) {
        debug_print(CONFIG, WARNING, "Processors tend to be unhappy with nothing on their bus.\n");
        debug_printf(CONFIG, WARNING, "(Run \"%s --help\" for more info.)\n", argv[0]);
    }

    for (i = 1; i < argc; ) {
        if (!strcmp(argv[i], "--region") || !strcmp(argv[i], "-r")) {
            uint32_t base, size;
            char *end;

            if (argc - i < 4) {
                debug_print(CONFIG, FATAL, "--region: expected <base> <size> <readmemh-file>\n");
                return 1;
            }
            base = strtoul(argv[i + 1], &end, 16);
            if (*end != '\0') {
                debug_printf(CONFIG, FATAL, "--region: invalid base \"%s\"\n", argv[i + 1]);
                return 1;
            }
            size = strtoul(argv[i + 2], &end, 16);
            if (*end != '\0') {
                debug_printf(CONFIG, FATAL, "--region: invalid size \"%s\"\n", argv[i + 2]);
                return 1;
            }
            if (do_region(cfg->mem, base, size, argv[i + 3])) {
                return 1;
            }
            i += 4;
        } else if (!strcmp(argv[i], "--pc") || !strcmp(argv[i], "-p")) {
            uint32_t pc;
            char *end;

            if (argc - i < 2) {
                debug_print(CONFIG, FATAL, "--pc: expected <initial-pc>\n");
                return 1;
            }
            pc = strtoul(argv[i + 1], &end, 16);
            if (*end != '\0') {
                debug_printf(CONFIG, FATAL, "--pc: invalid pc \"%s\"\n", argv[i + 1]);
                return 1;
            }
            cfg->pc = pc;
            i += 2;
        } else if (!strcmp(argv[i], "--dump") || !strcmp(argv[i], "-d")) {
            if (argc - i < 1) {
                debug_print(CONFIG, FATAL, "--dump: expected <dump-file>\n");
                return 1;
            }
            if (saw_dump_file) {
                debug_print(CONFIG, FATAL, "--dump: may not be specified multiple times\n");
                return 1;
            }
            cfg->dump_file = fopen(argv[i + 1], "w");
            if (!cfg->dump_file) {
                debug_printf(CONFIG, FATAL, "%s: %s\n", argv[i + 1], strerror(errno));
                return 1;
            }
            saw_dump_file = 1;
            i += 2;
        } else if (!strcmp(argv[i], "--console") || !strcmp(argv[i], "-c")) {
            uint32_t addr;
            char *end;

            if (argc - i < 2) {
                debug_print(CONFIG, FATAL, "--console: expected <addr>\n");
                return 1;
            }
            addr = strtoul(argv[i + 1], &end, 16);
            if (*end != '\0') {
                debug_printf(CONFIG, FATAL, "--console: invalid addr \"%s\"\n", argv[i + 1]);
                return 1;
            }
            mem_map(cfg->mem, addr, serial_create(0, 1));
            i += 2;
        } else if (!strcmp(argv[i], "--filter") || !strcmp(argv[i], "-f")) {
            if (argc - i < 2) {
                debug_print(CONFIG, FATAL, "--filter: expected <filter>\n");
                return 1;
            }
            cfg->filter = filter_find(argv[i + 1]);
            if (!cfg->filter) {
                debug_printf(CONFIG, FATAL, "--filter: unknown filter \"%s\"\n", argv[i + 1]);
                return 1;
            }
            i += 2;
        } else if (!strncmp(argv[i], "--filter=", 9)) {
            cfg->filter = filter_find(argv[i] + 9);
            if (!cfg->filter) {
                debug_printf(CONFIG, FATAL, "--filter: unknown filter \"%s\"\n", argv[i] + 9);
                return 1;
            }
            i += 1;
        } else if (!strcmp(argv[i], "--step") || !strcmp(argv[i], "-s")) {
            cfg->step = 1;
            i += 1;
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            usage(argv[0]);
            return 1;
        } else if (!strcmp(argv[i], "--quiet") || !strcmp(argv[i], "-q")) {
            if (cfg->debug > 0) {
                debug_set_level(--cfg->debug);
            }
            i += 1;
        } else if (!strcmp(argv[i], "--verbose") || !strcmp(argv[i], "-v")) {
            if (cfg->debug < NUM_DEBUG_LEVELS - 1) {
                debug_set_level(++cfg->debug);
            }
            i += 1;
        } else if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-V")) {
            version();
            return 1;
        } else {
            debug_printf(CONFIG, FATAL, "Invalid argument \"%s\"\n", argv[i]);
            return 1;
        }
    }

    return 0;
}

static void version(void)
{
    printf(
        "tmips 0.3.0 \"Pull the String\"\n"
        "Copyright (C) 2011 Thomas Tuttle\n"
        "This is free software, and comes with no warranty.\n"
        "See COPYING for details.\n");
}

static void usage(char *progn)
{
    printf(
        "Usage: %s <options>\n"
        "\n"
        "    --region|-r <base> <size> <file>\n"
        "        Maps RAM at the specified base address and size, and loads the\n"
        "        specified readmemh-format file at that address.\n"
        "\n"
        "    --pc|-p <addr>\n"
        "        Sets the initial value of the program counter.\n"
        "\n"
        "    --dump|-d <file>\n"
        "        Dumps the final state of the machine's registers to the specified\n"
        "        file.  (If unspecified, the state is dumped to standard output.)\n"
        "\n"
        "    --console|-c <addr>\n"
        "        Maps a serial console (connected to stdio) at the specified address.\n"
        "\n"
        "    --filter|-f <filter>\n"
        "        Sets a filter to allow only instructions required for a certain lab.\n"
        "        Valid values of filter are: lab1, lab2, lab3\n"
        "\n"
        "    --step|-s\n"
        "        Pause and dump registers after each instruction executes.\n"
        "\n"
        "    --help|-h\n"
        "        Shows this help screen.\n"
        "\n"
        "    --quiet|-q\n"
        "        Decreases the verbosity of the program's output.\n"
        "\n"
        "    --verbose|-v\n"
        "        Increases the verbosity of the program's output.\n"
        "\n"
        "    --version|-V\n"
        "        Prints the program version.\n"
        "\n", progn);
}

static int do_region(mem_t *mem, uint32_t base, uint32_t size, char *file)
{
    mem_map(mem, base, ram_create(size));
    return readmemh_load(mem, base, file);
}
