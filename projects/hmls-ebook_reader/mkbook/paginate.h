#ifndef PAGINATE_H
#define PAGINATE_H

#include "book_format.h"

enum { kPaginateLineBufSize = 256 };

typedef struct PaginateCtx {
    const unsigned char* data;
    long len;
    BookLayout layout;
} PaginateCtx;

void paginate_init(PaginateCtx* ctx, const unsigned char* data, long len, const BookLayout* layout);

/* Return byte offset where the next page begins (matches ReaderAdvancePage). */
long paginate_advance_page(PaginateCtx* ctx, long offset);

#endif
