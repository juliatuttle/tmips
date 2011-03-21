#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "mem.h"
#include "readmemh.h"

enum { ERROR = -1, OK = 0, VALID_EOF = 1 };

typedef struct context context_t;
struct context {
    char *file;
    FILE *f;
    int line;
};

static int read_word(context_t *ctx, int c0, uint32_t *out);

int readmemh_load(mem_t *mem, uint32_t base, char *file)
{
    struct context ctx;
    uint32_t at, w;
    char c;
    int ret;

    at = 0;

    ctx.file = file;
    ctx.line = 0;

    ctx.f = fopen(file, "r");
    if (!ctx.f) {
        debug_printf(READMEMH, ERROR, "%s: %s\n", ctx.file, strerror(errno));
        return 1;
    }

    while ((ret = fread(&c, 1, 1, ctx.f)) == 1) {
        if (c == '@') {
            ret = read_word(&ctx, -1, &at);
            if (ret == ERROR) { return 1; }
        } else if (c == '\n') {
            ret = 0;
            ctx.line++;
        } else if (isxdigit(c)) {
            ret = read_word(&ctx, c, &w);
            if (ret == ERROR) { return 1; }
            ret = mem_write(mem, base + at, w, 0xF);
            if (ret) { 
                debug_printf(READMEMH, ERROR, "%s:%d: Couldn't write word to %08x.\n",
                        file, ctx.line, base + at);
                return 1;
            }
            at += 4;
        } else if (isspace(c)) {
            ret = 0;
        } else {
            debug_printf(READMEMH, ERROR, "%s:%d: invalid character '%c'\n",
                         file, ctx.line, c);
            return 1;
        }

        if (ret == VALID_EOF) {
            ret = 0;
            break;
        }
    }

    if (ret < 0) {
        debug_printf(READMEMH, ERROR, "%s:%d: %s\n", ctx.file, ctx.line, strerror(errno));
        return 1;
    }

    debug_printf(READMEMH, INFO, "Loaded \"%s\" at %08x\n", file, base);

    return 0;
}

static int read_word(struct context *ctx, int c0, uint32_t *out)
{
    char buf[9];
    char *end;
    uint32_t w;
    int ret;

    char have_c0 = c0 >= 0;

    if (have_c0) { buf[0] = c0; }
    ret = fread(buf + have_c0, 1, 8 - have_c0, ctx->f);
    if (ret < 0) {
        debug_printf(READMEMH, ERROR, "%s:%d: %s\n", ctx->file, ctx->line, strerror(errno));
        return ERROR;
    }
    if (ret < 8 - have_c0) {
        debug_printf(READMEMH, ERROR, "%s:%d: unexpected EOF\n", ctx->file, ctx->line);
        return ERROR;
    }
    buf[8] = 0;

    w = strtoul(buf, &end, 16);
    if (*end) {
        debug_printf(READMEMH, ERROR, "%s:%d: invalid word data \"%s\"\n",
                     ctx->file, ctx->line, buf);
        return ERROR;
    }

    *out = w;

    return OK;
}
