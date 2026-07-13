#ifndef LETTERS_H
#define LETTERS_H

#include <Quickdraw.h>

enum {
    kLetterGlyphSize = 16
};

void DrawLetterGlyph(char letter, const Rect* cellRect, short invert);

#endif
