#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Quickdraw.h>
#include <Menus.h>
#include <StandardFile.h>
#include <Files.h>
#include <Events.h>

#include "zip_lite.h"
#include "xhtml_lite.h"
#include "epub_log.h"
#include "mac_fs.h"

#ifndef dupErr
#define dupErr (-48)
#endif

typedef struct {
    char* buf;
    size_t pos;
} FillBufCtx;

static void fill_buf_callback(const char* data, size_t size, void* user_data) {
    FillBufCtx* ctx = (FillBufCtx*)user_data;
    memcpy(ctx->buf + ctx->pos, data, size);
    ctx->pos += size;
}

void p2cstrcpy(char* dst, ConstStr255Param src) {
    int len = src[0];
    memcpy(dst, &src[1], len);
    dst[len] = '\0';
}

static void basename_no_ext(const char* path, char* out, int outlen) {
    const char* start = path;
    const char* slash;
    const char* dot;
    int len;

    slash = strrchr(path, '/');
    if (!slash) slash = strrchr(path, ':');
    if (slash) start = slash + 1;

    dot = strrchr(start, '.');
    len = dot ? (int)(dot - start) : (int)strlen(start);
    if (len >= outlen) len = outlen - 1;
    memcpy(out, start, len);
    out[len] = '\0';
}

typedef struct {
    ZipArchive* zip;
    char* content_dir;
    char output_folder[64];
    int part_idx;
} ConverterContext;

typedef struct {
    XHTML_Parser parser;
} ParserState;

static void write_callback(const char* text, void* user_data) {
    FILE* out = (FILE*)user_data;
    size_t n;

    if (!out || !text || !text[0]) return;
    n = strlen(text);
    fwrite(text, 1, n, out);
}

static void zip_callback(const char* data, size_t size, void* user_data) {
    ParserState* ps = (ParserState*)user_data;
    xhtml_parser_process(&ps->parser, data, size);
}

static char g_spine_zip_path[256];
static char g_spine_out_path[64];
static char g_spine_msg[64];

static void spine_callback(const char* href, void* user_data) {
    ConverterContext* ctx = (ConverterContext*)user_data;
    char* zip_path = g_spine_zip_path;
    char* out_path = g_spine_out_path;
    char* msg = g_spine_msg;
    ZipEntry entry;
    FILE* out;

    if (ctx->content_dir[0]) {
        sprintf(zip_path, "%s/%s", ctx->content_dir, href);
    } else {
        strcpy(zip_path, href);
    }

    if (!zip_find_entry(ctx->zip, zip_path, &entry)) return;

    sprintf(out_path, "out_%02d.txt", ctx->part_idx);
    mac_ensure_parent_dirs(out_path);

    out = fopen(out_path, "wb");
    if (!out) return;

    {
        ParserState ps;
        xhtml_parser_init(&ps.parser, write_callback, out);

        if (zip_stream_entry(ctx->zip, &entry, zip_callback, &ps)) {
            sprintf(msg, "spine part %d ok", ctx->part_idx);
            epub_log(msg);
        } else {
            epub_log("spine: zip_stream_entry failed");
        }
        fclose(out);
    }
    ctx->part_idx++;
}

static char* read_small_file(ZipArchive* zip, const char* name) {
    ZipEntry entry;
    if (zip_find_entry(zip, name, &entry)) {
        char* buf = malloc(entry.uncomp_size + 1);
        if (!buf) {
            epub_log("read_small_file: malloc failed");
            return NULL;
        }

        FillBufCtx ctx = { buf, 0 };
        if (zip_stream_entry(zip, &entry, fill_buf_callback, &ctx)) {
            buf[entry.uncomp_size] = '\0';
            return buf;
        }
        free(buf);
        epub_log("read_small_file: zip_stream_entry failed");
    } else {
        epub_log("read_small_file: entry not found");
    }
    return NULL;
}

static int extract_epub_to_folder(ZipArchive* zip, const char* folder_mac) {
    ZipExtractFolderCtx stats;
    char msg[80];

    epub_log("extract: start");
    if (!zip_extract_all_to_folder(zip, folder_mac, &stats)) {
        epub_log("extract: foreach failed");
        return 0;
    }

    sprintf(msg, "extract: %d ok %d failed", stats.ok, stats.fail);
    epub_log(msg);
    printf("%s\n", msg);
    fflush(stdout);
    return stats.ok > 0;
}

