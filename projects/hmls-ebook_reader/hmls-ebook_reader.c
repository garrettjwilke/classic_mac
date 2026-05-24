#include <Quickdraw.h>
#include <Windows.h>
#include <Menus.h>
#include <Fonts.h>
#include <Resources.h>
#include <TextEdit.h>
#include <TextUtils.h>
#include <Dialogs.h>
#include <Devices.h>
#include <StandardFile.h>
#include <Files.h>
#include <OSUtils.h>
#include <Memory.h>

#include <string.h>

#include "book_index.h"
#include "reader_doc.h"

enum {
    kMenuApple = 128,
    kMenuFile = 129,
    kMenuEdit = 130
};

enum {
    kItemAbout = 1
};

enum {
    kItemOpen = 1,
    kItemClose = 2,
    kItemQuit = 4
};

enum {
    kMenuBarHeight = 20,
    kGrowBoxSize = 15,
    kContentMargin = 8,
    kNavBarHeight = 40,
    kTextBoxExtraHeight = 2,
    kButtonWidth = 80,
    kButtonHeight = 20,
    kPageEditWidth = 48,
    kPageEditHeight = 20,
    kGoButtonWidth = 72,
    kNavItemGap = 10,
    kTextInset = 6,
    kLineBufSize = 256,
    kMinWindowWidth = 240,
    kMinWindowHeight = 180
};

static WindowRef gMainWindow;

static void FillScreenWindow(WindowRef w);
static void LayoutReaderWindow(WindowRef w);
static void DrawReaderPage(WindowRef w);
static void TurnPage(WindowRef w, short direction);
static void GoToPageNumber(WindowRef w, short pageNum);
static void DoContentClick(WindowRef w, Point localPt);
static void DoKeyPage(WindowRef w, long keyMessage);
static void ForceRedrawWindow(WindowRef w);

#ifndef ioDirMask
#define ioDirMask 0x10
#endif

/*
 * Standard File filter: TRUE = hide, FALSE = show (Inside Macintosh).
 */
static pascal Boolean TxtOnlyFileFilter(CInfoPBPtr cpb) {
    StringPtr namePtr;

    if (cpb == NULL) {
        return false;
    }

    if (cpb->dirInfo.ioFlAttrib & ioDirMask) {
        return false;
    }

    namePtr = cpb->dirInfo.ioNamePtr;
    if (namePtr == NULL) {
        return true;
    }

    return !BookIndexNameIsText((ConstStr255Param)namePtr);
}

static void SetButtonTitle(ControlHandle c, const char* title) {
    Str255 ptitle;
    short len = (short)strlen(title);
    if (len > 255) {
        len = 255;
    }
    ptitle[0] = (unsigned char)len;
    memcpy(&ptitle[1], title, len);
    SetControlTitle(c, ptitle);
}

static ReaderDoc* GetDoc(WindowRef w) {
    return (ReaderDoc*)GetWRefCon(w);
}

static void InvalidateReadBuf(ReaderDoc* doc) {
    doc->readBufPos = -1;
    doc->readBufCount = 0;
}

static int ReadRawByte(ReaderDoc* doc, long* pos) {
    long offset = *pos;
    long count;
    long index;
    OSErr err;

    if (doc->fileRef <= 0 || offset >= doc->fileLen) {
        return -1;
    }

    if (doc->readBufPos < 0 || offset < doc->readBufPos
        || offset >= doc->readBufPos + (long)doc->readBufCount) {
        doc->readBufPos = offset;
        count = kReadBufSize;
        if (doc->readBufPos + count > doc->fileLen) {
            count = doc->fileLen - doc->readBufPos;
        }
        if (count <= 0) {
            return -1;
        }
        err = SetFPos(doc->fileRef, fsFromStart, doc->readBufPos);
        if (err != noErr) {
            return -1;
        }
        err = FSRead(doc->fileRef, &count, (Ptr)doc->readBuf);
        if (err != noErr || count == 0) {
            return -1;
        }
        doc->readBufCount = (short)count;
    }

    index = offset - doc->readBufPos;
    (*pos)++;
    return (int)doc->readBuf[index];
}

static int ReadSanitizedChar(ReaderDoc* doc, long* pos) {
    int c = ReadRawByte(doc, pos);
    unsigned char uc;
    unsigned char u2;
    unsigned char u3;

    if (c < 0) {
        return -1;
    }

    uc = (unsigned char)c;
    if (uc < 0x80) {
        return (c == '\n') ? '\r' : c;
    }

    if (uc == 0xE2) {
        c = ReadRawByte(doc, pos);
        u2 = (c < 0) ? 0 : (unsigned char)c;
        c = ReadRawByte(doc, pos);
        u3 = (c < 0) ? 0 : (unsigned char)c;
        if (u2 == 0x80) {
            if (u3 == 0x9C || u3 == 0x9D) {
                return '"';
            }
            if (u3 == 0x98 || u3 == 0x99) {
                return '\'';
            }
            if (u3 == 0x94) {
                return '-';
            }
        }
        return ' ';
    }

    if (uc >= 0xC0) {
        for (;;) {
            c = ReadRawByte(doc, pos);
            if (c < 0) {
                break;
            }
            if (((unsigned char)c & 0xC0) != 0x80) {
                break;
            }
        }
    }
    return ' ';
}

