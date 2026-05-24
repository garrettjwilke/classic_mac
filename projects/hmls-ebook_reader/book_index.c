#include "book_index.h"
#include "reader_doc.h"

#include <Dialogs.h>
#include <Files.h>
#include <OSUtils.h>
#include <Quickdraw.h>
#include <TextUtils.h>
#include <Windows.h>

#include <string.h>

#ifndef dupErr
#define dupErr (-48)
#endif

enum {
    kBookMagic = 0x424F4F4B, /* 'BOOK' */
    kBookVersion = 1,
    kBookHeaderSize = 24,
    kBookDialogID = 129,
    kBookProgressItem = 2,
    kPagesPerIdle = 1
};

typedef struct BookHeader {
    long magic;
    short version;
    short linesPerPage;
    short lineHeight;
    short maxPixelWidth;
    long sourceLen;
    long sourceModDate;
    long pageCount;
} BookHeader;

static void BookFileName(ConstStr255Param txtName, Str255 bookName) {
    short len = txtName[0];
    short dot = 0;
    short i;

    if (len > 250) {
        len = 250;
    }
    bookName[0] = (unsigned char)len;
    memcpy(bookName + 1, txtName + 1, len);

    for (i = 1; i <= len; i++) {
        if (bookName[i] == '.') {
            dot = i;
        }
    }

    if (dot > 0 && dot + 4 <= 255) {
        bookName[dot + 1] = 'b';
        bookName[dot + 2] = 'o';
        bookName[dot + 3] = 'o';
        bookName[dot + 4] = 'k';
        bookName[0] = (unsigned char)(dot + 4);
    } else if (len + 5 <= 255) {
        bookName[++len] = '.';
        bookName[++len] = 'b';
        bookName[++len] = 'o';
        bookName[++len] = 'o';
        bookName[++len] = 'k';
        bookName[0] = (unsigned char)len;
    }
}

static OSErr OpenBookFile(ConstStr255Param name, short vRefNum, signed char permission, short* refNum) {
    OSErr err;
    WDPBRec wd;

    *refNum = 0;
    err = OpenDF(name, vRefNum, refNum);
    if (err == noErr) {
        return noErr;
    }

    err = FSOpen(name, vRefNum, refNum);
    if (err == noErr) {
        return noErr;
    }

    memset(&wd, 0, sizeof(wd));
    wd.ioNamePtr = NULL;
    wd.ioWDIndex = 0;
    wd.ioVRefNum = vRefNum;
    if (PBGetWDInfoSync(&wd) == noErr) {
        err = HOpenDF(wd.ioWDVRefNum, wd.ioWDDirID, name, permission, refNum);
        if (err == noErr) {
            return noErr;
        }
        err = HOpenDF(vRefNum, wd.ioWDDirID, name, permission, refNum);
        if (err == noErr) {
            return noErr;
        }
    }

    return err;
}

static OSErr ReadHeader(short refNum, BookHeader* hdr) {
    long count = kBookHeaderSize;
    OSErr err;

    err = SetFPos(refNum, fsFromStart, 0);
    if (err != noErr) {
        return err;
    }
    err = FSRead(refNum, &count, (Ptr)hdr);
    if (err != noErr || count != kBookHeaderSize) {
        return err != noErr ? err : eofErr;
    }
    return noErr;
}

static OSErr WriteHeader(short refNum, const BookHeader* hdr) {
    long count = kBookHeaderSize;
    OSErr err;

    err = SetFPos(refNum, fsFromStart, 0);
    if (err != noErr) {
        return err;
    }
    return FSWrite(refNum, &count, (Ptr)hdr);
}

static long IndexPageCountFromFile(short refNum, const BookHeader* hdr) {
    long eof;
    long fromFile;

    if (hdr->pageCount > 0) {
        return hdr->pageCount;
    }
    if (GetEOF(refNum, &eof) != noErr || eof <= kBookHeaderSize) {
        return 0;
    }
    fromFile = (eof - kBookHeaderSize) / 4;
    return fromFile > 0 ? fromFile : 0;
}

static Boolean HeaderMatchesBook(short refNum, const BookHeader* hdr, ReaderDoc* doc, long sourceLen,
    long modDate) {
    if (hdr->magic != kBookMagic || hdr->version != kBookVersion) {
        return false;
    }
    if (hdr->sourceLen != sourceLen || hdr->sourceModDate != modDate) {
        return false;
    }
    if (hdr->linesPerPage != doc->linesPerPage || hdr->lineHeight != doc->lineHeight
        || hdr->maxPixelWidth != doc->maxPixelWidth) {
        return false;
    }
    if (IndexPageCountFromFile(refNum, hdr) < 1) {
        return false;
    }
    return true;
}