static char g_convert_msg[128];
static char g_folder_mac[64];
static char g_content_dir[256];

void convert_epub_v2(const char* epub_path) {
    char* msg = g_convert_msg;
    char* folder_mac = g_folder_mac;
    ZipArchive* zip;

    basename_no_ext(epub_path, folder_mac, sizeof(folder_mac));

    sprintf(msg, "3 open %s", epub_path);
    epub_log_status(msg);
    printf("Opening: %s\n", epub_path);
    fflush(stdout);

    sprintf(msg, "convert: opening %s", epub_path);
    epub_log(msg);

    zip = zip_open(epub_path);
    if (!zip) {
        epub_log("convert: zip_open failed");
        printf("Error: Could not open EPUB.\n");
        fflush(stdout);
        return;
    }

    printf("Preparing folder: %s\n", folder_mac);
    fflush(stdout);
    epub_log("convert: enter output folder");
    if (!mac_enter_output_folder(folder_mac)) {
        epub_log("convert: could not enter output folder");
        zip_close(zip);
        return;
    }

    printf("Extracting files...\n");
    fflush(stdout);
    if (!extract_epub_to_folder(zip, "")) {
        zip_close(zip);
        return;
    }

    epub_log("convert: reading container.xml");
    {
        char* container_xml = read_small_file(zip, "META-INF/container.xml");
        char* rootfile_path;
        char* content_dir = g_content_dir;
        char* opf_xml;
        ConverterContext ctx;

        if (!container_xml) {
            epub_log("convert: no container.xml");
            zip_close(zip);
            return;
        }

        rootfile_path = find_rootfile(container_xml);
        free(container_xml);
        if (!rootfile_path) {
            epub_log("convert: no rootfile");
            zip_close(zip);
            return;
        }

        content_dir[0] = '\0';
        {
            char* last_slash = strrchr(rootfile_path, '/');
            if (last_slash) {
                size_t len = last_slash - rootfile_path;
                memcpy(content_dir, rootfile_path, len);
                content_dir[len] = '\0';
            }
        }

        opf_xml = read_small_file(zip, rootfile_path);
        if (!opf_xml) {
            free(rootfile_path);
            zip_close(zip);
            return;
        }

        ctx.zip = zip;
        strcpy(ctx.output_folder, folder_mac);
        strcpy(ctx.content_dir, content_dir);
        ctx.part_idx = 1;

        epub_log("convert: scanning spine");
        find_spine_items(opf_xml, spine_callback, &ctx);

        free(opf_xml);
        free(rootfile_path);
    }

    zip_close(zip);
    epub_log("convert: finished");
    printf("\nDone. Files in folder \"%s\"\n", folder_mac);
    fflush(stdout);
}

static void pump_events(void) {
    EventRecord event;
    int i;
    for (i = 0; i < 8; i++) {
        (void)EventAvail(everyEvent, &event);
        SystemTask();
    }
}

static int pick_epub_file(char* filename, int filename_len) {
    SFReply reply;
    Point where = {80, 50};

    SFGetFile(where, "\p", NULL, -1, NULL, NULL, &reply);
    if (!reply.good) return 0;

    if (reply.vRefNum > 0) {
        SetVol(NULL, reply.vRefNum);
    }
    p2cstrcpy(filename, reply.fName);
    if (filename_len > 0) {
        filename[filename_len - 1] = '\0';
    }
    return 1;
}

static void run_conversion(const char* filename) {
    epub_set_vref(0);
    zip_set_vref(0);
    mac_fs_set_vref(0);

    epub_log_init();
    epub_log("main: file selected");
    mac_fs_set_location(filename);

    epub_log_status("2 converting");
    convert_epub_v2(filename);

    epub_log_status("9 done");
    epub_log_close();
}

int main(void) {
    static char filename[64];

    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    InitDialogs(NULL);

    SetMenuBar(GetNewMBar(128));
    AppendResMenu(GetMenu(128), 'DRVR');
    DrawMenuBar();
    InitCursor();
    pump_events();

    if (pick_epub_file(filename, (int)sizeof(filename))) {
        run_conversion(filename);
    }

    ExitToShell();
    return 0;
}