static int ReadMemChar(ReaderDoc* doc, long* pos) {
    long i = *pos;
    unsigned char uc;
    unsigned char u2;
    unsigned char u3;

    if (i >= doc->memLen) {
        return -1;
    }

    (*pos)++;
    uc = (unsigned char)doc->memText[i];
    if (uc < 0x80) {
        return (uc == '\n') ? '\r' : (int)uc;
    }

    if (uc == 0xE2 && i + 2 < doc->memLen) {
        u2 = (unsigned char)doc->memText[i + 1];
        u3 = (unsigned char)doc->memText[i + 2];
        *pos += 2;
        if (u2 == 0x80) {
            if (u3 == 0x9C || u3 == 0x9D) {
                return '"';
            }
            if (u3 == 0x98 || u3 == 0x99) {
                return '\'';
            }
            if (u3 == 0x94) {
                return '-';
            }
        }
        return ' ';
    }

    return ' ';
}

long ReaderContentLength(ReaderDoc* doc) {
    if (doc->fileRef > 0) {
        return doc->fileLen;
    }
    if (doc->memText) {
        return doc->memLen;
    }
    return 0;
}

static void EnsureReaderFont(void) {
    TextFont(3); /* Geneva */
    TextSize(12);
}

static short LinePixelWidth(char* line, short len) {
    if (len <= 0) {
        return 0;
    }
    EnsureReaderFont();
    return TextWidth(line, 0, len);
}

static void TrimTrailingSpaces(char* lineBuf, short* lineLen) {
    while (*lineLen > 0 && lineBuf[*lineLen - 1] == ' ') {
        (*lineLen)--;
    }
    lineBuf[*lineLen] = '\0';
}

static void RewindPos(ReaderDoc* doc, long* pos, short consumed, short keep) {
    long rewindCount = (long)consumed - (long)keep;

    if (rewindCount <= 0) {
        return;
    }
    *pos -= rewindCount;
    if (doc->fileRef > 0) {
        SetFPos(doc->fileRef, fsFromStart, *pos);
        InvalidateReadBuf(doc);
    }
}

/*
 * Read one display line: honor source line breaks (\r), then word-wrap at
 * maxPixelWidth. Returns false at EOF.
 */
static Boolean ReaderReadOneLine(ReaderDoc* doc, long* pos, char* lineBuf, short* lineLen) {
    int ch;
    short len = 0;

    lineBuf[0] = '\0';

    while (len < kLineBufSize - 1) {
        if (doc->fileRef > 0) {
            ch = ReadSanitizedChar(doc, pos);
        } else {
            ch = ReadMemChar(doc, pos);
        }
        if (ch < 0) {
            *lineLen = len;
            return false;
        }

        if (ch == '\r') {
            *lineLen = len;
            return true;
        }

        lineBuf[len++] = (char)ch;
        lineBuf[len] = '\0';

        if (LinePixelWidth(lineBuf, len) > doc->maxPixelWidth) {
            short breakAt = len - 1;

            while (breakAt > 0 && lineBuf[breakAt - 1] != ' ') {
                breakAt--;
            }
            if (breakAt == 0) {
                breakAt = len - 1;
            }

            RewindPos(doc, pos, len, breakAt);
            len = breakAt;
            lineBuf[len] = '\0';
            break;
        }
    }

    *lineLen = len;
    return true;
}

static void AppendLineToPage(ReaderDoc* doc, const char* line, short len) {
    if (doc->pageTextLen + (size_t)len + 2 >= kPageBufSize) {
        return;
    }
    if (doc->pageTextLen > 0) {
        doc->pageText[doc->pageTextLen++] = '\r';
    }
    memcpy(doc->pageText + doc->pageTextLen, line, len);
    doc->pageTextLen += (size_t)len;
    doc->pageText[doc->pageTextLen] = '\0';
}

static long ReaderPaginateFrom(ReaderDoc* doc, long offset, Boolean storePage) {
    char lineBuf[kLineBufSize];
    short lineLen;
    short filled;
    long pos;
    Boolean atEOF;
    long contentLen;

    pos = offset;
    atEOF = false;
    contentLen = ReaderContentLength(doc);

    EnsureReaderFont();

    if (storePage) {
        if (!doc->pageText) {
            return offset;
        }
        doc->pageTextLen = 0;
        doc->pageText[0] = '\0';
        doc->pageOffset = offset;
    }

    if (doc->fileRef > 0) {
        if (SetFPos(doc->fileRef, fsFromStart, offset) != noErr) {
            return offset;
        }
        InvalidateReadBuf(doc);
    }

    for (filled = 0; filled < doc->linesPerPage;) {
        if (!ReaderReadOneLine(doc, &pos, lineBuf, &lineLen)) {
            atEOF = true;
            break;
        }

        TrimTrailingSpaces(lineBuf, &lineLen);
        if (lineLen <= 0) {
            /* Blank source lines must not consume a visible line slot. */
            continue;
        }

        if (storePage) {
            AppendLineToPage(doc, lineBuf, lineLen);
        }
        filled++;
    }

    if (storePage) {
        doc->nextPageOffset = pos;
        doc->canGoBack = doc->currentPage > 1;
        doc->canGoForward = pos < contentLen;
        if (!doc->canGoForward) {
            if (doc->totalPages <= 0 || doc->currentPage > doc->totalPages) {
                doc->totalPages = doc->currentPage;
            }
        }
    }

    if (pos < contentLen) {
        return pos;
    }
    return contentLen;
}

long ReaderAdvancePage(ReaderDoc* doc, long offset) {
    return ReaderPaginateFrom(doc, offset, false);
}

