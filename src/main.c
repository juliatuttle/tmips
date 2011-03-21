#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "core.h"
#include "debug.h"
#include "err.h"
#include "exc.h"
#include "mem.h"
#include "ram.h"
#include "readmemh.h"

int main(int argc, char *argv[])
{
    config_t c;
    int ret;

    debug_init();
    debug_set_level(DEBUG_LEVEL_WARNING);

    c.mem = mem_create();
    c.core = core_create(c.mem);
    c.pc = 0;
    c.dump_file = stdout;
    c.filter = NULL;
    c.step = 0;
    /* Note: config_parse_args calls debug_set_level itself so it will apply
       to messages output as a result of further configuration options. */
    c.debug = DEBUG_LEVEL_WARNING;

    ret = config_parse_args(&c, argc, argv);
    if (ret) {
        return 1;
    }

    core_reset(c.core);
    core_set_pc(c.core, c.pc);
    core_set_filter(c.core, c.filter);

    do {
        if (c.step) {
            core_dump_regs(c.core, stderr);
            {
                int ch;
                do { ch = getchar(); } while ((ch != '\n') && (ch != EOF));
                if (ch == EOF) { c.step = 0; }
            }
        }
        ret = core_step(c.core);
    } while (!ret);

    debug_printf(MAIN, INFO, "Halted: %s.\n", err_text[ret]);
    core_dump_regs(c.core, c.dump_file);

    return 0;
}
