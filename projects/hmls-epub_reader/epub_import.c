#include "epub_import.h"

#include "epub_util.h"
#include "mac_fs.h"
#include "reader_doc.h"
#include "xhtml_lite.h"
#include "zip_lite.h"

#include <Dialogs.h>
#include <Events.h>
#include <Files.h>
#include <Memory.h>
#include <OSUtils.h>
#include <StandardFile.h>
#include <string.h>
#include <stdlib.h>

#ifndef dupErr
#define dupErr (-48)
#endif

enum {
    kImportDialogID = 133,
    kImportProgressItem = 2
};

typedef struct {
    short bookRef;
    long writePos;
    ReaderDoc* doc;
    char content_dir[256];
    ZipArchive* zip;
    short chapterIdx;
} ImportCtx;

typedef struct {
    char* buf;
    size_t pos;
} FillBufCtx;

static void epub_copy_pascal(char* dst, ConstStr255Param src) {
    int len = src[0];
    memcpy(dst, &src[1], len);
    dst[len] = '\0';
}

static void cstr2pascal(const char* src, Str255 dst) {
    size_t len = strlen(src);
    if (len > 255) {
        len = 255;
    }
    dst[0] = (unsigned char)len;
    memcpy(dst + 1, src, len);
}

static OSErr OpenDataFile(ConstStr255Param name, short vRefNum, signed char permission, short* refNum) {
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

static OSErr WriteBytes(short refNum, const void* data, long len) {
    long count = len;
    if (len <= 0) {
        return noErr;
    }
    return FSWrite(refNum, &count, (Ptr)data);
}

static void fill_buf_callback(const char* data, size_t size, void* user_data) {
    FillBufCtx* ctx = (FillBufCtx*)user_data;
    memcpy(ctx->buf + ctx->pos, data, size);
    ctx->pos += size;
}

static char* zip_read_entry(ZipArchive* zip, const char* name) {
    ZipEntry entry;
    FillBufCtx ctx;

    if (!zip_find_entry(zip, name, &entry)) {
        return NULL;
    }
    if (entry.uncomp_size > 300000) {
        return NULL;
    }

    ctx.buf = (char*)malloc((size_t)entry.uncomp_size + 1);
    if (!ctx.buf) {
        return NULL;
    }
    ctx.pos = 0;
    if (!zip_stream_entry(zip, &entry, fill_buf_callback, &ctx)) {
        free(ctx.buf);
        return NULL;
    }
    ctx.buf[entry.uncomp_size] = '\0';
    return ctx.buf;
}

static int zip_try_paths(ZipArchive* zip, const char* base, const char* href, ZipEntry* entry) {
    char path[512];

    if (base[0]) {
        sprintf(path, "%s/%s", base, href);
        if (zip_find_entry(zip, path, entry)) {
            return 1;
        }
        mac_path_from_zip(path);
        if (zip_find_entry(zip, path, entry)) {
            return 1;
        }
    }
    if (zip_find_entry(zip, href, entry)) {
        return 1;
    }
    strcpy(path, href);
    mac_path_from_zip(path);
    return zip_find_entry(zip, path, entry);
}

typedef struct {
    ImportCtx* ctx;
    XHTML_Parser parser;
} StreamParser;

static void import_text_cb(const char* text, void* user_data) {
    ImportCtx* ctx = (ImportCtx*)user_data;
    long len;

    if (!text || !text[0] || ctx->bookRef <= 0) {
        return;
    }
    len = (long)strlen(text);
    if (WriteBytes(ctx->bookRef, text, len) == noErr) {
        ctx->writePos += len;
    }
}

static void zip_to_book_cb(const char* data, size_t size, void* user_data) {
    StreamParser* sp = (StreamParser*)user_data;
    xhtml_parser_process(&sp->parser, data, size);
}

static void chapter_title_from_href(const char* href, char* title, short maxLen) {
    const char* base = href;
    const char* slash;
    const char* dot;
    short len = 0;
    short i;

    slash = strrchr(href, '/');
    if (slash) {
        base = slash + 1;
    }
    dot = strrchr(base, '.');
    len = dot ? (short)(dot - base) : (short)strlen(base);
    if (len >= maxLen) {
        len = (short)(maxLen - 1);
    }
    for (i = 0; i < len; i++) {
        title[i] = base[i];
    }
    title[len] = '\0';
}

static void spine_import_cb(const char* href, void* user_data) {
    ImportCtx* ctx = (ImportCtx*)user_data;
    ReaderDoc* doc = ctx->doc;
    char* decoded;
    ZipEntry entry;
    StreamParser sp;
    char sep[4] = "\r\r";
    short ch;
    long startPos;
    long endPos;

    if (!href || doc->chapterCount >= kMaxChapters) {
        return;
    }

    decoded = epub_decode_url(href);
    if (!decoded) {
        decoded = (char*)href;
    }

    if (!zip_try_paths(ctx->zip, ctx->content_dir, decoded, &entry)) {
        if (decoded != href) {
            free(decoded);
        }
        return;
    }

    ch = doc->chapterCount;
    startPos = ctx->writePos;
    if (ch > 0) {
        (void)WriteBytes(ctx->bookRef, sep, 2);
        ctx->writePos += 2;
        startPos = ctx->writePos;
    }

    xhtml_parser_init(&sp.parser, import_text_cb, ctx);
    if (!zip_stream_entry(ctx->zip, &entry, zip_to_book_cb, &sp)) {
        ctx->writePos = startPos;
        if (decoded != href) {
            free(decoded);
        }
        return;
    }

    endPos = ctx->writePos;
    if (endPos <= startPos) {
        ctx->writePos = startPos;
        if (decoded != href) {
            free(decoded);
        }
        return;
    }

    doc->chapters[ch].sourceOffset = startPos;
    doc->chapters[ch].firstPage = 0;
    chapter_title_from_href(decoded, doc->chapters[ch].title, kMaxChapterTitle);
    doc->chapters[ch].titleLen = (short)strlen(doc->chapters[ch].title);
    doc->chapterCount++;

    if (decoded != href) {
        free(decoded);
    }
    ctx->chapterIdx++;
}

static char epub_tolower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

Boolean EpubNameIsEpub(ConstStr255Param name) {
    short len = name[0];
    if (len < 5) {
        return false;
    }
    return epub_tolower(name[len - 3]) == 'e' && epub_tolower(name[len - 2]) == 'p'
        && epub_tolower(name[len - 1]) == 'u' && epub_tolower(name[len]) == 'b';
}

void EpubBookNameFromEpub(ConstStr255Param epubName, Str255 bookName) {
    short len = epubName[0];
    short dot = 0;
    short i;

    if (len > 250) {
        len = 250;
    }
    bookName[0] = (unsigned char)len;
    memcpy(bookName + 1, epubName + 1, len);

    for (i = 1; i <= len; i++) {
        if (bookName[i] == '.') {
            dot = i;
        }
    }
    if (dot > 0 && dot + 4 <= len && bookName[dot + 1] == 'e' && bookName[dot + 2] == 'p'
        && bookName[dot + 3] == 'u' && bookName[dot + 4] == 'b') {
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

static void pump_events(void) {
    EventRecord e;
    short i;

    for (i = 0; i < 4; i++) {
        (void)EventAvail(everyEvent, &e);
        SystemTask();
    }
}

static OSErr epub_path_from_reply(const SFReply* reply, char* path, int pathLen) {
    if (!reply || !reply->good || pathLen < 2) {
        return paramErr;
    }
    epub_copy_pascal(path, reply->fName);
    if (path[0] == '\0') {
        return paramErr;
    }
    return noErr;
}

static OSErr DeleteSidecar(ConstStr255Param name, short vRefNum) {
    OSErr err = FSDelete(name, vRefNum);
    if (err == noErr || err == fnfErr) {
        return noErr;
    }
    return err;
}

static Boolean book_is_current(const SFReply* reply, ConstStr255Param bookName, long epubMod) {
    CInfoPBRec pb;
    OSErr err;
    Str255 pbook;
    long bookMod = 0;
    long bookLen = 0;
    short bookRef;

    memcpy(pbook, bookName, bookName[0] + 1);
    memset(&pb, 0, sizeof(pb));
    pb.hFileInfo.ioNamePtr = pbook;
    pb.hFileInfo.ioVRefNum = reply->vRefNum;
    pb.hFileInfo.ioFDirIndex = 0;
    err = PBGetCatInfoSync(&pb);
    if (err != noErr) {
        return false;
    }
    bookMod = pb.hFileInfo.ioFlMdDat;
    if (bookMod < epubMod) {
        return false;
    }
    if (OpenDataFile(bookName, reply->vRefNum, fsRdPerm, &bookRef) != noErr) {
        return false;
    }
    if (GetEOF(bookRef, &bookLen) != noErr || bookLen < 32) {
        FSClose(bookRef);
        return false;
    }
    FSClose(bookRef);
    return true;
}

OSErr EpubImportFromReply(const SFReply* reply, ReaderDoc* doc) {
    char* container;
    char* rootfile;
    char* opf;
    char content_dir[256];
    ZipArchive* zip;
    ImportCtx ictx;
    Str255 bookName;
    OSErr err;
    short bookRef = 0;
    short epubRef = 0;
    long epubMod = 0;
    CInfoPBRec pb;

    if (!reply || !doc) {
        return paramErr;
    }

    doc->chapterCount = 0;
    memset(doc->chapters, 0, sizeof(doc->chapters));

    /*
     * Open the EPUB immediately while SFGetFile still has the correct working
     * directory. Do not call SetVol (resets WD to volume root).
     */
    err = OpenDataFile(reply->fName, reply->vRefNum, fsRdPerm, &epubRef);
    if (err != noErr) {
        return err;
    }

    EpubBookNameFromEpub(reply->fName, bookName);
    memcpy(doc->epubSourceName, reply->fName, reply->fName[0] + 1);
    memcpy(doc->bookSourceName, bookName, bookName[0] + 1);
    doc->bookSourceVRefNum = reply->vRefNum;

    memset(&pb, 0, sizeof(pb));
    pb.hFileInfo.ioNamePtr = (StringPtr)reply->fName;
    pb.hFileInfo.ioVRefNum = reply->vRefNum;
    if (PBGetCatInfoSync(&pb) == noErr) {
        epubMod = pb.hFileInfo.ioFlMdDat;
    }
    doc->epubSourceModDate = epubMod;

    if (book_is_current(reply, bookName, epubMod)) {
        FSClose(epubRef);
        err = OpenDataFile(bookName, reply->vRefNum, fsRdPerm, &bookRef);
        if (err == noErr) {
            long eof;
            if (GetEOF(bookRef, &eof) == noErr) {
                doc->fileLen = eof;
            }
            FSClose(bookRef);
            doc->bookIndexPending = true;
            return noErr;
        }
    }

    zip = zip_open_ref(epubRef);
    if (!zip) {
        FSClose(epubRef);
        return fnfErr;
    }

    container = zip_read_entry(zip, "META-INF/container.xml");
    if (!container) {
        zip_close(zip);
        FSClose(epubRef);
        return fnfErr;
    }

    rootfile = find_rootfile(container);
    free(container);
    if (!rootfile) {
        zip_close(zip);
        FSClose(epubRef);
        return fnfErr;
    }

    content_dir[0] = '\0';
    {
        char* last_slash = strrchr(rootfile, '/');
        if (last_slash) {
            size_t len = (size_t)(last_slash - rootfile);
            if (len < sizeof(content_dir)) {
                memcpy(content_dir, rootfile, len);
                content_dir[len] = '\0';
            }
        }
    }

    opf = zip_read_entry(zip, rootfile);
    free(rootfile);
    if (!opf) {
        zip_close(zip);
        FSClose(epubRef);
        return fnfErr;
    }

    (void)DeleteSidecar(bookName, reply->vRefNum);

    err = Create(bookName, reply->vRefNum, 'TEXT', 'BOOK');
    if (err != noErr && err != dupErr) {
        free(opf);
        zip_close(zip);
        FSClose(epubRef);
        return err;
    }

    err = OpenDataFile(bookName, reply->vRefNum, fsWrPerm, &bookRef);
    if (err != noErr) {
        free(opf);
        zip_close(zip);
        FSClose(epubRef);
        return err;
    }

    memset(&ictx, 0, sizeof(ictx));
    ictx.bookRef = bookRef;
    ictx.writePos = 0;
    ictx.doc = doc;
    ictx.zip = zip;
    strcpy(ictx.content_dir, content_dir);

    pump_events();
    find_spine_items(opf, spine_import_cb, &ictx);
    free(opf);

    if (doc->chapterCount < 1) {
        FSClose(bookRef);
        zip_close(zip);
        FSClose(epubRef);
        (void)DeleteSidecar(bookName, reply->vRefNum);
        return paramErr;
    }

    {
        long eof;
        if (GetEOF(bookRef, &eof) == noErr) {
            doc->fileLen = eof;
        }
    }

    FSClose(bookRef);
    zip_close(zip);
    FSClose(epubRef);
    doc->bookIndexPending = true;
    return noErr;
}