static void SetPageNumberField(ReaderDoc* doc, short page) {
    char digits[8];
    short len = 0;
    short n = page;
    short i;

    if (!doc->pageNumTE) {
        return;
    }

    if (n <= 0) {
        n = 1;
    }

    {
        char temp[8];
        short tempLen = 0;
        while (n > 0 && tempLen < 7) {
            temp[tempLen++] = (char)('0' + (n % 10));
            n /= 10;
        }
        for (i = tempLen - 1; i >= 0; i--) {
            digits[len++] = temp[i];
        }
    }

    TESetText(digits, len, doc->pageNumTE);
}

static short ReadPageNumberField(ReaderDoc* doc) {
    TEHandle te = doc->pageNumTE;
    Handle text;
    long value;
    long i;
    short len;

    if (!te) {
        return 1;
    }

    len = (**te).teLength;
    if (len <= 0) {
        return doc->currentPage > 0 ? doc->currentPage : 1;
    }

    if (len > 8) {
        len = 8;
    }

    text = (**te).hText;
    if (!text) {
        return 1;
    }

    HLock(text);
    value = 0;
    for (i = 0; i < len; i++) {
        char c = (*text)[i];
        if (c >= '0' && c <= '9') {
            value = value * 10 + (c - '0');
            if (value > 30000) {
                break;
            }
        }
    }
    HUnlock(text);

    if (value < 1) {
        value = 1;
    }
    return (short)value;
}

static void UpdatePageNavDisplay(ReaderDoc* doc) {
    SetPageNumberField(doc, doc->currentPage);
}

static Boolean BuildPageAtOffset(ReaderDoc* doc, long offset) {
    if (!doc->pageText) {
        return false;
    }
    (void)ReaderPaginateFrom(doc, offset, true);
    return true;
}

static long ScanToPageOffset(ReaderDoc* doc, short targetPage) {
    long pos = 0;
    short page;

    if (targetPage < 1) {
        return 0;
    }

    for (page = 1; page < targetPage; page++) {
        pos = ReaderAdvancePage(doc, pos);
        if ((page & 3) == 0) {
            SystemTask();
        }
    }
    return pos;
}

static void UpdatePageButtons(ReaderDoc* doc) {
    if (doc->btnPrev) {
        HiliteControl(doc->btnPrev, doc->canGoBack ? 0 : 255);
    }
    if (doc->btnNext) {
        HiliteControl(doc->btnNext, doc->canGoForward ? 0 : 255);
    }
}

static void TextBoxRect(WindowRef w, Rect* box) {
    *box = w->portRect;
    InsetRect(box, kContentMargin, kContentMargin);
    box->bottom -= kNavBarHeight - kTextBoxExtraHeight;
}

static void FillScreenWindow(WindowRef w) {
    Rect screen = qd.screenBits.bounds;
    Rect bounds;

    bounds.left = screen.left;
    bounds.top = screen.top + kMenuBarHeight;
    bounds.right = screen.right;
    bounds.bottom = screen.bottom;

    MoveWindow(w, bounds.left, bounds.top, false);
    SizeWindow(w, bounds.right - bounds.left, bounds.bottom - bounds.top, true);
}

