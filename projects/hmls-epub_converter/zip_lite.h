#ifndef ZIP_LITE_H
#define ZIP_LITE_H

#include <stdio.h>
#include <stdint.h>

typedef struct {
    char name[256];
    uint32_t offset;
    uint32_t comp_size;
    uint32_t uncomp_size;
    uint16_t method;
} ZipEntry;

typedef struct {
    FILE* f;
    uint32_t entry_count;
    uint32_t central_dir_offset;
} ZipArchive;

ZipArchive* zip_open(const char* path);
void zip_close(ZipArchive* zip);

int zip_find_entry(ZipArchive* zip, const char* name, ZipEntry* entry);

// Callback-based reading to save memory
typedef void (*ZipChunkCallback)(const char* data, size_t size, void* user_data);
int zip_stream_entry(ZipArchive* zip, const ZipEntry* entry, ZipChunkCallback cb, void* user_data);

#endif
