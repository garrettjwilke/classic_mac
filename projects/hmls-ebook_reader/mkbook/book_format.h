#ifndef BOOK_FORMAT_H
#define BOOK_FORMAT_H

#include <stdint.h>

/* Must match book_index.c on the Classic reader. */
enum {
    kBookMagic = 0x424F4F4B, /* 'BOOK' */
    kBookVersion = 3,
    kBookHeaderSize = 24
};

/*
 * Default layout for a full-screen 512x342 Classic Mac reader window
 * (Geneva 12, menu bar, nav bar, and insets match hmls-ebook_reader.c).
 */
enum {
    kDefaultScreenWidth = 512,
    kDefaultScreenHeight = 342,
    kDefaultMenuBarHeight = 20,
    kDefaultContentMargin = 8,
    kDefaultNavBarHeight = 40,
    kDefaultTextBoxExtraHeight = 2,
    kDefaultTextInset = 6,
    kDefaultGenevaLineHeight = 16
};

typedef struct BookHeader {
    int32_t magic;
    int16_t version;
    int16_t linesPerPage;
    int16_t lineHeight;
    int16_t maxPixelWidth;
    int32_t sourceLen;
    int32_t sourceModDate;
    int32_t pageCount;
} BookHeader;

typedef struct BookLayout {
    int16_t linesPerPage;
    int16_t lineHeight;
    int16_t maxPixelWidth;
} BookLayout;

static inline int32_t be32(int32_t v) {
    return (int32_t)((((uint32_t)v & 0xFFU) << 24) | (((uint32_t)v & 0xFF00U) << 8)
        | (((uint32_t)v & 0xFF0000U) >> 8) | (((uint32_t)v & 0xFF000000U) >> 24));
}

static inline int16_t be16(int16_t v) {
    return (int16_t)((((uint16_t)v & 0xFFU) << 8) | (((uint16_t)v & 0xFF00U) >> 8));
}

static inline void book_header_to_be(const BookHeader* in, BookHeader* out) {
    out->magic = be32(in->magic);
    out->version = be16(in->version);
    out->linesPerPage = be16(in->linesPerPage);
    out->lineHeight = be16(in->lineHeight);
    out->maxPixelWidth = be16(in->maxPixelWidth);
    out->sourceLen = be32(in->sourceLen);
    out->sourceModDate = be32(in->sourceModDate);
    out->pageCount = be32(in->pageCount);
}

static inline void book_layout_defaults(BookLayout* layout) {
    short windowHeight;
    short boxHeight;
    short innerHeight;
    short boxWidth;

    windowHeight = (short)(kDefaultScreenHeight - kDefaultMenuBarHeight);
    boxHeight = (short)(windowHeight - (kDefaultContentMargin * 2)
        - (kDefaultNavBarHeight - kDefaultTextBoxExtraHeight));
    innerHeight = (short)(boxHeight - (kDefaultTextInset * 2));
    boxWidth = (short)(kDefaultScreenWidth - (kDefaultContentMargin * 2));

    layout->lineHeight = kDefaultGenevaLineHeight;
    layout->linesPerPage = (short)(innerHeight / layout->lineHeight);
    layout->maxPixelWidth = (short)(boxWidth - (kDefaultTextInset * 2));
    if (layout->linesPerPage < 4) {
        layout->linesPerPage = 4;
    }
}

#endif