static void LayoutReaderWindow(WindowRef w) {
    ReaderDoc* doc = GetDoc(w);
    Rect box;
    Rect navBar;
    Rect prevRect;
    Rect nextRect;
    Rect goRect;
    short navLeft;
    FontInfo fontInfo;

    if (!doc) {
        return;
    }

    TextBoxRect(w, &box);
    doc->textBox = box;

    EnsureReaderFont();
    GetFontInfo(&fontInfo);
    doc->lineHeight = fontInfo.ascent + fontInfo.descent + fontInfo.leading;
    if (doc->lineHeight < 10) {
        doc->lineHeight = 12;
    }
    {
        short innerHeight = box.bottom - box.top - (kTextInset * 2);
        doc->linesPerPage = innerHeight / doc->lineHeight;
        if (doc->linesPerPage < 4) {
            doc->linesPerPage = 4;
        }
    }
    doc->maxPixelWidth = (box.right - box.left) - (kTextInset * 2);

    if (doc->bookIndexRef > 0
        && (doc->linesPerPage != doc->bookLinesPerPage || doc->lineHeight != doc->bookLineHeight
            || doc->maxPixelWidth != doc->bookMaxPixelWidth)) {
        BookIndexClose(doc);
        doc->totalPages = 0;
    }

    navBar.left = box.left;
    navBar.right = box.right;
    navBar.top = box.bottom + 4;
    navBar.bottom = w->portRect.bottom - kContentMargin;

    {
        short navTop = navBar.top + 10;
        short navBottom = navTop + kButtonHeight;
        short totalWidth = kButtonWidth + kNavItemGap + kPageEditWidth + kNavItemGap
            + kGoButtonWidth + kNavItemGap + kButtonWidth;

        navLeft = navBar.left + ((navBar.right - navBar.left) - totalWidth) / 2;

        SetRect(&prevRect, navLeft, navTop, navLeft + kButtonWidth, navBottom);
        navLeft = prevRect.right + kNavItemGap;

        SetRect(&doc->pageNumEditRect,
            navLeft,
            navTop,
            navLeft + kPageEditWidth,
            navTop + kPageEditHeight);
        navLeft = doc->pageNumEditRect.right + kNavItemGap;

        SetRect(&goRect, navLeft, navTop, navLeft + kGoButtonWidth, navBottom);
        navLeft = goRect.right + kNavItemGap;

        SetRect(&nextRect, navLeft, navTop, navLeft + kButtonWidth, navBottom);
    }

    if (doc->btnPrev) {
        MoveControl(doc->btnPrev, prevRect.left, prevRect.top);
        SizeControl(doc->btnPrev, prevRect.right - prevRect.left, prevRect.bottom - prevRect.top);
    } else {
        doc->btnPrev = NewControl(w, &prevRect, "\p", true, 0, 0, 0, pushButProc, 0);
        SetButtonTitle(doc->btnPrev, "Page Left");
    }

    if (doc->btnNext) {
        MoveControl(doc->btnNext, nextRect.left, nextRect.top);
        SizeControl(doc->btnNext, nextRect.right - nextRect.left, nextRect.bottom - nextRect.top);
    } else {
        doc->btnNext = NewControl(w, &nextRect, "\p", true, 0, 0, 0, pushButProc, 0);
        SetButtonTitle(doc->btnNext, "Page Right");
    }

    if (doc->btnGoTo) {
        MoveControl(doc->btnGoTo, goRect.left, goRect.top);
        SizeControl(doc->btnGoTo, goRect.right - goRect.left, goRect.bottom - goRect.top);
    } else {
        doc->btnGoTo = NewControl(w, &goRect, "\p", true, 0, 0, 0, pushButProc, 0);
        SetButtonTitle(doc->btnGoTo, "Go To Page");
    }

    if (doc->pageNumTE) {
        (**doc->pageNumTE).destRect = doc->pageNumEditRect;
        (**doc->pageNumTE).viewRect = doc->pageNumEditRect;
    } else {
        Rect teRect = doc->pageNumEditRect;
        doc->pageNumTE = TENew(&teRect, &teRect);
        if (doc->pageNumTE) {
            (**doc->pageNumTE).txFont = 3;
            (**doc->pageNumTE).txSize = 12;
            TESetText((Ptr) "1", 1, doc->pageNumTE);
        }
    }

    if ((doc->hasFile || doc->memText) && !BookIndexBlocksUI(doc)) {
        if (doc->currentPage < 1) {
            doc->currentPage = 1;
        }
        BuildPageAtOffset(doc, doc->pageOffset);
        UpdatePageNavDisplay(doc);
        UpdatePageButtons(doc);
    }
}

static void DrawReaderPage(WindowRef w) {
    ReaderDoc* doc = GetDoc(w);
    Rect inner;
    static const char waitMsg[] = "Preparing book...\r\rA .book index file is being\rcreated. Please wait for the\rdialog to close.";

    if (!doc || !doc->pageText) {
        return;
    }

    FrameRect(&doc->textBox);
    inner = doc->textBox;
    InsetRect(&inner, kTextInset, kTextInset);
    EraseRect(&inner);

    TextFont(3);
    TextSize(12);

    if (BookIndexBlocksUI(doc) && doc->hasFile) {
        TETextBox((char*)waitMsg, (long)strlen(waitMsg), &inner, teJustLeft);
    } else if (doc->pageTextLen > 0) {
        TETextBox(doc->pageText, (long)doc->pageTextLen, &inner, teJustLeft);
    }

    if (doc->pageNumTE) {
        FrameRect(&doc->pageNumEditRect);
        TEUpdate(&doc->pageNumEditRect, doc->pageNumTE);
    }
}

static void DeactivatePageField(ReaderDoc* doc) {
    if (doc && doc->pageNumTE && (**doc->pageNumTE).active) {
        TEDeactivate(doc->pageNumTE);
    }
}

static Boolean PageFieldActive(ReaderDoc* doc) {
    return doc && doc->pageNumTE && (**doc->pageNumTE).active;
}

static void InvalidateReader(WindowRef w) {
    ReaderDoc* doc = GetDoc(w);
    if (!doc) {
        return;
    }
    InvalRect(&w->portRect);
}

static void GoToPageNumber(WindowRef w, short pageNum) {
    ReaderDoc* doc = GetDoc(w);
    long offset;

    if (!doc) {
        return;
    }

    if (pageNum < 1) {
        pageNum = 1;
    }
    if (doc->totalPages > 0 && pageNum > doc->totalPages) {
        pageNum = doc->totalPages;
    }
    if (pageNum == doc->currentPage) {
        return;
    }

    doc->pageHistoryCount = 0;

    if (BookIndexIsOpen(doc)) {
        if (pageNum > doc->bookPageCount) {
            pageNum = (short)doc->bookPageCount;
        }
        if (BookIndexPageOffset(doc, pageNum, &offset) == noErr) {
            doc->currentPage = pageNum;
        } else {
            offset = ScanToPageOffset(doc, pageNum);
            doc->currentPage = pageNum;
        }
    } else if (pageNum > doc->currentPage) {
        offset = doc->pageOffset;
        while (doc->currentPage < pageNum) {
            offset = ReaderAdvancePage(doc, offset);
            doc->currentPage++;
            if (offset >= ReaderContentLength(doc)) {
                break;
            }
            SystemTask();
        }
    } else {
        offset = ScanToPageOffset(doc, pageNum);
        doc->currentPage = pageNum;
    }

    BuildPageAtOffset(doc, offset);
    UpdatePageNavDisplay(doc);
    UpdatePageButtons(doc);
    InvalidateReader(w);
}

