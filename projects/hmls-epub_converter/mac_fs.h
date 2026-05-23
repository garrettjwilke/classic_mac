#ifndef MAC_FS_H
#define MAC_FS_H

#include <Files.h>

void mac_fs_set_vref(short vref);
void mac_fs_set_location(const char* epub_filename);
void mac_path_to_pstr(Str255 dst, const char* src);
short mac_fs_get_vref(void);
void mac_delete_file(const char* mac_path);

/* In-place: ZIP '/' paths become Mac ':' paths. */
void mac_path_from_zip(char* path);

/* Create every parent folder for a Mac path (file or folder). */
int mac_ensure_parent_dirs(const char* mac_path);

void mac_delete_folder(const char* folder_mac);
int mac_create_folder(const char* folder_mac);

/* chdir into output folder (relative zip paths after this). */
int mac_enter_output_folder(const char* folder_mac);

#endif
