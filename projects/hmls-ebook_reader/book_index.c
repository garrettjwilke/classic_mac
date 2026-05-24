#include "book_index.h"
#include "reader_doc.h"

#include <Dialogs.h>
#include <Events.h>
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
    kBookVersion = 3,
    kBookHeaderSize = 24,
    kBookDialogID = 129,
    kBookProgressItem = 2,
    kPagesPerIdle = 24,
    kBuildUIRedrawInterval = 20,
    kBookDlgStorageSize = 1600
};

extern void ReaderOnIndexReady(WindowRef w, ReaderDoc* doc);

static char sBookDlgStorage[kBookDlgStorageSize];

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

static Boolean PascalSuffixMatches(ConstStr255Param name, const char* suffix) {
    short nlen = name[0];
    short slen = (short)strlen(suffix);
    short i;

    if (nlen < slen) {
        return false;
    }

    for (i = 0; i < slen; i++) {
        char c = name[1 + nlen - slen + i];
        char s = suffix[i];

        if (c >= 'A' && c <= 'Z') {
            c = (char)(c + ('a' - 'A'));
        }
        if (s >= 'A' && s <= 'Z') {
            s = (char)(s + ('a' - 'A'));
        }
        if (c != s) {
            return false;
        }
    }
    return true;
}

Boolean BookIndexNameIsText(ConstStr255Param name) {
    return PascalSuffixMatches(name, ".txt");
}

Boolean BookIndexNameIsBook(ConstStr255Param name) {
    return PascalSuffixMatches(name, ".pgdata") || PascalSuffixMatches(name, ".book");
}

Boolean BookIndexNameIsAllowed(ConstStr255Param name) {
    return BookIndexNameIsText(name) || BookIndexNameIsBook(name);
}

void BookIndexAppendTxtExtension(ConstStr255Param baseName, Str255 txtName) {
    short len = baseName[0];

    if (len > 251) {
        len = 251;
    }
    txtName[0] = (unsigned char)len;
    memcpy(txtName + 1, baseName + 1, len);
    if (!BookIndexNameIsText(txtName) && len + 4 <= 255) {
        txtName[++len] = '.';
        txtName[++len] = 't';
        txtName[++len] = 'x';
        txtName[++len] = 't';
        txtName[0] = (unsigned char)len;
    }
}

void BookIndexTextNameFromBook(ConstStr255Param bookName, Str255 txtName) {
    short len = bookName[0];
    short dot = 0;
    short i;

    if (len > 250) {
        len = 250;
    }
    txtName[0] = (unsigned char)len;
    memcpy(txtName + 1, bookName + 1, len);

    for (i = 1; i <= len; i++) {
        if (txtName[i] == '.') {
            dot = i;
        }
    }

    if (dot > 0 && dot + 6 <= len && txtName[dot + 1] == 'p' && txtName[dot + 2] == 'g'
        && txtName[dot + 3] == 'd' && txtName[dot + 4] == 'a' && txtName[dot + 5] == 't'
        && txtName[dot + 6] == 'a') {
        txtName[0] = (unsigned char)(dot + 3);
        txtName[dot + 1] = 't';
        txtName[dot + 2] = 'x';
        txtName[dot + 3] = 't';
    } else if (dot > 0 && dot + 3 <= len && txtName[dot + 1] == 'b' && txtName[dot + 2] == 'o'
        && txtName[dot + 3] == 'k') {
        txtName[dot + 1] = 't';
        txtName[dot + 2] = 'x';
        txtName[dot + 3] = 't';
    } else {
        BookIndexAppendTxtExtension(bookName, txtName);
    }
}

Boolean BookIndexSFReplyIsBook(const SFReply* reply) {
    if (!reply) {
        return false;
    }
    if (reply->fType == (OSType)0x50474454) { /* 'PGDT' */
        return true;
    }
    if (reply->fType == (OSType)0x424F4F4B) { /* 'BOOK' legacy */
        return true;
    }
    return BookIndexNameIsBook(reply->fName);
}

