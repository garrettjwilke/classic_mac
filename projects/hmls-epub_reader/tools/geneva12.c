#include "geneva12.h"

/*
 * Geneva 12 character advance widths (pixels), Mac Roman / ASCII.
 * Measured from Geneva 12 on macOS; geneva12_text_width adds +1 px per glyph
 * so mkbook wraps before Classic TextWidth and index offsets stay aligned.
 */
static const unsigned char kGeneva12Widths[256] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   4,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    4,   4,   6,   8,   8,  11,   9,   4,   5,   5,   6,   8,   4,   5,   4,   6,
    8,   8,   8,   8,   8,   8,   8,   8,   8,   8,   4,   4,   8,   8,   8,   7,
   10,   9,   8,   8,   8,   7,   7,   8,   8,   3,   6,   8,   7,   9,   9,   9,
    7,   9,   7,   7,   8,   8,   9,  11,   7,   7,   7,   5,   6,   5,   8,   8,
    7,   7,   7,   7,   7,   7,   5,   7,   7,   3,   3,   6,   3,  11,   7,   7,
    7,   7,   5,   6,   5,   7,   7,   9,   6,   7,   7,   5,   3,   5,   8,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    4,   4,   8,   8,   8,   8,   3,   8,   7,  11,   4,   6,   8,   0,  11,   7,
    4,   8,   5,   5,   7,   7,   8,   8,   7,   5,   4,   6,  11,  11,  11,   7,
    9,   9,   9,   9,   9,   9,  12,   8,   7,   7,   7,   7,   3,   3,   3,   3,
    8,   9,   9,   9,   9,   9,   9,   8,   9,   8,   8,   8,   8,   7,   7,   7,
    7,   7,   7,   7,   7,   7,  11,   7,   7,   7,   7,   7,   3,   3,   3,   3,
    7,   7,   7,   7,   7,   7,   7,   8,   7,   7,   7,   7,   7,   7,   7,   7,
};

int geneva12_text_width(const char* text, int len) {
    int width = 0;
    int i;
    unsigned char w;

    for (i = 0; i < len; i++) {
        w = kGeneva12Widths[(unsigned char)text[i]];
        if (w > 0) {
            width += (int)w + 1;
        }
    }
    return width;
}
