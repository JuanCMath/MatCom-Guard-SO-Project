#include "port_scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_TRACKED 64
#define MAX_HASHES 8192
#define CHANGE_THRESHOLD 0.1

typedef struct {
    char path[512];
    int tracked;
} TrackedDevice;

typedef struct {
    char filepath[1024];
    unsigned char hash[SHA256_DIGEST_LENGTH];
} FileHash;

static TrackedDevice tracked[MAX_TRACKED];
static int tracked_count = 0;

static FileHash baseline[MAX_HASHES];
static int baseline_count = 0;

int is_tracked(const char *path) {
    for (int i = 0; i < tracked_count; ++i)
        if (strcmp(tracked[i].path, path) == 0) return 1;
    return 0;
}

void mark_as_tracked(const char *path) {
    if (tracked_count < MAX_TRACKED) {
        strcpy(tracked[tracked_count++].path, path);
    }
}

int compute_file_hash(const char *filepath, unsigned char *out_hash) {
    FILE *file = fopen(filepath, "rb");
    if (!file) return -1;

    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    unsigned char buffer[4096];
    size_t bytes;

    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) != 0)
        SHA256_Update(&ctx, buffer, bytes);

    SHA256_Final(out_hash, &ctx);
    fclose(file);
    return 0;
}

void recurse_hash(const char *dir) {
    DIR *dp = opendir(dir);
    if (!dp) return;

    struct dirent *entry;
    while ((entry = readdir(dp))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);

        struct stat st;
        if (stat(path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            recurse_hash(path);
        } else {
            unsigned char hash[SHA256_DIGEST_LENGTH];
            if (compute_file_hash(path, hash) == 0 && baseline_count < MAX_HASHES) {
                strcpy(baseline[baseline_count].filepath, path);
                memcpy(baseline[baseline_count].hash, hash, SHA256_DIGEST_LENGTH);
                baseline_count++;
            }
        }
    }

    closedir(dp);
}

int compare_hashes(const char *root_path) {
    int changed = 0;
    int scanned = 0;

    for (int i = 0; i < baseline_count; ++i) {
        unsigned char current_hash[SHA256_DIGEST_LENGTH];
        if (compute_file_hash(baseline[i].filepath, current_hash) == 0) {
            scanned++;
            if (memcmp(baseline[i].hash, current_hash, SHA256_DIGEST_LENGTH) != 0)
                changed++;
        }
    }

    if (scanned == 0) return 0;
    double ratio = (double)changed / scanned;
    return ratio > CHANGE_THRESHOLD;
}

int scan_mounts(const char *mount_dir, char **devices) {
    int count = 0;
    DIR *dp = opendir(mount_dir);
    if (!dp) return 0;

    struct dirent *entry;
    while ((entry = readdir(dp))) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 &&
            strcmp(entry->d_name, "..") != 0) {
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s%s", mount_dir, entry->d_name);
            devices[count++] = strdup(full_path);
        }
    }

    closedir(dp);
    return count;
}

int scan_device(const char *device_path) {
    baseline_count = 0;
    recurse_hash(device_path);
    sleep(1);  // espera mínima para detectar cambios
    return compare_hashes(device_path);
}
