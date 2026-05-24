#include "reader_state.h"
#include "reader_doc.h"

#include <Files.h>
#include <Menus.h>
#include <TextUtils.h>

#include <string.h>

enum {
    kStateMagic = 0x52454144, /* 'READ' */
    kStateVersion = 1,
    kStateHeaderSize = 20
};

#ifndef dupErr
#define dupErr (-48)
#endif

typedef struct StateHeader {
    long magic;
    short version;
    short linesPerPage;
    short lineHeight;
    short maxPixelWidth;
    long sourceLen;
    short lastPage;
    short bookmarkCount;
} StateHeader;

static void StateFileName(ConstStr255Param txtName, Str255 stateName) {
    short len = txtName[0];
    short dot = 0;
    short i;

    if (len > 250) {
        len = 250;
    }
    stateName[0] = (unsigned char)len;
    memcpy(stateName + 1, txtName + 1, len);

    for (i = 1; i <= len; i++) {
        if (stateName[i] == '.') {
            dot = i;
        }
    }

    if (dot > 0 && dot + 4 <= 255) {
        stateName[dot + 1] = 'r';
        stateName[dot + 2] = 'e';
        stateName[dot + 3] = 'a';
        stateName[dot + 4] = 'd';
        stateName[0] = (unsigned char)(dot + 4);
    } else if (len + 5 <= 255) {
        stateName[++len] = '.';
        stateName[++len] = 'r';
        stateName[++len] = 'e';
        stateName[++len] = 'a';
        stateName[++len] = 'd';
        stateName[0] = (unsigned char)len;
    }
}