static void TurnPage(WindowRef w, short direction) {
    ReaderDoc* doc = GetDoc(w);

    if (!doc) {
        return;
    }

    if (direction > 0) {
        if (!doc->canGoForward) {
            return;
        }
        if (doc->pageHistoryCount < kPageHistoryMax) {
            doc->pageHistory[doc->pageHistoryCount] = doc->pageOffset;
            doc->pageHistoryPage[doc->pageHistoryCount] = doc->currentPage;
            doc->pageHistoryCount++;
        }
        doc->currentPage++;
        BuildPageAtOffset(doc, doc->nextPageOffset);
    } else {
        long offset;

        if (doc->currentPage <= 1) {
            return;
        }

        if (doc->pageHistoryCount > 0) {
            doc->pageHistoryCount--;
            doc->currentPage = doc->pageHistoryPage[doc->pageHistoryCount];
            BuildPageAtOffset(doc, doc->pageHistory[doc->pageHistoryCount]);
        } else {
            doc->currentPage--;
            if (BookIndexIsOpen(doc)
                && BookIndexPageOffset(doc, doc->currentPage, &offset) == noErr) {
                BuildPageAtOffset(doc, offset);
            } else {
                offset = ScanToPageOffset(doc, doc->currentPage);
                BuildPageAtOffset(doc, offset);
            }
        }
    }

    UpdatePageNavDisplay(doc);
    UpdatePageButtons(doc);
    InvalidateReader(w);
}

static WindowRef NewReaderWindow(ConstStr255Param title) {
    WindowRef w = GetNewWindow(128, NULL, (WindowPtr)-1);
    ReaderDoc* doc;

    if (!w) {
        return NULL;
    }

    doc = (ReaderDoc*)NewPtrClear(sizeof(ReaderDoc));
    if (!doc) {
        DisposeWindow(w);
        return NULL;
    }

    doc->pageText = (char*)NewPtrClear(kPageBufSize);
    if (!doc->pageText) {
        DisposePtr((Ptr)doc);
        DisposeWindow(w);
        return NULL;
    }

    SetWTitle(w, title);
    SetWRefCon(w, (long)doc);
    FillScreenWindow(w);
    SetPort(w);
    LayoutReaderWindow(w);
    ShowWindow(w);
    return w;
}

static void SetWelcomeText(WindowRef w) {
    ReaderDoc* doc = GetDoc(w);
    static const char welcome[] =
        "hmls ebook reader\r\r"
        "select Open from the File menu to read a .txt file.\r\r"
        "Each screen is one page. Use Page Left and Page Right, "
        "left and right arrow keys, or enter a page number and "
        "Go To Page.\r\r"
        "Long chapters are read from disk in sections; there is "
        "no 32K limit.\r\r"
        "A .book index file is created beside each text file "
        "for fast page jumps.";

    if (!doc) {
        return;
    }

    doc->hasFile = false;
    doc->memText = welcome;
    doc->memLen = (long)strlen(welcome);
    doc->fileRef = 0;
    doc->fileLen = doc->memLen;
    doc->pageHistoryCount = 0;
    doc->pageOffset = 0;
    doc->currentPage = 1;
    doc->totalPages = 0;
    BuildPageAtOffset(doc, 0);
    UpdatePageNavDisplay(doc);
    UpdatePageButtons(doc);
    InvalidateReader(w);
}

static OSErr OpenFromSFReply(const SFReply* reply, short* refNum, long* fileLen) {
    OSErr err;
    WDPBRec wd;

    *refNum = 0;
    *fileLen = 0;

    /*
     * Open immediately while SFGetFile still has the correct volume and
     * working directory. Do not call SetVol (resets dir to volume root).
     */
    err = OpenDF(reply->fName, reply->vRefNum, refNum);
    if (err == noErr) {
        return GetEOF(*refNum, fileLen);
    }

    err = FSOpen(reply->fName, reply->vRefNum, refNum);
    if (err == noErr) {
        return GetEOF(*refNum, fileLen);
    }

    memset(&wd, 0, sizeof(wd));
    wd.ioNamePtr = NULL;
    wd.ioWDIndex = 0;
    wd.ioVRefNum = reply->vRefNum;
    if (PBGetWDInfoSync(&wd) == noErr) {
        err = HOpenDF(wd.ioWDVRefNum, wd.ioWDDirID, reply->fName, fsRdPerm, refNum);
        if (err == noErr) {
            return GetEOF(*refNum, fileLen);
        }
        err = HOpenDF(reply->vRefNum, wd.ioWDDirID, reply->fName, fsRdPerm, refNum);
        if (err == noErr) {
            return GetEOF(*refNum, fileLen);
        }
    }

    return err;
}

static Boolean FileRefLooksLikeBookIndex(short refNum) {
    unsigned char hdr[8];
    long count = 8;
    long eof;
    OSErr err;

    if (refNum <= 0) {
        return false;
    }

    if (GetEOF(refNum, &eof) != noErr) {
        return false;
    }
    /* Real books are much larger than an index file. */
    if (eof > 65536) {
        return false;
    }

    err = SetFPos(refNum, fsFromStart, 0);
    if (err != noErr) {
        return false;
    }

    err = FSRead(refNum, &count, (Ptr)hdr);
    SetFPos(refNum, fsFromStart, 0);

    if (err != noErr || count < 8) {
        return false;
    }

    return hdr[0] == 'B' && hdr[1] == 'O' && hdr[2] == 'O' && hdr[3] == 'K' && hdr[4] == 0 && hdr[5] <= 3;
}

