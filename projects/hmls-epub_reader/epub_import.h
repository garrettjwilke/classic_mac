#ifndef EPUB_IMPORT_H
#define EPUB_IMPORT_H

#include <MacTypes.h>
#include <StandardFile.h>

typedef struct ReaderDoc ReaderDoc;

/* Import EPUB into sidecar .book; fills doc->chapters. Returns Mac OSErr. */
OSErr EpubImportFromReply(const SFReply* reply, ReaderDoc* doc);

Boolean EpubNameIsEpub(ConstStr255Param name);

/* book.book from MyBook.epub */
void EpubBookNameFromEpub(ConstStr255Param epubName, Str255 bookName);

#endif
