#include "zip_lite.h"
#include "epub_log.h"
#include "mac_fs.h"

#include <Files.h>
#include <string.h>
#include <stdlib.h>
#include "puff.h"

#define ZIP_CENTRAL_SIG 0x02014b50
#define ZIP_LOCAL_SIG 0x04034b50
#define ZIP_MAX_ENTRIES 256

static short g_vref = 0;
static char g_entry_name[256];

void zip_set_vref(short vref) {
    (void)vref;
    g_vref = 0;
}

static int zip_seek(ZipArchive* z, long pos) {
    if (z->fileRef > 0) {
        if (SetFPos(z->fileRef, fsFromStart, pos) != noErr) {
            return -1;
        }
        z->pos = pos;
        return 0;
    }
    if (!z->f || fseek(z->f, pos, SEEK_SET) != 0) {
        return -1;
    }
    z->pos = pos;
    return 0;
}

static int zip_read(ZipArchive* z, void* buf, long len) {
    if (len <= 0) {
        return 0;
    }
    if (z->fileRef > 0) {
        long count = len;
        if (FSRead(z->fileRef, &count, (Ptr)buf) != noErr || count != len) {
            return -1;
        }
        z->pos += len;
        return 0;
    }
    if (!z->f || fread(buf, 1, (size_t)len, z->f) != (size_t)len) {
        return -1;
    }
    z->pos += len;
    return 0;
}

static int zip_skip(ZipArchive* z, long n) {
    return zip_seek(z, z->pos + n);
}