static void StripExtension(ConstStr255Param name, Str255 base) {
    short len = name[0];
    short dot = 0;
    short i;

    if (len > 250) {
        len = 250;
    }
    for (i = 1; i <= len; i++) {
        if (name[i] == '.') {
            dot = i;
        }
    }
    if (dot > 1) {
        len = dot - 1;
    }
    base[0] = (unsigned char)len;
    memcpy(base + 1, name + 1, len);
}

#ifndef fnfErr
#define fnfErr (-43)
#endif

static OSErr OpenTextCandidates(const SFReply* bookReply, Str255 chosenName, short* refNum, long* fileLen) {
    Str255 names[4];
    short nameCount = 0;
    short i;
    SFReply tryReply;
    OSErr err;

    BookIndexTextNameFromBook(bookReply->fName, names[nameCount]);
    nameCount++;

    BookIndexAppendTxtExtension(bookReply->fName, names[nameCount]);
    nameCount++;

    StripExtension(bookReply->fName, names[nameCount]);
    nameCount++;

    if (BookIndexNameIsText(bookReply->fName)) {
        memcpy(names[nameCount], bookReply->fName, bookReply->fName[0] + 1);
        nameCount++;
    }

    for (i = 0; i < nameCount; i++) {
        short j;

        for (j = 0; j < i; j++) {
            if (names[i][0] == names[j][0] && memcmp(names[i] + 1, names[j] + 1, names[i][0]) == 0) {
                break;
            }
        }
        if (j < i) {
            continue;
        }

        tryReply = *bookReply;
        BookIndexCopyToSFName(names[i], tryReply.fName);

        err = OpenFromSFReply(&tryReply, refNum, fileLen);
        if (err == noErr && !FileRefLooksLikeBookIndex(*refNum)) {
            memcpy(chosenName, names[i], names[i][0] + 1);
            return noErr;
        }
        if (*refNum > 0) {
            FSClose(*refNum);
            *refNum = 0;
        }
    }

    return fnfErr;
}

static void AttachBookToWindow(WindowRef w, short refNum, long fileLen, ConstStr255Param title,
    const SFReply* reply) {
    ReaderDoc* doc = GetDoc(w);

    if (!doc) {
        if (refNum > 0) {
            FSClose(refNum);
        }
        return;
    }

    if (doc->fileRef > 0) {
        FSClose(doc->fileRef);
    }

    SetWTitle(w, title);
    doc->fileRef = refNum;
    doc->memText = NULL;
    doc->memLen = 0;
    doc->hasFile = true;
    doc->fileLen = fileLen;
    doc->pageHistoryCount = 0;
    doc->pageOffset = 0;
    doc->nextPageOffset = 0;
    doc->currentPage = 1;
    doc->totalPages = 0;
    doc->bookIndexPending = false;
    InvalidateReadBuf(doc);
    BookIndexClose(doc);

    SetPort(w);
    LayoutReaderWindow(w);

    if (reply) {
        doc->bookAwaitingDisplay = true;
        doc->bookIndexPending = true;
        doc->bookSourceVRefNum = reply->vRefNum;
        memcpy(doc->bookSourceName, reply->fName, reply->fName[0] + 1);
        ForceRedrawWindow(w);
    } else if (doc->currentPage < 1) {
        doc->currentPage = 1;
    }

    if (!reply) {
        BuildPageAtOffset(doc, doc->pageOffset);
        UpdatePageNavDisplay(doc);
        UpdatePageButtons(doc);
        ForceRedrawWindow(w);
    }
}

void DoCloseWindow(WindowRef w) {
    if (!w) {
        return;
    }

    if (GetWindowKind(w) < 0) {
        CloseDeskAcc(GetWindowKind(w));
        return;
    }

    if (w == gMainWindow) {
        ReaderDoc* doc = GetDoc(w);
        if (doc) {
            BookIndexClose(doc);
            if (doc->fileRef > 0) {
                FSClose(doc->fileRef);
                doc->fileRef = 0;
            }
        }
        SetWelcomeText(gMainWindow);
        return;
    }

    {
        ReaderDoc* doc = GetDoc(w);
        if (doc) {
            BookIndexClose(doc);
            if (doc->fileRef > 0) {
                FSClose(doc->fileRef);
            }
            if (doc->pageText) {
                DisposePtr((Ptr)doc->pageText);
            }
            if (doc->btnPrev) {
                DisposeControl(doc->btnPrev);
            }
            if (doc->btnNext) {
                DisposeControl(doc->btnNext);
            }
            if (doc->btnGoTo) {
                DisposeControl(doc->btnGoTo);
            }
            if (doc->pageNumTE) {
                TEDispose(doc->pageNumTE);
            }
            DisposePtr((Ptr)doc);
        }
        DisposeWindow(w);
    }
}