static Boolean PascalNameHasExtension(ConstStr255Param name) {
    short len = name[0];
    short i;

    for (i = len; i >= 1; i--) {
        if (name[i] == '.') {
            return true;
        }
    }
    return false;
}

Boolean BookIndexResolveTextOpen(const SFReply* reply, Str255 textName) {
    if (!reply || !reply->good) {
        return false;
    }

    if (BookIndexSFReplyIsBook(reply)) {
        BookIndexTextNameFromBook(reply->fName, textName);
        return textName[0] > 0;
    }

    if (BookIndexNameIsBook(reply->fName)) {
        BookIndexTextNameFromBook(reply->fName, textName);
        return textName[0] > 0;
    }

    if (BookIndexNameIsText(reply->fName)) {
        memcpy(textName, reply->fName, reply->fName[0] + 1);
        return true;
    }

    /* Mac text files are often plain names (no .txt) with type TEXT or unknown. */
    if (!PascalNameHasExtension(reply->fName)) {
        memcpy(textName, reply->fName, reply->fName[0] + 1);
        return textName[0] > 0;
    }

    return false;
}

void BookIndexCopyToSFName(ConstStr255Param src, Str63 dst) {
    short len = src[0];

    if (len > 63) {
        len = 63;
    }
    dst[0] = (unsigned char)len;
    memcpy(dst + 1, src + 1, len);
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

    if (dot > 0 && dot + 6 <= 255) {
        bookName[dot + 1] = 'p';
        bookName[dot + 2] = 'g';
        bookName[dot + 3] = 'd';
        bookName[dot + 4] = 'a';
        bookName[dot + 5] = 't';
        bookName[dot + 6] = 'a';
        bookName[0] = (unsigned char)(dot + 6);
    } else if (len + 7 <= 255) {
        bookName[++len] = '.';
        bookName[++len] = 'p';
        bookName[++len] = 'g';
        bookName[++len] = 'd';
        bookName[++len] = 'a';
        bookName[++len] = 't';
        bookName[++len] = 'a';
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

static void YieldDuringBuild(ReaderDoc* doc) {
    EventRecord e;
    DialogPtr dlg = doc->bookBuildDlg;
    WindowPtr dlgWin;
    GrafPtr oldPort;

    SystemTask();
    if (!dlg) {
        return;
    }

    dlgWin = (WindowPtr)dlg;
    oldPort = qd.thePort;
    SetPort(dlg);
    DrawDialog(dlg);
    SetPort(oldPort);

    while (EventAvail(updateMask, &e)) {
        (void)GetNextEvent(updateMask, &e);
        if (e.what == updateEvt) {
            WindowPtr w = (WindowPtr)e.message;
            BeginUpdate(w);
            if (w == dlgWin) {
                SetPort(dlg);
                DrawDialog(dlg);
                SetPort(oldPort);
            }
            EndUpdate(w);
        }
    }
}

static DialogPtr BeginBuildDialog(ReaderDoc* doc) {
    DialogPtr dlg;
    DialogItemType type;
    Handle item;
    Rect box;
    GrafPtr oldPort;
    WindowPtr dlgWin;

    dlg = GetNewDialog(kBookDialogID, sBookDlgStorage, (WindowPtr)-1);
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
    YieldDuringBuild(doc);
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
    Boolean refreshMain = (page <= 0 || (page % kBuildUIRedrawInterval) == 0);

    if (page <= 0) {
        msg[0] = 10;
        memcpy(msg + 1, "Starting...", 10);
    } else {
        PageNumberToStr255(page, msg);
    }

    if (doc->bookProgressItem) {
        SetDialogItemText(doc->bookProgressItem, msg);
    }

    if (refreshMain && w && doc->bookBuilding) {
        Str255 title;
        short i;

        title[0] = doc->bookSavedTitle[0];
        for (i = 1; i <= title[0]; i++) {
            title[i] = doc->bookSavedTitle[i];
        }
        if (title[0] + msg[0] + 3 <= 255) {
            title[++title[0]] = ' ';
            title[++title[0]] = '-';
            title[++title[0]] = ' ';
            for (i = 1; i <= msg[0]; i++) {
                title[++title[0]] = msg[i];
            }
        }
        SetWTitle(w, title);
    }

    YieldDuringBuild(doc);

    if (refreshMain && w && (doc->bookBuilding || doc->bookAwaitingDisplay)) {
        ReaderOnBuildProgress(w, doc);
    }

    if (refreshMain && doc->bookTitleProgress && w) {
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
    YieldDuringBuild(doc);
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

    err = Create(bookName, vRefNum, 'RDR ', 'PGDT');
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
    ReaderOnIndexReady(w, doc);
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
        SetBuildProgress(w, doc, doc->bookBuildPage);

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

Boolean BookIndexBlocksUI(ReaderDoc* doc) {
    if (!doc) {
        return false;
    }
    return doc->bookBuilding || doc->bookIndexPending || doc->bookAwaitingDisplay;
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

static OSErr DeleteSidecarFile(ConstStr255Param name, short vRefNum) {
    OSErr err;
    WDPBRec wd;

    err = FSDelete(name, vRefNum);
    if (err == noErr || err == fnfErr) {
        return noErr;
    }

    memset(&wd, 0, sizeof(wd));
    wd.ioNamePtr = NULL;
    wd.ioWDIndex = 0;
    wd.ioVRefNum = vRefNum;
    if (PBGetWDInfoSync(&wd) == noErr) {
        err = HDelete(wd.ioWDVRefNum, wd.ioWDDirID, name);
        if (err == noErr || err == fnfErr) {
            return noErr;
        }
        err = HDelete(vRefNum, wd.ioWDDirID, name);
        if (err == noErr || err == fnfErr) {
            return noErr;
        }
    }

    return err;
}

OSErr BookIndexRegenerate(WindowRef w, ReaderDoc* doc) {
    Str255 bookName;
    OSErr err;

    if (!doc || !w || !doc->hasFile || doc->fileRef <= 0) {
        return paramErr;
    }
    if (BookIndexIsBuilding(doc)) {
        return paramErr;
    }

    BookIndexClose(doc);
    doc->totalPages = 0;

    BookFileName(doc->bookSourceName, bookName);
    (void)DeleteSidecarFile(bookName, doc->bookSourceVRefNum);

    doc->bookAwaitingDisplay = true;
    err = StartBookBuild(w, doc, bookName, doc->bookSourceVRefNum, doc->fileLen, 0);
    if (err != noErr) {
        doc->bookAwaitingDisplay = false;
        ReaderOnIndexReady(w, doc);
    }
    return err;
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
        doc->bookAwaitingDisplay = false;
        ReaderOnIndexReady(w, doc);
        return noErr;
    }

    doc->bookAwaitingDisplay = true;
    if (StartBookBuild(w, doc, bookName, reply->vRefNum, sourceLen, modDate) != noErr) {
        doc->bookAwaitingDisplay = false;
        ReaderOnIndexReady(w, doc);
        return noErr;
    }
    return noErr;
}

void BookIndexIdle(WindowRef w, ReaderDoc* doc) {
    OSErr err;
    Boolean wasBuilding;
    short burst;

    if (!doc || !doc->bookBuilding) {
        return;
    }

    wasBuilding = doc->bookBuilding;
    for (burst = 0; burst < 4 && doc->bookBuilding; burst++) {
        err = BuildStep(w, doc);
        if (err != noErr) {
            BookIndexCancelBuild(doc);
            ReaderOnIndexReady(w, doc);
            return;
        }
    }
    ServiceBuildDialog(doc);
    if (wasBuilding && !doc->bookBuilding && w) {
        InvalRect(&w->portRect);
    }
}