static uint32_t zip_read_u32(ZipArchive* z) {
    unsigned char b[4];
    if (zip_read(z, b, 4) != 0) return 0;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static uint16_t zip_read_u16(ZipArchive* z) {
    unsigned char b[2];
    if (zip_read(z, b, 2) != 0) return 0;
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static ZipArchive* zip_open_common(long size, short fileRef, FILE* f) {
    long search_range;
    long tail_start;
    static unsigned char tail_buf[4096];
    long eocd_rel_offset;
    long eocd_offset;
    ZipArchive* zip;

    if (size < 22) {
        if (f) {
            fclose(f);
        }
        epub_log("zip_open: file too small");
        return NULL;
    }

    search_range = size > 4096 ? 4096 : size;
    tail_start = size - search_range;

    zip = (ZipArchive*)malloc(sizeof(ZipArchive));
    if (!zip) {
        if (f) {
            fclose(f);
        }
        return NULL;
    }

    memset(zip, 0, sizeof(*zip));
    zip->fileRef = fileRef;
    zip->f = f;
    zip->size = size;
    zip->pos = 0;

    if (zip_seek(zip, tail_start) != 0) {
        zip_close(zip);
        epub_log("zip_open: seek tail failed");
        return NULL;
    }
    if (zip_read(zip, tail_buf, search_range) != 0) {
        zip_close(zip);
        epub_log("zip_open: read tail failed");
        return NULL;
    }

    eocd_rel_offset = -1;
    for (long i = search_range - 22; i >= 0; i--) {
        if (tail_buf[i] == 0x50 && tail_buf[i + 1] == 0x4b && tail_buf[i + 2] == 0x05
            && tail_buf[i + 3] == 0x06) {
            eocd_rel_offset = i;
            break;
        }
    }
    if (eocd_rel_offset < 0) {
        zip_close(zip);
        epub_log("zip_open: EOCD not found");
        return NULL;
    }

    eocd_offset = size - search_range + eocd_rel_offset;
    if (zip_seek(zip, eocd_offset + 10) != 0) {
        zip_close(zip);
        return NULL;
    }

    zip->entry_count = zip_read_u16(zip);
    if (zip_skip(zip, 4) != 0) {
        zip_close(zip);
        return NULL;
    }
    zip->central_dir_offset = zip_read_u32(zip);

    if (zip->entry_count == 0 || zip->entry_count > ZIP_MAX_ENTRIES) {
        epub_log("zip_open: bad entry count");
        zip_close(zip);
        return NULL;
    }
    if ((long)zip->central_dir_offset >= size) {
        epub_log("zip_open: bad cd offset");
        zip_close(zip);
        return NULL;
    }

    epub_log("zip_open: ok");
    epub_log_status("zip_open ok");
    return zip;
}

ZipArchive* zip_open_ref(short refNum) {
    long size;

    if (refNum <= 0) {
        return NULL;
    }
    if (GetEOF(refNum, &size) != noErr) {
        return NULL;
    }
    epub_log("zip_open_ref: start");
    return zip_open_common(size, refNum, NULL);
}

ZipArchive* zip_open(const char* path) {
    FILE* f;
    long size;

    epub_log_status("zip_open start");
    epub_log("zip_open: start");

    f = fopen(path, "rb");
    if (!f) {
        epub_log("zip_open: fopen failed");
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        epub_log("zip_open: fseek end failed");
        return NULL;
    }
    size = ftell(f);
    epub_log("zip_open: start");
    return zip_open_common(size, 0, f);
}

void zip_close(ZipArchive* zip) {
    if (zip) {
        if (zip->f) {
            fclose(zip->f);
        }
        free(zip);
    }
}

int zip_find_entry(ZipArchive* zip, const char* name, ZipEntry* entry) {
    if (zip_seek(zip, (long)zip->central_dir_offset) != 0) return 0;

    for (uint32_t i = 0; i < zip->entry_count; i++) {
        uint32_t sig = zip_read_u32(zip);
        if (sig != ZIP_CENTRAL_SIG) break;

        if (zip_skip(zip, 6) != 0) return 0;
        entry->method = zip_read_u16(zip);
        if (zip_skip(zip, 8) != 0) return 0;
        entry->comp_size = zip_read_u32(zip);
        entry->uncomp_size = zip_read_u32(zip);
        uint16_t name_len = zip_read_u16(zip);
        uint16_t extra_len = zip_read_u16(zip);
        uint16_t comment_len = zip_read_u16(zip);
        if (zip_skip(zip, 8) != 0) return 0;
        entry->offset = zip_read_u32(zip);

        int read_len = name_len < 255 ? name_len : 255;
        if (zip_read(zip, g_entry_name, read_len) != 0) return 0;
        g_entry_name[read_len] = '\0';
        if (zip_skip(zip, name_len - read_len + extra_len + comment_len) != 0) return 0;

        if (strcmp(g_entry_name, name) == 0) {
            strcpy(entry->name, g_entry_name);
            return 1;
        }
    }
    return 0;
}

int zip_stream_entry(ZipArchive* zip, const ZipEntry* entry, ZipChunkCallback cb, void* user_data) {
    if (zip_seek(zip, (long)entry->offset) != 0) return 0;
    if (zip_read_u32(zip) != ZIP_LOCAL_SIG) return 0;
    if (zip_skip(zip, 22) != 0) return 0;

    uint16_t name_len = zip_read_u16(zip);
    uint16_t extra_len = zip_read_u16(zip);
    if (zip_skip(zip, name_len + extra_len) != 0) return 0;

    if (entry->method == 0) {
        char chunk[512];
        uint32_t remaining = entry->uncomp_size;
        while (remaining > 0) {
            uint32_t to_read = remaining > 512 ? 512 : remaining;
            if (zip_read(zip, chunk, (long)to_read) != 0) return 0;
            if (cb) cb(chunk, to_read, user_data);
            remaining -= to_read;
        }
        return 1;
    }

    if (entry->method == 8) {
        unsigned char* comp_buf;
        unsigned char* uncomp_buf;
        unsigned long destlen;
        unsigned long sourcelen;
        int res;

        if (entry->uncomp_size > 300000 || entry->comp_size > 300000) {
            return 0;
        }
        comp_buf = malloc(entry->comp_size);
        if (!comp_buf) return 0;
        if (zip_read(zip, comp_buf, (long)entry->comp_size) != 0) {
            free(comp_buf);
            return 0;
        }
        uncomp_buf = malloc(entry->uncomp_size);
        if (!uncomp_buf) {
            free(comp_buf);
            return 0;
        }
        destlen = entry->uncomp_size;
        sourcelen = entry->comp_size;
        res = puff(uncomp_buf, &destlen, comp_buf, &sourcelen);
        if (res == 0 && cb) {
            cb((const char*)uncomp_buf, destlen, user_data);
        }
        free(uncomp_buf);
        free(comp_buf);
        return (res == 0);
    }
    return 0;
}

int zip_foreach_entry(ZipArchive* zip, ZipEntryCallback cb, void* user_data) {
    if (zip_seek(zip, (long)zip->central_dir_offset) != 0) return 0;

    for (uint32_t i = 0; i < zip->entry_count; i++) {
        ZipEntry entry;

        if (zip_read_u32(zip) != ZIP_CENTRAL_SIG) break;
        if (zip_skip(zip, 6) != 0) return 0;
        entry.method = zip_read_u16(zip);
        if (zip_skip(zip, 8) != 0) return 0;
        entry.comp_size = zip_read_u32(zip);
        entry.uncomp_size = zip_read_u32(zip);
        uint16_t name_len = zip_read_u16(zip);
        uint16_t extra_len = zip_read_u16(zip);
        uint16_t comment_len = zip_read_u16(zip);
        if (zip_skip(zip, 8) != 0) return 0;
        entry.offset = zip_read_u32(zip);

        int read_len = name_len < 255 ? name_len : 255;
        if (zip_read(zip, g_entry_name, read_len) != 0) break;
        g_entry_name[read_len] = '\0';
        strcpy(entry.name, g_entry_name);
        if (zip_skip(zip, name_len - read_len + extra_len + comment_len) != 0) return 0;

        {
            long resume_pos = zip->pos;
            if (!cb(&entry, g_entry_name, user_data)) return 0;
            if (zip_seek(zip, resume_pos) != 0) return 0;
        }
    }
    return 1;
}

typedef struct {
    FILE* out;
} FileWriteCtx;

static void file_write_chunk(const char* data, size_t size, void* user_data) {
    FileWriteCtx* ctx = (FileWriteCtx*)user_data;
    if (ctx->out && size > 0) {
        fwrite(data, 1, size, ctx->out);
        fflush(ctx->out);
    }
}

int zip_extract_to_file(ZipArchive* zip, const ZipEntry* entry, const char* path) {
    FILE* out;
    FileWriteCtx ctx;
    int ok;

    if (!mac_ensure_parent_dirs(path)) return 0;

    out = fopen(path, "wb");
    if (!out) return 0;

    ctx.out = out;
    ok = zip_stream_entry(zip, entry, file_write_chunk, &ctx);
    fclose(out);
    return ok;
}

static char g_extract_rel[256];

static int extract_folder_cb(const ZipEntry* entry, const char* name, void* user_data) {
    ZipExtractFolderCtx* ctx = (ZipExtractFolderCtx*)user_data;
    char* rel = g_extract_rel;

    (void)ctx;
    strncpy(rel, name, 255);
    rel[255] = '\0';
    mac_path_from_zip(rel);

    {
        size_t n = strlen(rel);
        if (n > 0 && (rel[n - 1] == ':' || rel[n - 1] == '/')) return 1;
    }

    if (zip_extract_to_file(ctx->zip, entry, rel)) {
        ctx->ok++;
    } else {
        ctx->fail++;
    }
    return 1;
}

int zip_extract_all_to_folder(ZipArchive* zip, const char* folder_mac, ZipExtractFolderCtx* stats) {
    stats->zip = zip;
    stats->folder_mac = folder_mac;
    stats->ok = 0;
    stats->fail = 0;
    return zip_foreach_entry(zip, extract_folder_cb, stats);
}

static int delete_folder_cb(const ZipEntry* entry, const char* name, void* user_data) {
    const char* folder_mac = (const char*)user_data;
    char rel[256];
    char path[300];

    (void)entry;
    strncpy(rel, name, 255);
    rel[255] = '\0';
    mac_path_from_zip(rel);

    strcpy(path, folder_mac);
    strcat(path, ":");
    strcat(path, rel);
    mac_delete_file(path);
    return 1;
}

int zip_delete_all_in_folder(ZipArchive* zip, const char* folder_mac) {
    return zip_foreach_entry(zip, delete_folder_cb, (void*)folder_mac);
}
