#include "zip_lite.h"
#include <string.h>
#include <stdlib.h>
#include "puff.h"

#define ZIP_END_SIG 0x06054b50
#define ZIP_CENTRAL_SIG 0x02014b50
#define ZIP_LOCAL_SIG 0x04034b50

// Byte swapping for Big Endian (68k)
#define SWAP32(x) ((((x) & 0xFF) << 24) | (((x) & 0xFF00) << 8) | (((x) & 0xFF0000) >> 8) | (((x) & 0xFF000000) >> 24))
#define SWAP16(x) ((((x) & 0xFF) << 8) | (((x) & 0xFF00) >> 8))

static uint32_t read_u32_le(FILE* f) {
    uint32_t val;
    fread(&val, 4, 1, f);
    return SWAP32(val);
}

static uint16_t read_u16_le(FILE* f) {
    uint16_t val;
    fread(&val, 2, 1, f);
    return SWAP16(val);
}

ZipArchive* zip_open(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        perror("zip_open: fopen failed");
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 22) {
        fprintf(stderr, "zip_open: File too small (%ld bytes)\n", size);
        fclose(f);
        return NULL;
    }

    long search_range = size > 4096 ? 4096 : size;
    fseek(f, -search_range, SEEK_END);
    unsigned char* buf = malloc(search_range);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    fread(buf, 1, search_range, f);

    long eocd_rel_offset = -1;
    for (long i = search_range - 22; i >= 0; i--) {
        if (buf[i] == 0x50 && buf[i+1] == 0x4b && buf[i+2] == 0x05 && buf[i+3] == 0x06) {
            eocd_rel_offset = i;
            break;
        }
    }
    free(buf);

    if (eocd_rel_offset == -1) {
        fprintf(stderr, "zip_open: EOCD signature not found\n");
        fclose(f);
        return NULL;
    }

    long eocd_offset = size - search_range + eocd_rel_offset;
    fseek(f, eocd_offset + 10, SEEK_SET);
    
    ZipArchive* zip = malloc(sizeof(ZipArchive));
    if (!zip) { fclose(f); return NULL; }
    zip->f = f;
    zip->entry_count = read_u16_le(f);
    fseek(f, 4, SEEK_CUR);
    zip->central_dir_offset = read_u32_le(f);

    return zip;
}

void zip_close(ZipArchive* zip) {
    if (zip) {
        fclose(zip->f);
        free(zip);
    }
}

int zip_find_entry(ZipArchive* zip, const char* name, ZipEntry* entry) {
    fseek(zip->f, zip->central_dir_offset, SEEK_SET);
    for (uint32_t i = 0; i < zip->entry_count; i++) {
        uint32_t sig = read_u32_le(zip->f);
        if (sig != ZIP_CENTRAL_SIG) break;

        fseek(zip->f, 6, SEEK_CUR);
        entry->method = read_u16_le(zip->f);
        fseek(zip->f, 8, SEEK_CUR);
        entry->comp_size = read_u32_le(zip->f);
        entry->uncomp_size = read_u32_le(zip->f);
        uint16_t name_len = read_u16_le(zip->f);
        uint16_t extra_len = read_u16_le(zip->f);
        uint16_t comment_len = read_u16_le(zip->f);
        fseek(zip->f, 8, SEEK_CUR);
        entry->offset = read_u32_le(zip->f);

        char entry_name[256];
        int read_len = name_len < 255 ? name_len : 255;
        fread(entry_name, 1, read_len, zip->f);
        entry_name[read_len] = '\0';
        fseek(zip->f, name_len - read_len + extra_len + comment_len, SEEK_CUR);

        if (strcmp(entry_name, name) == 0) {
            strcpy(entry->name, entry_name);
            return 1;
        }
    }
    return 0;
}

int zip_stream_entry(ZipArchive* zip, const ZipEntry* entry, ZipChunkCallback cb, void* user_data) {
    fseek(zip->f, entry->offset, SEEK_SET);
    uint32_t sig = read_u32_le(zip->f);
    if (sig != ZIP_LOCAL_SIG) return 0;

    fseek(zip->f, 22, SEEK_CUR);
    uint16_t name_len = read_u16_le(zip->f);
    uint16_t extra_len = read_u16_le(zip->f);
    fseek(zip->f, name_len + extra_len, SEEK_CUR);

    if (entry->method == 0) { // Uncompressed
        char chunk[2048];
        uint32_t remaining = entry->uncomp_size;
        while (remaining > 0) {
            uint32_t to_read = remaining > 2048 ? 2048 : remaining;
            fread(chunk, 1, to_read, zip->f);
            cb(chunk, to_read, user_data);
            remaining -= to_read;
        }
        return 1;
    } else if (entry->method == 8) { // Deflate
        unsigned char* comp_buf = malloc(entry->comp_size);
        if (!comp_buf) return 0;
        fread(comp_buf, 1, entry->comp_size, zip->f);
        
        unsigned char* uncomp_buf = malloc(entry->uncomp_size);
        if (!uncomp_buf) { free(comp_buf); return 0; }
        
        unsigned long destlen = entry->uncomp_size;
        unsigned long sourcelen = entry->comp_size;
        int res = puff(uncomp_buf, &destlen, comp_buf, &sourcelen);
        
        if (res == 0) {
            cb((const char*)uncomp_buf, destlen, user_data);
        }
        
        free(uncomp_buf);
        free(comp_buf);
        return (res == 0);
    }
    return 0;
}