void DoOpenFile(void) {
    SFReply reply;
    Point where = {80, 50};
    short refNum;
    long fileLen;
    OSErr err;
    Str255 textName;
    ReaderDoc* doc;

    SFGetFile(where, "\p", NewFileFilterUPP(TxtOnlyFileFilter), -1, NULL, NULL, &reply);

    if (!gMainWindow) {
        return;
    }

    doc = GetDoc(gMainWindow);
    if (!reply.good) {
        if (doc && !doc->hasFile) {
            SetWelcomeText(gMainWindow);
        }
        return;
    }

    if (!BookIndexNameIsText(reply.fName)) {
        SysBeep(1);
        if (doc && !doc->hasFile) {
            SetWelcomeText(gMainWindow);
        }
        return;
    }

    memcpy(textName, reply.fName, reply.fName[0] + 1);
    BookIndexCopyToSFName(textName, reply.fName);

    err = OpenFromSFReply(&reply, &refNum, &fileLen);
    if (err != noErr) {
        SysBeep(1);
        if (doc && !doc->hasFile) {
            SetWelcomeText(gMainWindow);
        }
        return;
    }

    if (FileRefLooksLikeBookIndex(refNum)) {
        FSClose(refNum);
        SysBeep(1);
        if (doc && !doc->hasFile) {
            SetWelcomeText(gMainWindow);
        }
        return;
    }

    SelectWindow(gMainWindow);
    AttachBookToWindow(gMainWindow, refNum, fileLen, textName, &reply);
}

void AdjustMenus(void) {
    WindowRef w = FrontWindow();
    MenuRef fileMenu = GetMenu(kMenuFile);
    if (w) {
        EnableItem(fileMenu, kItemClose);
    } else {
        DisableItem(fileMenu, kItemClose);
    }

    MenuRef editMenu = GetMenu(kMenuEdit);
    if (w && GetWindowKind(w) < 0) {
        EnableItem(editMenu, 1);
        EnableItem(editMenu, 3);
        EnableItem(editMenu, 4);
        EnableItem(editMenu, 5);
        EnableItem(editMenu, 6);
    } else {
        DisableItem(editMenu, 1);
        DisableItem(editMenu, 3);
        DisableItem(editMenu, 4);
        DisableItem(editMenu, 5);
        DisableItem(editMenu, 6);
    }
}

void DoMenuCommand(long menuCommand) {
    short menuID = HiWord(menuCommand);
    short menuItem = LoWord(menuCommand);
    Str255 str;

    if (menuID == kMenuApple) {
        if (menuItem == kItemAbout) {
            NoteAlert(128, NULL);
        } else {
            GetMenuItemText(GetMenu(kMenuApple), menuItem, str);
            OpenDeskAcc(str);
        }
    } else if (menuID == kMenuFile) {
        switch (menuItem) {
            case kItemOpen:
                DoOpenFile();
                break;
            case kItemClose:
                DoCloseWindow(FrontWindow());
                break;
            case kItemQuit:
                ExitToShell();
                break;
        }
    } else if (menuID == kMenuEdit) {
        if (!SystemEdit(menuItem - 1)) {
            /* Edit command not handled by Desk Accessory */
        }
    }

    HiliteMenu(0);
}

void ReaderOnIndexReady(WindowRef w, ReaderDoc* doc) {
    if (!doc || !w) {
        return;
    }

    doc->bookAwaitingDisplay = false;
    if (!doc->hasFile) {
        return;
    }

    if (doc->currentPage < 1) {
        doc->currentPage = 1;
    }
    doc->pageOffset = 0;
    BuildPageAtOffset(doc, 0);
    UpdatePageNavDisplay(doc);
    UpdatePageButtons(doc);
    ForceRedrawWindow(w);
}

static void ForceRedrawWindow(WindowRef w) {
    ReaderDoc* doc = GetDoc(w);

    if (!doc) {
        return;
    }

    SetPort(w);
    DrawReaderPage(w);
    if (doc->btnPrev) {
        Draw1Control(doc->btnPrev);
    }
    if (doc->btnNext) {
        Draw1Control(doc->btnNext);
    }
    if (doc->btnGoTo) {
        Draw1Control(doc->btnGoTo);
    }
}

void DoUpdate(WindowRef w) {
    ReaderDoc* doc;

    if (GetWindowKind(w) < 0) {
        return;
    }

    SetPort(w);
    BeginUpdate(w);
    EraseRect(&w->portRect);

    doc = GetDoc(w);
    if (doc) {
        DrawReaderPage(w);
        if (doc->btnPrev) {
            Draw1Control(doc->btnPrev);
        }
        if (doc->btnNext) {
            Draw1Control(doc->btnNext);
        }
        if (doc->btnGoTo) {
            Draw1Control(doc->btnGoTo);
        }
    }

    EndUpdate(w);
}

static void DoContentClick(WindowRef w, Point localPt) {
    ReaderDoc* doc = GetDoc(w);
    ControlHandle control;
    short part;

    if (!doc || BookIndexBlocksUI(doc)) {
        return;
    }

    if (doc->pageNumTE && PtInRect(localPt, &doc->pageNumEditRect)) {
        TEActivate(doc->pageNumTE);
        TEClick(localPt, false, doc->pageNumTE);
        return;
    }

    DeactivatePageField(doc);

    control = NULL;
    part = FindControl(localPt, w, &control);
    if (!control || part == 0) {
        return;
    }

    if (control == doc->btnPrev) {
        if (TrackControl(control, localPt, NULL)) {
            TurnPage(w, -1);
        }
        return;
    }

    if (control == doc->btnNext) {
        if (TrackControl(control, localPt, NULL)) {
            TurnPage(w, 1);
        }
        return;
    }

    if (control == doc->btnGoTo) {
        if (TrackControl(control, localPt, NULL)) {
            DeactivatePageField(doc);
            GoToPageNumber(w, ReadPageNumberField(doc));
        }
    }
}

