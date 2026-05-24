#ifndef READER_DOC_H
#define READER_DOC_H

#include <stddef.h>
#include <MacTypes.h>
#include <Quickdraw.h>
#include <Dialogs.h>

struct ControlRecord;
typedef struct ControlRecord** ControlHandle;
struct TERec;
typedef struct TERec** TEHandle;

enum {
    kPageBufSize = 8192,
    kPageHistoryMax = 64,
    kReadBufSize = 4096,
    kMaxBookmarks = 32
};

typedef struct ReaderDoc {
    short fileRef;
    const char* memText;
    long memLen;
    long fileLen;
    long pageOffset;
    long nextPageOffset;
    char* pageText;
    size_t pageTextLen;
    Boolean hasFile;
    Boolean canGoBack;
    Boolean canGoForward;
    ControlHandle btnPrev;
    ControlHandle btnNext;
    ControlHandle btnGoTo;
    TEHandle pageNumTE;
    Rect pageNumEditRect;
    short currentPage;
    short totalPages;
    Rect textBox;
    short lineHeight;
    short linesPerPage;
    short maxPixelWidth;
    long pageHistory[kPageHistoryMax];
    short pageHistoryPage[kPageHistoryMax];
    short pageHistoryCount;
    short bookIndexRef;
    long bookPageCount;
    short bookLinesPerPage;
    short bookLineHeight;
    short bookMaxPixelWidth;
    Boolean bookBuilding;
    short bookWriteRef;
    short bookBuildVRefNum;
    Str255 bookBuildName;
    Str255 bookSavedTitle;
    long bookBuildPos;
    long bookBuildSourceLen;
    long bookBuildModDate;
    short bookBuildPage;
    DialogPtr bookBuildDlg;
    Handle bookProgressItem;
    Boolean bookTitleProgress;
    Boolean bookIndexPending;
    Boolean bookAwaitingDisplay;
    short bookSourceVRefNum;
    Str255 bookSourceName;
    short savedLastPage;
    short bookmarkPages[kMaxBookmarks];
    short bookmarkCount;
    long readBufPos;
    short readBufCount;
    unsigned char readBuf[kReadBufSize];
} ReaderDoc;

long ReaderAdvancePage(ReaderDoc* doc, long offset);
long ReaderContentLength(ReaderDoc* doc);

/* Redraw the main window while a .book index is being built. */
void ReaderOnBuildProgress(WindowRef w, ReaderDoc* doc);

#endif
