#ifndef BOOK_INDEX_H
#define BOOK_INDEX_H

#include <MacTypes.h>
#include <Quickdraw.h>
#include <Windows.h>
#include <StandardFile.h>

typedef struct ReaderDoc ReaderDoc;

/* Load an existing .book file, or start building one in the background. */
OSErr BookIndexPrepare(WindowRef w, ReaderDoc* doc, const SFReply* reply);

/* Call often from the main event loop while indexing. */
void BookIndexIdle(WindowRef w, ReaderDoc* doc);

void BookIndexCancelBuild(ReaderDoc* doc);

Boolean BookIndexIsBuilding(ReaderDoc* doc);

Boolean BookIndexBlocksUI(ReaderDoc* doc);

OSErr BookIndexPageOffset(ReaderDoc* doc, short pageNum, long* outOffset);

void BookIndexClose(ReaderDoc* doc);

Boolean BookIndexIsOpen(ReaderDoc* doc);

#endif
