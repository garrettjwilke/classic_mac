#ifndef ZIP_LITE_H
#define ZIP_LITE_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char name[256];
    uint32_t offset;
    uint32_t comp_size;
    uint32_t uncomp_size;
    uint16_t method;
} ZipEntry;

typedef struct {
    short fileRef;
    FILE* f;
    long size;
    long pos;
    uint32_t entry_count;
    uint32_t central_dir_offset;
} ZipArchive;

void zip_set_vref(short vref);

/* Open via stdio path (host / fallback). */
ZipArchive* zip_open(const char* path);

/* Open via open Toolbox ref (Classic Mac — keeps SF working directory). */
ZipArchive* zip_open_ref(short refNum);

void zip_close(ZipArchive* zip);

int zip_find_entry(ZipArchive* zip, const char* name, ZipEntry* entry);

typedef void (*ZipChunkCallback)(const char* data, size_t size, void* user_data);
int zip_stream_entry(ZipArchive* zip, const ZipEntry* entry, ZipChunkCallback cb, void* user_data);

typedef int (*ZipEntryCallback)(const ZipEntry* entry, const char* name, void* user_data);
int zip_foreach_entry(ZipArchive* zip, ZipEntryCallback cb, void* user_data);

int zip_extract_to_file(ZipArchive* zip, const ZipEntry* entry, const char* path);

typedef struct {
    ZipArchive* zip;
    const char* folder_mac;
    int ok;
    int fail;
} ZipExtractFolderCtx;

int zip_extract_all_to_folder(ZipArchive* zip, const char* folder_mac, ZipExtractFolderCtx* stats);
int zip_delete_all_in_folder(ZipArchive* zip, const char* folder_mac);

#endif
