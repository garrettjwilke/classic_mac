#include "book_index.h"
#include "reader_doc.h"

#include "book_format.h"
#include "paginate.h"

#include <Dialogs.h>
#include <Events.h>
#include <Files.h>
#include <Memory.h>
#include <OSUtils.h>
#include <Quickdraw.h>
#include <TextUtils.h>
#include <Windows.h>

#include <string.h>

#ifndef dupErr
#define dupErr (-48)
#endif

enum {
    kBookDialogID = 129,
    kBookProgressItem = 2,
    kPagesPerIdle = 128,
    kBuildIdleBursts = 8,
    kBuildUIRedrawInterval = 48,
    kBookDlgStorageSize = 1600
};

static Handle sBuildSource;
static PaginateCtx sBuildPaginate;
static Boolean sBuildUseMemory;
static long sBuildOffsetBatch[64];
static short sBuildOffsetBatchLen;
static long sBuildPageOffsetBase;
static long sBuildPageOffsets[2048];
static long sBuildPageOffsetCount;

extern void ReaderOnIndexReady(WindowRef w, ReaderDoc* doc);

void BookIndexLoadChapters(ReaderDoc* doc);

static char sBookDlgStorage[kBookDlgStorageSize];

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

Boolean BookIndexNameIsDotBook(ConstStr255Param name) {
    return PascalSuffixMatches(name, ".book");
}

Boolean BookIndexNameIsBook(ConstStr255Param name) {
    return PascalSuffixMatches(name, ".pgdata");
}

Boolean BookIndexNameIsEpub(ConstStr255Param name) {
    short len = name[0];
    if (len < 5) {
        return false;
    }
    return name[len - 3] == 'e' && name[len - 2] == 'p' && name[len - 1] == 'u'
        && name[len] == 'b';
}

Boolean BookIndexNameIsAllowed(ConstStr255Param name) {
    return BookIndexNameIsDotBook(name) || BookIndexNameIsText(name) || BookIndexNameIsBook(name)
        || BookIndexNameIsEpub(name);
}