static OSErr OpenStateFile(ConstStr255Param name, short vRefNum, signed char permission, short* refNum) {
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

static Boolean HeaderMatchesDoc(const StateHeader* hdr, const ReaderDoc* doc) {
    if (hdr->magic != kStateMagic || hdr->version != kStateVersion) {
        return false;
    }
    if (hdr->sourceLen != doc->fileLen) {
        return false;
    }
    if (hdr->linesPerPage != doc->linesPerPage || hdr->lineHeight != doc->lineHeight
        || hdr->maxPixelWidth != doc->maxPixelWidth) {
        return false;
    }
    return true;
}

static void ResetState(ReaderDoc* doc) {
    doc->savedLastPage = 1;
    doc->bookmarkCount = 0;
}

static void SortBookmarks(ReaderDoc* doc) {
    short i;
    short j;

    for (i = 0; i < doc->bookmarkCount - 1; i++) {
        for (j = (short)(i + 1); j < doc->bookmarkCount; j++) {
            if (doc->bookmarkPages[j] < doc->bookmarkPages[i]) {
                short tmp = doc->bookmarkPages[i];
                doc->bookmarkPages[i] = doc->bookmarkPages[j];
                doc->bookmarkPages[j] = tmp;
            }
        }
    }
}

void ReaderStateLoad(ReaderDoc* doc) {
    Str255 stateName;
    short refNum = 0;
    StateHeader hdr;
    long count;
    OSErr err;

    ResetState(doc);
    if (!doc || !doc->hasFile || doc->bookSourceName[0] == 0) {
        return;
    }

    StateFileName(doc->bookSourceName, stateName);
    err = OpenStateFile(stateName, doc->bookSourceVRefNum, fsRdPerm, &refNum);
    if (err != noErr) {
        return;
    }

    count = kStateHeaderSize;
    err = FSRead(refNum, &count, (Ptr)&hdr);
    if (err != noErr || count != kStateHeaderSize || !HeaderMatchesDoc(&hdr, doc)) {
        FSClose(refNum);
        return;
    }

    doc->savedLastPage = hdr.lastPage;
    if (doc->savedLastPage < 1) {
        doc->savedLastPage = 1;
    }

    doc->bookmarkCount = hdr.bookmarkCount;
    if (doc->bookmarkCount < 0) {
        doc->bookmarkCount = 0;
    }
    if (doc->bookmarkCount > kMaxBookmarks) {
        doc->bookmarkCount = kMaxBookmarks;
    }

    if (doc->bookmarkCount > 0) {
        count = (long)doc->bookmarkCount * (long)sizeof(short);
        err = FSRead(refNum, &count, (Ptr)doc->bookmarkPages);
        if (err != noErr || count != (long)doc->bookmarkCount * (long)sizeof(short)) {
            doc->bookmarkCount = 0;
        }
    }

    FSClose(refNum);
    SortBookmarks(doc);
}

void ReaderStateSave(ReaderDoc* doc) {
    Str255 stateName;
    short refNum = 0;
    StateHeader hdr;
    long count;
    OSErr err;

    if (!doc || !doc->hasFile || doc->bookSourceName[0] == 0) {
        return;
    }

    StateFileName(doc->bookSourceName, stateName);
    err = Create(stateName, doc->bookSourceVRefNum, 'RDR ', 'READ');
    if (err != noErr && err != dupErr) {
        return;
    }

    err = OpenStateFile(stateName, doc->bookSourceVRefNum, fsWrPerm, &refNum);
    if (err != noErr) {
        return;
    }

    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = kStateMagic;
    hdr.version = kStateVersion;
    hdr.linesPerPage = doc->linesPerPage;
    hdr.lineHeight = doc->lineHeight;
    hdr.maxPixelWidth = doc->maxPixelWidth;
    hdr.sourceLen = doc->fileLen;
    hdr.lastPage = doc->currentPage;
    hdr.bookmarkCount = doc->bookmarkCount;

    count = kStateHeaderSize;
    err = FSWrite(refNum, &count, (Ptr)&hdr);
    if (err != noErr) {
        FSClose(refNum);
        return;
    }

    if (doc->bookmarkCount > 0) {
        count = (long)doc->bookmarkCount * (long)sizeof(short);
        err = FSWrite(refNum, &count, (Ptr)doc->bookmarkPages);
    }

    FSClose(refNum);
}

Boolean ReaderStateHasBookmark(const ReaderDoc* doc, short page) {
    short i;

    if (!doc) {
        return false;
    }

    for (i = 0; i < doc->bookmarkCount; i++) {
        if (doc->bookmarkPages[i] == page) {
            return true;
        }
    }
    return false;
}

OSErr ReaderStateAddBookmark(ReaderDoc* doc, short page) {
    if (!doc || page < 1) {
        return paramErr;
    }
    if (ReaderStateHasBookmark(doc, page)) {
        return noErr;
    }
    if (doc->bookmarkCount >= kMaxBookmarks) {
        return memFullErr;
    }

    doc->bookmarkPages[doc->bookmarkCount] = page;
    doc->bookmarkCount++;
    SortBookmarks(doc);
    ReaderStateSave(doc);
    RebuildBookmarkMenu(doc);
    return noErr;
}

OSErr ReaderStateDeleteBookmark(ReaderDoc* doc, short page) {
    short i;

    if (!doc) {
        return paramErr;
    }

    for (i = 0; i < doc->bookmarkCount; i++) {
        if (doc->bookmarkPages[i] == page) {
            short j;
            for (j = i; j < doc->bookmarkCount - 1; j++) {
                doc->bookmarkPages[j] = doc->bookmarkPages[j + 1];
            }
            doc->bookmarkCount--;
            ReaderStateSave(doc);
            RebuildBookmarkMenu(doc);
            return noErr;
        }
    }

    return fnfErr;
}

void RebuildBookmarkMenu(const ReaderDoc* doc) {
    MenuRef menu = GetMenu(kMenuBookmarks);
    short count;
    short i;

    if (!menu) {
        return;
    }

    count = CountMItems(menu);
    while (count >= kItemBookmarkFirst) {
        DeleteMenuItem(menu, count);
        count--;
    }

    if (!doc || !doc->hasFile) {
        return;
    }

    for (i = 0; i < doc->bookmarkCount; i++) {
        Str255 label;
        Str255 numStr;
        short len = 0;
        short j;
        const char prefix[] = "Page ";

        NumToString(doc->bookmarkPages[i], numStr);
        for (j = 0; prefix[j] != '\0'; j++) {
            label[++len] = prefix[j];
        }
        for (j = 1; j <= numStr[0]; j++) {
            label[++len] = numStr[j];
        }
        label[0] = (unsigned char)len;
        AppendMenu(menu, label);
    }
}
