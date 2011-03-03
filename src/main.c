#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "core.h"
#include "exc.h"
#include "mem.h"
#include "ram.h"
#include "readmemh.h"

int main(int argc, char *argv[])
{
    config_t c;
    int ret;

    c.mem = mem_create();
    c.core = core_create(c.mem);
    c.pc = 0;
    c.dump_file = stdout;

    ret = config_parse_args(&c, argc, argv);
    if (ret) {
        return 1;
    }

    core_reset(c.core);
    core_set_pc(c.core, c.pc);

    do {
        ret = core_step(c.core);
    } while (!ret);

    fprintf(c.dump_file, "Halted: %s.\n", /* exc_text[ret] */ "TODO");
    core_dump_regs(c.core, c.dump_file);

    return 0;
}
