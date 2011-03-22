#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "debug.h"

static debug_level_t max_level[NUM_DEBUG_MODULES];

void debug_init(void)
{
    debug_set_level(DEBUG_LEVEL_INFO);
}

void debug_set_level(debug_level_t level)
{
    int i;

    assert(level < NUM_DEBUG_LEVELS);

    for (i = 0; i < NUM_DEBUG_MODULES; i++) {
        max_level[i] = level;
    }
}

void debug_set_module_level(debug_module_t module, debug_level_t level)
{
    assert(module < NUM_DEBUG_MODULES);
    assert(level < NUM_DEBUG_LEVELS);

    max_level[module] = level;
}

void __debug_printf(debug_module_t module, debug_level_t level, char *fmt,
                    ...) 
{
    va_list ap;

    assert(module < NUM_DEBUG_MODULES);
    assert(level < NUM_DEBUG_LEVELS);

    if (level > max_level[module])
        return;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}
