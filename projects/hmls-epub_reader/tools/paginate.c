#include "paginate.h"

#include "geneva12.h"

#include <string.h>

static int read_sanitized(const PaginateCtx* ctx, long* pos) {
    long i = *pos;
    unsigned char uc;
    unsigned char u2;
    unsigned char u3;
    int ch;

    if (i >= ctx->len) {
        return -1;
    }

    uc = ctx->data[i];
    if (uc == 0x1B) {
        i++;
        if (i < ctx->len && ctx->data[i] == '[') {
            i++;
            while (i < ctx->len && ctx->data[i] != 'm' && ctx->data[i] != 'h') {
                i++;
            }
            if (i < ctx->len) {
                i++;
            }
        }
        *pos = i;
        return read_sanitized(ctx, pos);
    }

    (*pos)++;
    uc = ctx->data[i];
    if (uc < 0x80) {
        return (uc == '\n') ? '\r' : (int)uc;
    }

    if (uc == 0xE2 && i + 2 < ctx->len) {
        u2 = ctx->data[i + 1];
        u3 = ctx->data[i + 2];
        *pos += 2;
        if (u2 == 0x80) {
            if (u3 == 0x9C || u3 == 0x9D) {
                return '"';
            }
            if (u3 == 0x98 || u3 == 0x99) {
                return '\'';
            }
            if (u3 == 0x94) {
                return '-';
            }
        }
        return ' ';
    }

    if (uc >= 0xC0) {
        while (*pos < ctx->len) {
            ch = (int)ctx->data[*pos];
            if ((ch & 0xC0) != 0x80) {
                break;
            }
            (*pos)++;
        }
    }
    return ' ';
}

static void trim_trailing_spaces(char* lineBuf, short* lineLen) {
    while (*lineLen > 0 && lineBuf[*lineLen - 1] == ' ') {
        (*lineLen)--;
    }
    lineBuf[*lineLen] = '\0';
}

static int read_one_line(PaginateCtx* ctx, long* pos, char* lineBuf, short* lineLen) {
    int ch;
    short len = 0;

    lineBuf[0] = '\0';

    while (len < kPaginateLineBufSize - 1) {
        ch = read_sanitized(ctx, pos);
        if (ch < 0) {
            *lineLen = len;
            return 0;
        }

        if (ch == '\r') {
            *lineLen = len;
            return 1;
        }

        lineBuf[len++] = (char)ch;
        lineBuf[len] = '\0';

        if (geneva12_text_width(lineBuf, len)
            > ctx->layout.maxPixelWidth - kLineWrapMargin) {
            short breakAt = (short)(len - 1);

            while (breakAt > 0 && lineBuf[breakAt - 1] != ' ') {
                breakAt--;
            }
            if (breakAt == 0) {
                breakAt = (short)(len - 1);
            }

            *pos -= (long)(len - breakAt);
            len = breakAt;
            lineBuf[len] = '\0';
            break;
        }
    }

    *lineLen = len;
    return 1;
}

void paginate_init(PaginateCtx* ctx, const unsigned char* data, long len, const BookLayout* layout) {
    ctx->data = data;
    ctx->len = len;
    ctx->layout = *layout;
}

long paginate_advance_page(PaginateCtx* ctx, long offset) {
    char lineBuf[kPaginateLineBufSize];
    short lineLen;
    short filled;
    long pos = offset;
    int at_eof = 0;

    for (filled = 0; filled < ctx->layout.linesPerPage;) {
        if (!read_one_line(ctx, &pos, lineBuf, &lineLen)) {
            at_eof = 1;
            break;
        }

        trim_trailing_spaces(lineBuf, &lineLen);
        if (lineLen <= 0) {
            continue;
        }

        filled++;
    }

    if (at_eof || pos >= ctx->len) {
        return ctx->len;
    }
    return pos;
}
