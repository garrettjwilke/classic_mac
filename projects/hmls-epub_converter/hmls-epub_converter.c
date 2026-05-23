#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Quickdraw.h>
#include <StandardFile.h>
#include <Files.h>
#include <Memory.h>

#include "zip_lite.h"
#include "xhtml_lite.h"

void p2cstrcpy(char* dst, ConstStr255Param src) {
    int len = src[0];
    memcpy(dst, &src[1], len);
    dst[len] = '\0';
}

typedef struct {
    ZipArchive* zip;
    char* content_dir;
    char* base_name;
    int part_idx;
} ConverterContext;

typedef struct {
    FILE* f;
    long count;
    XHTML_Parser parser;
} ParserState;

void write_callback(const char* text, void* user_data) {
    ParserState* ps = (ParserState*)user_data;
    size_t len = strlen(text);
    if (fputs(text, ps->f) != EOF) {
        ps->count += len;
    }
}

void zip_callback(const char* data, size_t size, void* user_data) {
    ParserState* ps = (ParserState*)user_data;
    xhtml_parser_process(&ps->parser, data, size);
}

void spine_callback(const char* href, void* user_data) {
    ConverterContext* ctx = (ConverterContext*)user_data;
    char full_path[512];
    if (ctx->content_dir[0]) {
        sprintf(full_path, "%s/%s", ctx->content_dir, href);
    } else {
        strcpy(full_path, href);
    }

    ZipEntry entry;
    if (zip_find_entry(ctx->zip, full_path, &entry)) {
        printf("  Part %d: %s (%ld bytes)\n", ctx->part_idx, full_path, entry.uncomp_size);
        long free_mem = MaxBlock();
        printf("    RAM: %ld\n", free_mem);
        fflush(stdout);

        // We still need memory for the compressed buffer and the uncompressed output.
        // In zip_stream_entry (current impl), we still malloc both.
        // But let's see if it fits now that we don't have overhead.
        
        char part_path[256];
        sprintf(part_path, "%s_p%d.txt", ctx->base_name, ctx->part_idx);
        
        FILE* out = fopen(part_path, "wb");
        if (out) {
            ParserState ps;
            ps.f = out;
            ps.count = 0;
            xhtml_parser_init(&ps.parser, write_callback, &ps);
            
            if (zip_stream_entry(ctx->zip, &entry, zip_callback, &ps)) {
                printf("    Wrote %ld chars to %s\n", ps.count, part_path);
            } else {
                printf("    Error: Decompression failed or Out of Memory.\n");
            }
            fclose(out);
        } else {
            printf("    Error: Could not create %s\n", part_path);
        }
        ctx->part_idx++;
    }
}

void convert_epub(const char* epub_path) {
    printf("Opening: %s\n", epub_path);
    ZipArchive* zip = zip_open(epub_path);
    if (!zip) {
        printf("Error: Could not open EPUB.\n");
        return;
    }

    ZipEntry entry;
    char* container_xml = NULL;
    if (zip_find_entry(zip, "META-INF/container.xml", &entry)) {
        container_xml = malloc(entry.uncomp_size + 1);
        if (zip_stream_entry(zip, &entry, NULL, NULL)) { // Need a simple read here
             // Wait, zip_stream_entry needs a callback.
             // Let's just use a simple block read for small metadata files.
        }
        // Actually, let's just implement a simple zip_read_entry again for small files.
    }
    
    // I'll fix convert_epub to use zip_stream_entry with a buffer for metadata
}

// Rewriting convert_epub with a helper for small files
static char* read_small_file(ZipArchive* zip, const char* name) {
    ZipEntry entry;
    if (zip_find_entry(zip, name, &entry)) {
        char* buf = malloc(entry.uncomp_size + 1);
        if (!buf) return NULL;
        
        // Use a simple buffer-filling callback
        struct { char* b; size_t p; } ctx = { buf, 0 };
        void fill_buf(const char* d, size_t s, void* u) {
            struct { char* b; size_t p; } *c = u;
            memcpy(c->b + c->p, d, s);
            c->p += s;
        }
        
        if (zip_stream_entry(zip, &entry, fill_buf, &ctx)) {
            buf[entry.uncomp_size] = '\0';
            return buf;
        }
        free(buf);
    }
    return NULL;
}

void convert_epub_v2(const char* epub_path) {
    printf("Opening: %s\n", epub_path);
    ZipArchive* zip = zip_open(epub_path);
    if (!zip) {
        printf("Error: Could not open EPUB.\n");
        return;
    }

    char* container_xml = read_small_file(zip, "META-INF/container.xml");
    if (!container_xml) {
        printf("Error: No container.xml\n");
        zip_close(zip);
        return;
    }

    char* rootfile_path = find_rootfile(container_xml);
    free(container_xml);
    if (!rootfile_path) {
        printf("Error: No rootfile.\n");
        zip_close(zip);
        return;
    }

    char content_dir[256] = "";
    char* last_slash = strrchr(rootfile_path, '/');
    if (last_slash) {
        size_t len = last_slash - rootfile_path;
        memcpy(content_dir, rootfile_path, len);
        content_dir[len] = '\0';
    }

    char* opf_xml = read_small_file(zip, rootfile_path);
    if (!opf_xml) {
        printf("Error: Could not read OPF.\n");
        free(rootfile_path);
        zip_close(zip);
        return;
    }

    char base_name[256];
    strcpy(base_name, epub_path);
    char* dot = strrchr(base_name, '.');
    if (dot) *dot = '\0';

    ConverterContext ctx = { zip, content_dir, base_name, 1 };
    find_spine_items(opf_xml, spine_callback, &ctx);

    free(opf_xml);
    free(rootfile_path);
    zip_close(zip);
    printf("\nConversion finished.\n");
}

int main(void) {
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);
    InitCursor();

    SFReply reply;
    Point where = {80, 50};
    SFGetFile(where, "\pSelect EPUB file:", NULL, -1, NULL, NULL, &reply);

    if (reply.good) {
        SetVol(NULL, reply.vRefNum);
        char filename[256];
        p2cstrcpy(filename, reply.fName);
        convert_epub_v2(filename);
    }

    printf("\nPress any key to exit\n");
    getchar();
    return 0;
}
