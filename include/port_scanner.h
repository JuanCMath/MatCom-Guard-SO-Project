#ifndef SCANNER_H
#define SCANNER_H

int scan_mounts(const char *mount_dir, char **devices);
int scan_device(const char *device_path);
void mark_as_tracked(const char *path);
int is_tracked(const char *path);

#endif
