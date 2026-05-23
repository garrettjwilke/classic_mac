#include "mac_fs.h"
#include "epub_log.h"
#include <string.h>
#include <stdio.h>
#include <Files.h>

#ifndef dupErr
#define dupErr (-48)
#endif

#ifndef fnfErr
#define fnfErr (-43)
#endif

#ifndef ioDirMask
#define ioDirMask 0x10
#endif

static short g_vref = 0;
static long g_wd_dir_id = 2;

static Str255 g_pstr;
static FSSpec g_fspec;
static CInfoPBRec g_cat;
static WDPBRec g_wdpb;

void mac_fs_set_vref(short vref) {
    (void)vref;
    g_vref = 0;
}

short mac_fs_get_vref(void) {
    return g_vref;
}

void mac_fs_set_location(const char* epub_filename) {
    INTEGER vref_out = 0;
    LONGINT dir_id = 2;
    LONGINT proc_id = 0;
    FILE* test;

    g_vref = 0;

    if (GetWDInfo(0, &vref_out, &dir_id, &proc_id) == noErr && dir_id != 0) {
        g_wd_dir_id = dir_id;
    }

    test = fopen(epub_filename, "rb");
    if (test) {
        fclose(test);
        epub_log("fs epub fopen ok");
    } else {
        epub_log("fs epub fopen failed");
    }

    {
        char msg[48];
        sprintf(msg, "fs wd=%d", (int)g_wd_dir_id);
        epub_log(msg);
    }
}

void mac_path_to_pstr(Str255 dst, const char* src) {
    size_t n = strlen(src);
    if (n > 255) n = 255;
    dst[0] = (unsigned char)n;
    memcpy(dst + 1, src, n);
}

void mac_path_from_zip(char* path) {
    char* p = path;
    while (*p) {
        if (*p == '/') *p = ':';
        p++;
    }
}

static void log_fs_err(const char* step, OSErr err) {
    char msg[64];
    sprintf(msg, "fs %s err=%d", step, (int)err);
    epub_log(msg);
}

static long dir_id_for_spec(void) {
    memset(&g_cat, 0, sizeof(g_cat));
    g_cat.dirInfo.ioVRefNum = g_fspec.vRefNum;
    g_cat.dirInfo.ioDrDirID = g_fspec.parID;
    g_cat.dirInfo.ioNamePtr = g_fspec.name;
    g_cat.dirInfo.ioFDirIndex = 0;
    if (PBGetCatInfoSync(&g_cat) != noErr) return 0;
    if (!(g_cat.dirInfo.ioFlAttrib & ioDirMask)) return 0;
    return g_cat.dirInfo.ioDrDirID;
}

static int dir_create_segment(long parentID, const char* seg, long* outID) {
    int32_t newID = 0;
    OSErr err;

    mac_path_to_pstr(g_pstr, seg);
    err = FSMakeFSSpec(0, parentID, g_pstr, &g_fspec);
    if (err == noErr) {
        long id = dir_id_for_spec();
        if (!id) return 0;
        *outID = id;
        return 1;
    }
    if (err != fnfErr) {
        log_fs_err("MakeFSSpec", err);
        return 0;
    }
    err = FSpDirCreate(&g_fspec, 0, &newID);
    if (err != noErr) {
        log_fs_err("DirCreate", err);
        return 0;
    }
    *outID = newID;
    return 1;
}

void mac_delete_file(const char* mac_path) {
    if (!mac_path || strlen(mac_path) > 200) return;
    mac_path_to_pstr(g_pstr, mac_path);
    if (FSMakeFSSpec(0, g_wd_dir_id, g_pstr, &g_fspec) == noErr) {
        FSpDelete(&g_fspec);
    }
}

void mac_delete_folder(const char* folder_mac) {
    mac_delete_file(folder_mac);
}

int mac_create_folder(const char* folder_mac) {
    long newID = 0;

    if (!folder_mac || !folder_mac[0]) return 0;
    if (strchr(folder_mac, ':')) return 0;
    return dir_create_segment(g_wd_dir_id, folder_mac, &newID);
}

int mac_enter_output_folder(const char* folder_mac) {
    long newID = 0;
    long proc_id = 0;
    short vref = 0;

    if (!folder_mac || !folder_mac[0]) return 0;
    if (strchr(folder_mac, ':')) return 0;

    epub_log("fs create output folder");
    if (!dir_create_segment(g_wd_dir_id, folder_mac, &newID)) return 0;

    memset(&g_wdpb, 0, sizeof(g_wdpb));
    mac_path_to_pstr(g_pstr, folder_mac);
    g_wdpb.ioNamePtr = g_pstr;
    g_wdpb.ioWDVRefNum = 0;
    g_wdpb.ioWDDirID = g_wd_dir_id;
    if (PBHSetVolSync(&g_wdpb) != noErr) {
        epub_log("fs PBHSetVol failed");
        return 0;
    }
    if (GetWDInfo(0, &vref, &g_wd_dir_id, &proc_id) != noErr) {
        epub_log("fs GetWDInfo failed");
        return 0;
    }
    epub_log("fs entered output folder");
    return 1;
}

static char g_path_buf[256];

int mac_ensure_parent_dirs(const char* mac_path) {
    char* buf = g_path_buf;
    char* last_colon;
    char* seg;
    char* end;
    long dirID;

    if (!mac_path || !mac_path[0]) return 0;

    strncpy(buf, mac_path, 255);
    buf[255] = '\0';
    last_colon = strrchr(buf, ':');
    if (!last_colon) return 1;
    *last_colon = '\0';
    if (buf[0] == '\0') return 1;

    dirID = g_wd_dir_id;
    seg = buf;
    while (seg && *seg) {
        end = strchr(seg, ':');
        if (end) *end = '\0';

        if (!dir_create_segment(dirID, seg, &dirID)) return 0;

        if (!end) break;
        *end = ':';
        seg = end + 1;
    }

    return 1;
}