static Boolean KeyIs(long keyMessage, unsigned char code) {
    unsigned char lo = (unsigned char)(keyMessage & charCodeMask);
    unsigned char hi = (unsigned char)((keyMessage >> 8) & 0xFF);
    return lo == code || hi == code;
}

static void DoKeyPage(WindowRef w, long keyMessage) {
    unsigned char lo = (unsigned char)(keyMessage & charCodeMask);
    unsigned char hi = (unsigned char)((keyMessage >> 8) & 0xFF);

    /* Page Up, Apple left (0x1B), or left arrow in char byte (0x1C). */
    if (KeyIs(keyMessage, 0x0B) || KeyIs(keyMessage, 0x1B) || lo == 0x1C) {
        TurnPage(w, -1);
        return;
    }
    /* Page Down, right arrow in char byte (0x1D), or Apple right in hi byte. */
    if (KeyIs(keyMessage, 0x0C) || lo == 0x1D || (hi == 0x1C && lo != 0x1C)) {
        TurnPage(w, 1);
        return;
    }
    if (lo == ' ') {
        TurnPage(w, 1);
    }
}

static void DoGrowWindow(WindowRef w, Point startPt) {
    Rect minBounds;
    Rect screen;
    long growResult;

    SetRect(&minBounds, kMinWindowWidth, kMinWindowHeight, kMinWindowWidth, kMinWindowHeight);
    screen = qd.screenBits.bounds;
    screen.top += kMenuBarHeight;

    growResult = GrowWindow(w, startPt, &minBounds);
    if (growResult != 0) {
        short width = LoWord(growResult);
        short height = HiWord(growResult);
        SizeWindow(w, width, height, true);
        LayoutReaderWindow(w);
        InvalRect(&w->portRect);
    }
}

int main(void) {
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);

    SetMenuBar(GetNewMBar(128));
    AppendResMenu(GetMenu(128), 'DRVR');
    DrawMenuBar();

    InitCursor();

    gMainWindow = NewReaderWindow("\pebook reader");
    if (gMainWindow) {
        DoOpenFile();
    }

    for (;;) {
        EventRecord e;
        WindowRef win;
        ReaderDoc* idleDoc = GetDoc(gMainWindow);
        Boolean building = idleDoc && BookIndexIsBuilding(idleDoc);
        Boolean blocksUI = idleDoc && BookIndexBlocksUI(idleDoc);
        Boolean gotEvent;

        SystemTask();

        if (idleDoc && idleDoc->bookIndexPending && !building) {
            SFReply pendingReply;

            memset(&pendingReply, 0, sizeof(pendingReply));
            pendingReply.good = true;
            pendingReply.vRefNum = idleDoc->bookSourceVRefNum;
            memcpy(pendingReply.fName, idleDoc->bookSourceName, idleDoc->bookSourceName[0] + 1);
            idleDoc->bookIndexPending = false;
            (void)BookIndexPrepare(gMainWindow, idleDoc, &pendingReply);
        }

        if (building) {
            BookIndexIdle(gMainWindow, idleDoc);
        }

        if (idleDoc && idleDoc->pageNumTE) {
            TEIdle(idleDoc->pageNumTE);
        }

        if (building) {
            gotEvent = WaitNextEvent(everyEvent, &e, 0, NULL);
        } else {
            gotEvent = GetNextEvent(everyEvent, &e);
        }

        if (gotEvent) {
            switch (e.what) {
                case keyDown:
                case autoKey:
                    if ((e.modifiers & cmdKey) != 0) {
                        AdjustMenus();
                        DoMenuCommand(MenuKey(e.message & charCodeMask));
                    } else {
                        char key = (char)(e.message & charCodeMask);
                        ReaderDoc* doc;

                        win = FrontWindow();
                        doc = (win && GetWindowKind(win) >= 0) ? GetDoc(win) : NULL;
                        if (PageFieldActive(doc)) {
                            if (key == '\r' || key == 0x03) {
                                GoToPageNumber(win, ReadPageNumberField(doc));
                                DeactivatePageField(doc);
                            } else {
                                TEKey(key, doc->pageNumTE);
                            }
                        } else if (win && GetWindowKind(win) >= 0) {
                            ReaderDoc* keyDoc = GetDoc(win);
                            if (!keyDoc || !BookIndexBlocksUI(keyDoc)) {
                                DoKeyPage(win, e.message);
                            }
                        }
                    }
                    break;
                case mouseDown:
                    switch (FindWindow(e.where, &win)) {
                        case inMenuBar:
                            if (!blocksUI) {
                                AdjustMenus();
                                DoMenuCommand(MenuSelect(e.where));
                            }
                            break;
                        case inDrag:
                            DragWindow(win, e.where, &qd.screenBits.bounds);
                            break;
                        case inGoAway:
                            if (TrackGoAway(win, e.where)) {
                                DoCloseWindow(win);
                            }
                            break;
                        case inGrow:
                            DoGrowWindow(win, e.where);
                            break;
                        case inContent:
                            if (!blocksUI) {
                                if (win != FrontWindow()) {
                                    SelectWindow(win);
                                } else {
                                    SetPort(win);
                                    {
                                        Point localPt = e.where;
                                        GlobalToLocal(&localPt);
                                        DoContentClick(win, localPt);
                                    }
                                }
                            }
                            break;
                        case inSysWindow:
                            SystemClick(&e, win);
                            break;
                    }
                    break;
                case updateEvt:
                    DoUpdate((WindowRef)e.message);
                    break;
                case nullEvent:
                    break;
            }
        }
    }

    return 0;
}