void BookIndexAppendTxtExtension(ConstStr255Param baseName, Str255 txtName) {
    short len = baseName[0];

    if (len > 251) {
        len = 251;
    }
    txtName[0] = (unsigned char)len;
    memcpy(txtName + 1, baseName + 1, len);
    if (!BookIndexNameIsText(txtName) && !BookIndexNameIsDotBook(txtName) && len + 5 <= 255) {
        txtName[++len] = '.';
        txtName[++len] = 'b';
        txtName[++len] = 'o';
        txtName[++len] = 'o';
        txtName[++len] = 'k';
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
        txtName[dot + 1] = 'b';
        txtName[dot + 2] = 'o';
        txtName[dot + 3] = 'o';
        txtName[dot + 4] = 'k';
        txtName[0] = (unsigned char)(dot + 4);
    } else if (dot > 0 && dot + 4 <= len && txtName[dot + 1] == 'b' && txtName[dot + 2] == 'o'
        && txtName[dot + 3] == 'o' && txtName[dot + 4] == 'k') {
        return;
    } else if (dot > 0 && dot + 3 <= len && txtName[dot + 1] == 't' && txtName[dot + 2] == 'x'
        && txtName[dot + 3] == 't') {
        txtName[dot + 1] = 'b';
        txtName[dot + 2] = 'o';
        txtName[dot + 3] = 'o';
        txtName[dot + 4] = 'k';
        txtName[0] = (unsigned char)(dot + 4);
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

    if (BookIndexNameIsDotBook(reply->fName)) {
        memcpy(textName, reply->fName, reply->fName[0] + 1);
        return true;
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

    if (dot > 0 && dot + 6 <= len && bookName[dot + 1] == 'p' && bookName[dot + 2] == 'g'
        && bookName[dot + 3] == 'd' && bookName[dot + 4] == 'a' && bookName[dot + 5] == 't'
        && bookName[dot + 6] == 'a') {
        return;
    }
    if (dot > 0 && dot + 4 <= len && bookName[dot + 1] == 'b' && bookName[dot + 2] == 'o'
        && bookName[dot + 3] == 'o' && bookName[dot + 4] == 'k') {
        bookName[dot + 1] = 'p';
        bookName[dot + 2] = 'g';
        bookName[dot + 3] = 'd';
        bookName[dot + 4] = 'a';
        bookName[dot + 5] = 't';
        bookName[dot + 6] = 'a';
        bookName[0] = (unsigned char)(dot + 6);
        return;
    }
    if (dot > 0 && dot + 3 <= len && bookName[dot + 1] == 't' && bookName[dot + 2] == 'x'
        && bookName[dot + 3] == 't') {
        bookName[dot + 1] = 'p';
        bookName[dot + 2] = 'g';
        bookName[dot + 3] = 'd';
        bookName[dot + 4] = 'a';
        bookName[dot + 5] = 't';
        bookName[dot + 6] = 'a';
        bookName[0] = (unsigned char)(dot + 6);
        return;
    }
    if (len + 7 <= 255) {
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

static OSErr ReadIndexChapterCount(short refNum, const BookHeader* hdr, long* chapterCount) {
    long count = 4;
    int32_t raw;
    OSErr err;

    *chapterCount = 0;
    if (hdr->version < kBookVersion) {
        return noErr;
    }

    err = SetFPos(refNum, fsFromStart, kBookHeaderSize);
    if (err != noErr) {
        return err;
    }
    err = FSRead(refNum, &count, (Ptr)&raw);
    if (err != noErr || count != 4) {
        return err != noErr ? err : eofErr;
    }
    *chapterCount = (long)be32(raw);
    if (*chapterCount < 0 || *chapterCount > kMaxChapters) {
        *chapterCount = 0;
    }
    return noErr;
}

static long IndexTitleBytesSize(short refNum, const BookHeader* hdr, long chapterCount) {
    long i;
    long total = 0;
    long pos;
    BookChapter ch;
    long count;

    if (chapterCount <= 0) {
        return 0;
    }

    pos = kBookHeaderSize + 4 + chapterCount * kBookChapterEntrySize;
    for (i = 0; i < chapterCount; i++) {
        count = kBookChapterEntrySize;
        if (SetFPos(refNum, fsFromStart, pos) != noErr) {
            return 0;
        }
        if (FSRead(refNum, &count, (Ptr)&ch) != noErr || count != kBookChapterEntrySize) {
            return 0;
        }
        ch.titleLen = be16(ch.titleLen);
        total += ch.titleLen;
        pos += kBookChapterEntrySize;
    }
    return total;
}

static long IndexPageOffsetBase(short refNum, const BookHeader* hdr) {
    long chapterCount = 0;

    if (hdr->version < kBookVersion) {
        return kBookHeaderSize;
    }
    if (ReadIndexChapterCount(refNum, hdr, &chapterCount) != noErr) {
        return kBookHeaderSize;
    }
    return kBookHeaderSize + 4 + chapterCount * kBookChapterEntrySize
        + IndexTitleBytesSize(refNum, hdr, chapterCount);
}

static long IndexPageCountFromFile(short refNum, const BookHeader* hdr) {
    long eof;
    long fromFile;
    long base;

    if (hdr->pageCount > 0) {
        return hdr->pageCount;
    }
    if (GetEOF(refNum, &eof) != noErr || eof <= kBookHeaderSize) {
        return 0;
    }
    base = IndexPageOffsetBase(refNum, hdr);
    fromFile = (eof - base) / 4;
    return fromFile > 0 ? fromFile : 0;
}

static Boolean HeaderMatchesBook(short refNum, const BookHeader* hdr, ReaderDoc* doc, long sourceLen,
    long modDate) {
    if (hdr->magic != kBookMagic
        || (hdr->version != kBookVersion && hdr->version != kBookVersionLegacy)) {
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
    BookIndexLoadChapters(doc);
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

static void ReleaseBuildSource(void) {
    if (sBuildSource) {
        HUnlock(sBuildSource);
        DisposeHandle(sBuildSource);
        sBuildSource = NULL;
    }
    sBuildUseMemory = false;
    sBuildOffsetBatchLen = 0;
    memset(&sBuildPaginate, 0, sizeof(sBuildPaginate));
}

static OSErr FlushBuildOffsets(ReaderDoc* doc) {
    long writeCount;
    OSErr err;

    if (sBuildOffsetBatchLen <= 0) {
        return noErr;
    }
    writeCount = (long)sBuildOffsetBatchLen * 4L;
    err = FSWrite(doc->bookWriteRef, &writeCount, (Ptr)sBuildOffsetBatch);
    sBuildOffsetBatchLen = 0;
    return err;
}

static OSErr QueueBuildOffset(ReaderDoc* doc, long offset) {
    if (sBuildPageOffsetCount < (long)(sizeof(sBuildPageOffsets) / sizeof(sBuildPageOffsets[0]))) {
        sBuildPageOffsets[sBuildPageOffsetCount++] = offset;
    }
    sBuildOffsetBatch[sBuildOffsetBatchLen++] = offset;
    if (sBuildOffsetBatchLen >= (short)(sizeof(sBuildOffsetBatch) / sizeof(sBuildOffsetBatch[0]))) {
        return FlushBuildOffsets(doc);
    }
    return noErr;
}

static OSErr WriteChapterSection(ReaderDoc* doc) {
    int32_t countBe;
    long count = 4;
    short i;
    OSErr err;

    if (!doc || doc->chapterCount <= 0) {
        sBuildPageOffsetBase = kBookHeaderSize;
        return noErr;
    }

    countBe = be32((int32_t)doc->chapterCount);
    err = FSWrite(doc->bookWriteRef, &count, (Ptr)&countBe);
    if (err != noErr) {
        return err;
    }

    for (i = 0; i < doc->chapterCount; i++) {
        BookChapter ch;
        BookChapter chBe;
        long entryCount = kBookChapterEntrySize;

        ch.sourceOffset = (int32_t)doc->chapters[i].sourceOffset;
        ch.firstPage = 0;
        ch.titleLen = doc->chapters[i].titleLen;
        book_chapter_to_be(&ch, &chBe);
        err = FSWrite(doc->bookWriteRef, &entryCount, (Ptr)&chBe);
        if (err != noErr) {
            return err;
        }
    }

    for (i = 0; i < doc->chapterCount; i++) {
        long titleCount = doc->chapters[i].titleLen;
        if (titleCount > 0) {
            err = FSWrite(doc->bookWriteRef, &titleCount, doc->chapters[i].title);
            if (err != noErr) {
                return err;
            }
        }
    }

    if (GetFPos(doc->bookWriteRef, &sBuildPageOffsetBase) != noErr) {
        sBuildPageOffsetBase = kBookHeaderSize;
    }
    return noErr;
}

static OSErr RewriteChapterFirstPages(ReaderDoc* doc) {
    short i;
    long page;
    OSErr err;

    if (!doc || doc->chapterCount <= 0 || doc->bookWriteRef <= 0) {
        return noErr;
    }

    for (i = 0; i < doc->chapterCount; i++) {
        doc->chapters[i].firstPage = 0;
        for (page = 0; page < sBuildPageOffsetCount; page++) {
            if (sBuildPageOffsets[page] >= doc->chapters[i].sourceOffset) {
                doc->chapters[i].firstPage = (short)(page + 1);
                break;
            }
        }
    }

    err = SetFPos(doc->bookWriteRef, fsFromStart, kBookHeaderSize + 4);
    if (err != noErr) {
        return err;
    }

    for (i = 0; i < doc->chapterCount; i++) {
        BookChapter ch;
        BookChapter chBe;
        long entryCount = kBookChapterEntrySize;

        ch.sourceOffset = (int32_t)doc->chapters[i].sourceOffset;
        ch.firstPage = (int32_t)doc->chapters[i].firstPage;
        ch.titleLen = doc->chapters[i].titleLen;
        book_chapter_to_be(&ch, &chBe);
        err = FSWrite(doc->bookWriteRef, &entryCount, (Ptr)&chBe);
        if (err != noErr) {
            return err;
        }
    }
    return noErr;
}

static OSErr PrepareBuildSource(ReaderDoc* doc) {
    Handle source;
    long count;
    OSErr err;
    BookLayout layout;

    ReleaseBuildSource();
    if (doc->fileRef <= 0 || doc->bookBuildSourceLen <= 0) {
        return paramErr;
    }

    source = NewHandle(doc->bookBuildSourceLen);
    if (!source) {
        return memFullErr;
    }

    err = SetFPos(doc->fileRef, fsFromStart, 0);
    if (err != noErr) {
        DisposeHandle(source);
        return err;
    }

    HLock(source);
    count = doc->bookBuildSourceLen;
    err = FSRead(doc->fileRef, &count, *source);
    if (err != noErr || count != doc->bookBuildSourceLen) {
        HUnlock(source);
        DisposeHandle(source);
        return err != noErr ? err : eofErr;
    }

    layout.linesPerPage = doc->linesPerPage;
    layout.lineHeight = doc->lineHeight;
    layout.maxPixelWidth = doc->maxPixelWidth;
    paginate_init(&sBuildPaginate, (const unsigned char*)*source, doc->bookBuildSourceLen, &layout);

    sBuildSource = source;
    sBuildUseMemory = true;
    return noErr;
}

static long AdvanceBuildPage(ReaderDoc* doc, long offset) {
    if (sBuildUseMemory && sBuildSource) {
        return paginate_advance_page(&sBuildPaginate, offset);
    }
    return ReaderAdvancePage(doc, offset);
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
    sBuildOffsetBatchLen = 0;
    sBuildPageOffsetCount = 0;
    sBuildPageOffsetBase = kBookHeaderSize;

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

    err = WriteChapterSection(doc);
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

    err = PrepareBuildSource(doc);
    if (err == memFullErr) {
        /* Low memory: fall back to streaming pagination (slower, same offsets). */
        ReleaseBuildSource();
        err = noErr;
    } else if (err != noErr) {
        ReleaseBuildSource();
        FSClose(doc->bookWriteRef);
        doc->bookWriteRef = 0;
        doc->bookBuilding = false;
        EndBuildDialog(w, doc);
        return err;
    }
    return noErr;
}

static OSErr FinishBookBuild(WindowRef w, ReaderDoc* doc) {
    BookHeader hdr;
    OSErr err;
    short readRef;

    err = FlushBuildOffsets(doc);
    if (err != noErr) {
        ReleaseBuildSource();
        FSClose(doc->bookWriteRef);
        doc->bookWriteRef = 0;
        doc->bookBuilding = false;
        EndBuildDialog(w, doc);
        return err;
    }

    err = RewriteChapterFirstPages(doc);
    if (err != noErr) {
        ReleaseBuildSource();
        FSClose(doc->bookWriteRef);
        doc->bookWriteRef = 0;
        doc->bookBuilding = false;
        EndBuildDialog(w, doc);
        return err;
    }

    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = kBookMagic;
    hdr.version = (int16_t)(doc->chapterCount > 0 ? kBookVersion : kBookVersionLegacy);
    hdr.linesPerPage = doc->linesPerPage;
    hdr.lineHeight = doc->lineHeight;
    hdr.maxPixelWidth = doc->maxPixelWidth;
    hdr.sourceLen = doc->bookBuildSourceLen;
    hdr.sourceModDate = doc->bookBuildModDate;
    hdr.pageCount = doc->bookBuildPage;

    err = WriteHeader(doc->bookWriteRef, &hdr);
    ReleaseBuildSource();
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
    OSErr err;
    short batch;

    if (!doc->bookBuilding || doc->bookWriteRef <= 0) {
        return noErr;
    }

    contentLen = ReaderContentLength(doc);
    err = noErr;

    for (batch = 0; batch < (sBuildUseMemory ? kPagesPerIdle : 24); batch++) {
        long next;

        err = QueueBuildOffset(doc, doc->bookBuildPos);
        if (err != noErr) {
            break;
        }

        doc->bookBuildPage++;
        if (doc->bookBuildPage == 1
            || (doc->bookBuildPage % kBuildUIRedrawInterval) == 0) {
            SetBuildProgress(w, doc, doc->bookBuildPage);
        }

        if (doc->bookBuildPos >= contentLen) {
            SetBuildProgress(w, doc, doc->bookBuildPage);
            return FinishBookBuild(w, doc);
        }

        next = AdvanceBuildPage(doc, doc->bookBuildPos);
        if (next <= doc->bookBuildPos) {
            SetBuildProgress(w, doc, doc->bookBuildPage);
            return FinishBookBuild(w, doc);
        }
        doc->bookBuildPos = next;
        if (doc->bookBuildPos >= contentLen) {
            SetBuildProgress(w, doc, doc->bookBuildPage);
            return FinishBookBuild(w, doc);
        }
    }

    return err;
}

void BookIndexCancelBuild(ReaderDoc* doc) {
    if (!doc) {
        return;
    }
    ReleaseBuildSource();
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

    {
        BookHeader hdr;
        if (ReadHeader(doc->bookIndexRef, &hdr) != noErr) {
            return paramErr;
        }
        filePos = IndexPageOffsetBase(doc->bookIndexRef, &hdr) + ((long)pageNum - 1L) * 4L;
    }
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
    modDate = doc->epubSourceModDate;
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
    for (burst = 0; burst < kBuildIdleBursts && doc->bookBuilding; burst++) {
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

void BookIndexLoadChapters(ReaderDoc* doc) {
    BookHeader hdr;
    long chapterCount = 0;
    long i;
    long pos;
    long titlePos;
    short ch;

    if (!doc || doc->bookIndexRef <= 0) {
        return;
    }

    if (ReadHeader(doc->bookIndexRef, &hdr) != noErr) {
        return;
    }
    if (ReadIndexChapterCount(doc->bookIndexRef, &hdr, &chapterCount) != noErr || chapterCount <= 0) {
        return;
    }

    doc->chapterCount = (short)chapterCount;
    if (doc->chapterCount > kMaxChapters) {
        doc->chapterCount = kMaxChapters;
    }

    pos = kBookHeaderSize + 4;
    titlePos = kBookHeaderSize + 4 + doc->chapterCount * kBookChapterEntrySize;

    for (ch = 0; ch < doc->chapterCount; ch++) {
        BookChapter chdr;
        long count = kBookChapterEntrySize;
        long titleCount;

        if (SetFPos(doc->bookIndexRef, fsFromStart, pos) != noErr) {
            break;
        }
        if (FSRead(doc->bookIndexRef, &count, (Ptr)&chdr) != noErr) {
            break;
        }
        doc->chapters[ch].sourceOffset = be32(chdr.sourceOffset);
        doc->chapters[ch].firstPage = (short)be32(chdr.firstPage);
        doc->chapters[ch].titleLen = be16(chdr.titleLen);
        if (doc->chapters[ch].titleLen > kMaxChapterTitle - 1) {
            doc->chapters[ch].titleLen = kMaxChapterTitle - 1;
        }
        if (doc->chapters[ch].titleLen > 0) {
            titleCount = doc->chapters[ch].titleLen;
            if (SetFPos(doc->bookIndexRef, fsFromStart, titlePos) != noErr) {
                break;
            }
            if (FSRead(doc->bookIndexRef, &titleCount, doc->chapters[ch].title) != noErr) {
                break;
            }
            doc->chapters[ch].title[doc->chapters[ch].titleLen] = '\0';
            titlePos += doc->chapters[ch].titleLen;
        } else {
            doc->chapters[ch].title[0] = '\0';
        }
        pos += kBookChapterEntrySize;
    }

    for (i = 0; i < doc->chapterCount; i++) {
        if (doc->chapters[i].titleLen <= 0) {
            char fallback[24];
            short n = (short)(i + 1);
            fallback[0] = 'C';
            fallback[1] = 'h';
            fallback[2] = ' ';
            if (n >= 10) {
                fallback[3] = (char)('0' + (n / 10));
                fallback[4] = (char)('0' + (n % 10));
                fallback[5] = '\0';
                strcpy(doc->chapters[i].title, fallback);
                doc->chapters[i].titleLen = 6;
            } else {
                fallback[3] = (char)('0' + n);
                fallback[4] = '\0';
                strcpy(doc->chapters[i].title, fallback);
                doc->chapters[i].titleLen = 5;
            }
        }
    }
}

short BookIndexChapterCount(const ReaderDoc* doc) {
    return doc ? doc->chapterCount : 0;
}

OSErr BookIndexChapterFirstPage(ReaderDoc* doc, short chapterNum, short* outPage) {
    if (!doc || !outPage || chapterNum < 1 || chapterNum > doc->chapterCount) {
        return paramErr;
    }
    *outPage = doc->chapters[chapterNum - 1].firstPage;
    if (*outPage < 1) {
        *outPage = 1;
    }
    return noErr;
}
