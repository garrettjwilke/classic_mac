#ifndef EPUB_UTIL_H
#define EPUB_UTIL_H

/* Decode %xx and + in EPUB manifest hrefs. Caller must free result. */
char* epub_decode_url(const char* url);

#endif
