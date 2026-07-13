#include "letters.h"

extern const unsigned char gLetterBits[26][32];

void DrawLetterGlyph(char letter, const Rect* cellRect, short invert)
{
    BitMap src;
    Rect srcRect;
    Rect destRect;
    GrafPtr port;
    short index;
    short left;
    short top;

    if (letter >= 'a' && letter <= 'z') {
        letter = (char)(letter - 'a' + 'A');
    }
    if (letter < 'A' || letter > 'Z') {
        return;
    }

    index = (short)(letter - 'A');
    left = (short)(cellRect->left + (cellRect->right - cellRect->left - kLetterGlyphSize) / 2);
    top = (short)(cellRect->top + (cellRect->bottom - cellRect->top - kLetterGlyphSize) / 2);

    SetRect(&srcRect, 0, 0, kLetterGlyphSize, kLetterGlyphSize);
    SetRect(&destRect, left, top, (short)(left + kLetterGlyphSize), (short)(top + kLetterGlyphSize));

    src.baseAddr = (Ptr)gLetterBits[index];
    src.rowBytes = 2;
    src.bounds = srcRect;

    GetPort(&port);
    CopyBits(
        &src,
        &port->portBits,
        &srcRect,
        &destRect,
        invert ? srcBic : srcOr,
        NULL);
}