static OSErr RepairIndexHeader(short refNum, BookHeader* hdr) {
    long pages = IndexPageCountFromFile(refNum, hdr);

    if (pages < 1 || hdr->pageCount > 0) {
        return noErr;
    }
    hdr->pageCount = pages;
    return WriteHeader(refNum, hdr);
}

static void ApplyHeaderToDoc(ReaderDoc* doc, const BookHeader* hdr, short refNum) {
    long pages = IndexPageCountFromFile(refNum, hdr);

    doc->bookIndexRef = refNum;
    doc->bookPageCount = pages;
    doc->totalPages = (short)pages;
    doc->bookLinesPerPage = hdr->linesPerPage;
    doc->bookLineHeight = hdr->lineHeight;
    doc->bookMaxPixelWidth = hdr->maxPixelWidth;
    if (doc->totalPages <= 0) {
        doc->totalPages = 1;
    }
}

static OSErr TryOpenExistingIndex(ReaderDoc* doc, ConstStr255Param bookName, short vRefNum, long sourceLen,
    long modDate) {
    BookHeader hdr;
    short refNum;
    OSErr err;

    err = OpenBookFile(bookName, vRefNum, fsRdPerm, &refNum);
    if (err != noErr) {
        return err;
    }

    err = ReadHeader(refNum, &hdr);
    if (err != noErr) {
        FSClose(refNum);
        return err;
    }

    if (hdr.pageCount < 1 && IndexPageCountFromFile(refNum, &hdr) > 0) {
        short wrRef;
        OSErr repairErr;

        FSClose(refNum);
        repairErr = OpenBookFile(bookName, vRefNum, fsWrPerm, &wrRef);
        if (repairErr == noErr) {
            repairErr = ReadHeader(wrRef, &hdr);
            if (repairErr == noErr) {
                (void)RepairIndexHeader(wrRef, &hdr);
            }
            FSClose(wrRef);
        }
        err = OpenBookFile(bookName, vRefNum, fsRdPerm, &refNum);
        if (err != noErr) {
            return err;
        }
        err = ReadHeader(refNum, &hdr);
        if (err != noErr) {
            FSClose(refNum);
            return err;
        }
    }

    if (HeaderMatchesBook(refNum, &hdr, doc, sourceLen, modDate)) {
        ApplyHeaderToDoc(doc, &hdr, refNum);
        return noErr;
    }

    FSClose(refNum);
    return paramErr;
}

static DialogPtr BeginBuildDialog(ReaderDoc* doc) {
    DialogPtr dlg;
    DialogItemType type;
    Handle item;
    Rect box;
    GrafPtr oldPort;
    WindowPtr dlgWin;

    dlg = GetNewDialog(kBookDialogID, NULL, (WindowPtr)-1);
    if (!dlg) {
        doc->bookTitleProgress = true;
        return NULL;
    }

    doc->bookTitleProgress = false;
    doc->bookBuildDlg = dlg;
    doc->bookProgressItem = NULL;

    GetDialogItem(dlg, kBookProgressItem, &type, &item, &box);
    doc->bookProgressItem = item;

    dlgWin = (WindowPtr)dlg;
    ShowWindow(dlgWin);
    SelectWindow(dlgWin);
    oldPort = qd.thePort;
    SetPort(dlg);
    DrawDialog(dlg);
    SetPort(oldPort);
    return dlg;
}

static void PageNumberToStr255(short page, Str255 msg) {
    char buf[24];
    short len = 0;
    short n = page;
    short i;
    char digits[8];
    short dlen = 0;

    if (n <= 0) {
        n = 0;
    }
    while (n > 0 && dlen < 7) {
        digits[dlen++] = (char)('0' + (n % 10));
        n /= 10;
    }

    buf[len++] = 'P';
    buf[len++] = 'a';
    buf[len++] = 'g';
    buf[len++] = 'e';
    buf[len++] = ' ';
    for (i = dlen - 1; i >= 0; i--) {
        buf[len++] = digits[i];
    }
    buf[len++] = '.';
    buf[len++] = '.';
    buf[len++] = '.';

    msg[0] = (unsigned char)len;
    memcpy(msg + 1, buf, len);
}

