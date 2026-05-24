#ifndef READER_STATE_H
#define READER_STATE_H

#include <MacTypes.h>

typedef struct ReaderDoc ReaderDoc;

enum {
    kMenuBookmarks = 131,
    kItemAddBookmark = 1,
    kItemDeleteBookmark = 2,
    kItemBookmarkFirst = 4
};

void ReaderStateLoad(ReaderDoc* doc);
void ReaderStateSave(ReaderDoc* doc);

Boolean ReaderStateHasBookmark(const ReaderDoc* doc, short page);
OSErr ReaderStateAddBookmark(ReaderDoc* doc, short page);
OSErr ReaderStateDeleteBookmark(ReaderDoc* doc, short page);

void RebuildBookmarkMenu(const ReaderDoc* doc);

/* Delete the .read sidecar and clear bookmarks and saved page in memory. */
OSErr ReaderStateDeleteFile(ReaderDoc* doc);

#endif
