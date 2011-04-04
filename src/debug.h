#ifndef HAVE_DEBUG_H
#define HAVE_DEBUG_H

typedef enum {
    DEBUG_MODULE_CONFIG,
    DEBUG_MODULE_CORE,
    DEBUG_MODULE_EXC,
    DEBUG_MODULE_MAIN,
    DEBUG_MODULE_MEM,
    DEBUG_MODULE_READMEMH,
    DEBUG_MODULE_RAM,
    DEBUG_MODULE_SERIAL,
    DEBUG_MODULE_UTIL,
    DEBUG_MODULE_VM,
    NUM_DEBUG_MODULES
} debug_module_t;

typedef enum {
    DEBUG_LEVEL_FATAL,
    DEBUG_LEVEL_ERROR,
    DEBUG_LEVEL_WARNING,
    DEBUG_LEVEL_INFO,
    DEBUG_LEVEL_DETAIL,
    DEBUG_LEVEL_TRACE,
    NUM_DEBUG_LEVELS
} debug_level_t;

void debug_init(void);
void debug_set_level(debug_level_t level);
void debug_set_module_level(debug_module_t module, debug_level_t level);
void __debug_printf(debug_module_t module, debug_level_t level, char *fmt,
                    ...);
#define debug_printf(m, l, f, ...) \
        __debug_printf(DEBUG_MODULE_ ## m, DEBUG_LEVEL_ ## l, f, __VA_ARGS__)
#define debug_print(m, l, s) debug_printf(m, l, "%s", s)

#endif