static void SetBuildProgress(WindowRef w, ReaderDoc* doc, short page) {
    Str255 msg;

    if (page <= 0) {
        msg[0] = 10;
        memcpy(msg + 1, "Starting...", 10);
    } else {
        PageNumberToStr255(page, msg);
    }

    if (doc->bookProgressItem) {
        SetDialogItemText(doc->bookProgressItem, msg);
        if (doc->bookBuildDlg) {
            GrafPtr oldPort = qd.thePort;
            SetPort(doc->bookBuildDlg);
            DrawDialog(doc->bookBuildDlg);
            SetPort(oldPort);
        }
    }

    if (doc->bookTitleProgress && w) {
        Str255 title;
        short i;

        title[0] = doc->bookSavedTitle[0];
        for (i = 1; i <= title[0]; i++) {
            title[i] = doc->bookSavedTitle[i];
        }
        if (title[0] + msg[0] + 2 <= 255) {
            title[++title[0]] = ' ';
            title[++title[0]] = '-';
            title[++title[0]] = ' ';
            for (i = 1; i <= msg[0]; i++) {
                title[++title[0]] = msg[i];
            }
        }
        SetWTitle(w, title);
    }
}

static void ServiceBuildDialog(ReaderDoc* doc) {
    GrafPtr oldPort;

    if (!doc->bookBuildDlg) {
        return;
    }

    oldPort = qd.thePort;
    SetPort(doc->bookBuildDlg);
    DrawDialog(doc->bookBuildDlg);
    SetPort(oldPort);
}

static void EndBuildDialog(WindowRef w, ReaderDoc* doc) {
    if (doc->bookBuildDlg) {
        DisposeDialog(doc->bookBuildDlg);
        doc->bookBuildDlg = NULL;
        doc->bookProgressItem = NULL;
    }
    if (doc->bookTitleProgress && w) {
        SetWTitle(w, doc->bookSavedTitle);
        doc->bookTitleProgress = false;
    }
}

static OSErr CreateEmptyBook(ConstStr255Param bookName, short vRefNum, short* refNum) {
    OSErr err;

    err = Create(bookName, vRefNum, 'RDR ', 'BOOK');
    if (err != noErr && err != dupErr) {
        return err;
    }
    return OpenBookFile(bookName, vRefNum, fsWrPerm, refNum);
}

static OSErr StartBookBuild(WindowRef w, ReaderDoc* doc, ConstStr255Param bookName, short vRefNum,
    long sourceLen, long modDate) {
    BookHeader hdr;
    OSErr err;

    GetWTitle(w, doc->bookSavedTitle);
    BeginBuildDialog(doc);
    SetBuildProgress(w, doc, 0);

    err = CreateEmptyBook(bookName, vRefNum, &doc->bookWriteRef);
    if (err != noErr) {
        EndBuildDialog(w, doc);
        return err;
    }

    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = kBookMagic;
    hdr.version = kBookVersion;
    hdr.linesPerPage = doc->linesPerPage;
    hdr.lineHeight = doc->lineHeight;
    hdr.maxPixelWidth = doc->maxPixelWidth;
    hdr.sourceLen = sourceLen;
    hdr.sourceModDate = modDate;
    hdr.pageCount = 0;

    err = WriteHeader(doc->bookWriteRef, &hdr);
    if (err != noErr) {
        FSClose(doc->bookWriteRef);
        doc->bookWriteRef = 0;
        EndBuildDialog(w, doc);
        return err;
    }

    doc->bookBuilding = true;
    doc->bookBuildVRefNum = vRefNum;
    memcpy(doc->bookBuildName, bookName, bookName[0] + 1);
    doc->bookBuildPos = 0;
    doc->bookBuildPage = 0;
    doc->bookBuildSourceLen = sourceLen;
    doc->bookBuildModDate = modDate;
    return noErr;
}

static OSErr FinishBookBuild(WindowRef w, ReaderDoc* doc) {
    BookHeader hdr;
    OSErr err;
    short readRef;

    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = kBookMagic;
    hdr.version = kBookVersion;
    hdr.linesPerPage = doc->linesPerPage;
    hdr.lineHeight = doc->lineHeight;
    hdr.maxPixelWidth = doc->maxPixelWidth;
    hdr.sourceLen = doc->bookBuildSourceLen;
    hdr.sourceModDate = doc->bookBuildModDate;
    hdr.pageCount = doc->bookBuildPage;

    err = WriteHeader(doc->bookWriteRef, &hdr);
    FSClose(doc->bookWriteRef);
    doc->bookWriteRef = 0;
    doc->bookBuilding = false;
    EndBuildDialog(w, doc);

    if (err != noErr) {
        return err;
    }

    err = OpenBookFile(doc->bookBuildName, doc->bookBuildVRefNum, fsRdPerm, &readRef);
    if (err != noErr) {
        return err;
    }
    err = ReadHeader(readRef, &hdr);
    if (err != noErr || !HeaderMatchesBook(readRef, &hdr, doc, doc->bookBuildSourceLen, doc->bookBuildModDate)) {
        FSClose(readRef);
        return err != noErr ? err : paramErr;
    }

    ApplyHeaderToDoc(doc, &hdr, readRef);
    return noErr;
}

static OSErr BuildStep(WindowRef w, ReaderDoc* doc) {
    long contentLen;
    long writeCount;
    OSErr err;
    short batch;

    if (!doc->bookBuilding || doc->bookWriteRef <= 0) {
        return noErr;
    }

    contentLen = ReaderContentLength(doc);
    err = noErr;

    for (batch = 0; batch < kPagesPerIdle; batch++) {
        long next;

        writeCount = 4;
        err = FSWrite(doc->bookWriteRef, &writeCount, (Ptr)&doc->bookBuildPos);
        if (err != noErr) {
            break;
        }

        doc->bookBuildPage++;
        if (doc->bookBuildPos >= contentLen) {
            return FinishBookBuild(w, doc);
        }

        next = ReaderAdvancePage(doc, doc->bookBuildPos);
        if (next <= doc->bookBuildPos) {
            return FinishBookBuild(w, doc);
        }
        doc->bookBuildPos = next;
        if (doc->bookBuildPos >= contentLen) {
            return FinishBookBuild(w, doc);
        }
    }

    SetBuildProgress(w, doc, doc->bookBuildPage);
    return err;
}

void BookIndexCancelBuild(ReaderDoc* doc) {
    if (!doc) {
        return;
    }
    if (doc->bookWriteRef > 0) {
        FSClose(doc->bookWriteRef);
        doc->bookWriteRef = 0;
    }
    doc->bookBuilding = false;
    if (doc->bookBuildDlg) {
        EndBuildDialog(NULL, doc);
    }
}

void BookIndexClose(ReaderDoc* doc) {
    if (!doc) {
        return;
    }
    BookIndexCancelBuild(doc);
    if (doc->bookIndexRef > 0) {
        FSClose(doc->bookIndexRef);
        doc->bookIndexRef = 0;
    }
    doc->bookPageCount = 0;
}

Boolean BookIndexIsBuilding(ReaderDoc* doc) {
    return doc && doc->bookBuilding;
}

Boolean BookIndexIsOpen(ReaderDoc* doc) {
    return doc && doc->bookIndexRef > 0 && doc->bookPageCount > 0 && !doc->bookBuilding;
}

OSErr BookIndexPageOffset(ReaderDoc* doc, short pageNum, long* outOffset) {
    long count = 4;
    long filePos;
    OSErr err;

    if (!BookIndexIsOpen(doc) || !outOffset) {
        return paramErr;
    }
    if (pageNum < 1 || pageNum > doc->bookPageCount) {
        return paramErr;
    }

    filePos = kBookHeaderSize + ((long)pageNum - 1L) * 4L;
    err = SetFPos(doc->bookIndexRef, fsFromStart, filePos);
    if (err != noErr) {
        return err;
    }
    err = FSRead(doc->bookIndexRef, &count, (Ptr)outOffset);
    if (err != noErr || count != 4) {
        return err != noErr ? err : eofErr;
    }
    return noErr;
}

OSErr BookIndexPrepare(WindowRef w, ReaderDoc* doc, const SFReply* reply) {
    Str255 bookName;
    long sourceLen;
    long modDate;

    if (!doc || !reply || doc->fileRef <= 0) {
        return noErr;
    }

    BookIndexClose(doc);

    sourceLen = doc->fileLen;
    modDate = 0;
    BookFileName(reply->fName, bookName);

    if (TryOpenExistingIndex(doc, bookName, reply->vRefNum, sourceLen, modDate) == noErr) {
        return noErr;
    }

    return StartBookBuild(w, doc, bookName, reply->vRefNum, sourceLen, modDate);
}

void BookIndexIdle(WindowRef w, ReaderDoc* doc) {
    OSErr err;
    Boolean wasBuilding;

    if (!doc || !doc->bookBuilding) {
        return;
    }

    wasBuilding = doc->bookBuilding;
    ServiceBuildDialog(doc);
    err = BuildStep(w, doc);
    if (err != noErr) {
        BookIndexCancelBuild(doc);
    } else if (wasBuilding && !doc->bookBuilding && w) {
        InvalRect(&w->portRect);
    }
}
