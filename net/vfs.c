/* Simple read-write embedded VFS to support `ls`, `cd`, `run`, and a tiny editor 'nova'.
   This VFS stores a small tree in static arrays and allows writing to files (overwrite).
   Path resolution supports relative paths and '..'.
*/

#include "vfs.h"
#include "../terminal.h"
#include <string.h>
#include <stdio.h>

static const char* root_entries[] = {"bin","home","README.txt"};
static const char* bin_entries[] = {"hello.sh","script.sh"};
static const char* home_entries[] = {"user.txt"};

static char file_hello[128] = "echo Hello from embedded script!\n";
static char file_script[128] = "echo This is a test script.\n";
static char file_user[128] = "User: delix\n";
static char file_readme[128] = "DELIX embedded VFS demo\n";

static char cwd_buf[64] = "/";
static const char* cwd = cwd_buf;

/* Normalize and resolve path into out (must be at least 64 bytes). Returns 0 on success. */
static int resolve_path(const char* path, char* out, int out_len) {
    if (!path || !out || out_len < 2) return -1;
    char temp[128];
    if (path[0] == '/') {
        /* absolute */
        strncpy(temp, path, sizeof(temp)-1);
        temp[sizeof(temp)-1] = '\0';
    } else {
        /* relative: merge cwd and path */
        if (strcmp(cwd, "/") == 0) {
            snprintf(temp, sizeof(temp), "/%s", path);
        } else {
            snprintf(temp, sizeof(temp), "%s/%s", cwd, path);
        }
    }
    /* Split tokens and normalize '.' and '..' */
    char* parts[32];
    int pc = 0;
    char* tok = temp;
    /* skip leading slash */
    if (*tok == '/') tok++;
    char *p = tok;
    while (1) {
        if (*p == '/' || *p == '\0') {
            if (p > tok) {
                *p = '\0';
                parts[pc++] = tok;
            }
            if (*p == '\0') break;
            tok = p + 1;
        }
        p++;
        if (pc >= (int)(sizeof(parts)/sizeof(parts[0]))-1) break;
    }
    /* Build normalized path */
    char outtmp[128] = "";
    for (int i = 0; i < pc; i++) {
        if (strcmp(parts[i], ".") == 0) continue;
        if (strcmp(parts[i], "..") == 0) {
            /* remove last segment from outtmp */
            int len = strlen(outtmp);
            if (len == 0) continue;
            /* remove trailing slash */
            if (outtmp[len-1] == '/') outtmp[len-1] = '\0';
            /* find previous slash */
            char *q = strrchr(outtmp, '/');
            if (q) *q = '\0'; else outtmp[0] = '\0';
            continue;
        }
        /* append */
        if (strlen(outtmp) + 1 + strlen(parts[i]) + 1 >= sizeof(outtmp)) return -1;
        strcat(outtmp, "/");
        strcat(outtmp, parts[i]);
    }
    if (strlen(outtmp) == 0) strncpy(out, "/", out_len-1);
    else strncpy(out, outtmp, out_len-1);
    out[out_len-1] = '\0';
    return 0;
}

void vfs_init(void) {
    strcpy(cwd_buf, "/");
    cwd = cwd_buf;
}

const char* vfs_get_cwd(void) {
    return cwd;
}

int vfs_ls(const char* path) {
    char resolved[64];
    if (resolve_path(path ? path : cwd, resolved, sizeof(resolved)) != 0) {
        print("Path error\n"); return 0;
    }
    const char* p = resolved;
    if (strcmp(p, "/") == 0) {
        for (int i=0;i<(int)(sizeof(root_entries)/sizeof(root_entries[0]));i++) {
            print(root_entries[i]); print("\n");
        }
        return (int)(sizeof(root_entries)/sizeof(root_entries[0]));
    }
    if (strcmp(p, "/bin") == 0) {
        for (int i=0;i<(int)(sizeof(bin_entries)/sizeof(bin_entries[0]));i++) {
            print(bin_entries[i]); print("\n");
        }
        return (int)(sizeof(bin_entries)/sizeof(bin_entries[0]));
    }
    if (strcmp(p, "/home") == 0) {
        for (int i=0;i<(int)(sizeof(home_entries)/sizeof(home_entries[0]));i++) {
            print(home_entries[i]); print("\n");
        }
        return (int)(sizeof(home_entries)/sizeof(home_entries[0]));
    }
    print("No such directory\n");
    return 0;
}

int vfs_cd(const char* path) {
    char resolved[64];
    if (!path || path[0] == '\0') {
        strcpy(cwd_buf, "/"); cwd = cwd_buf; return 0;
    }
    if (resolve_path(path, resolved, sizeof(resolved)) != 0) {
        print("Path error\n"); return -1;
    }
    if (strcmp(resolved, "/") == 0 || strcmp(resolved, "") == 0) { strcpy(cwd_buf, "/"); cwd = cwd_buf; return 0; }
    if (strcmp(resolved, "/bin") == 0) { strcpy(cwd_buf, "/bin"); cwd = cwd_buf; return 0; }
    if (strcmp(resolved, "/home") == 0) { strcpy(cwd_buf, "/home"); cwd = cwd_buf; return 0; }
    print("No such directory\n");
    return -1;
}

const char* vfs_readfile(const char* path) {
    char resolved[64];
    if (resolve_path(path, resolved, sizeof(resolved)) != 0) return NULL;
    if (strcmp(resolved, "/bin/hello.sh") == 0) return file_hello;
    if (strcmp(resolved, "/bin/script.sh") == 0) return file_script;
    if (strcmp(resolved, "/home/user.txt") == 0) return file_user;
    if (strcmp(resolved, "/README.txt") == 0) return file_readme;
    return NULL;
}

int vfs_file_size(const char* path) {
    const char* c = vfs_readfile(path);
    if (!c) return -1;
    int len = 0; while (c[len]) len++; return len;
}

int vfs_writefile(const char* path, const char* data) {
    char resolved[64];
    if (resolve_path(path, resolved, sizeof(resolved)) != 0) return -1;
    if (strcmp(resolved, "/bin/hello.sh") == 0) {
        strncpy(file_hello, data, sizeof(file_hello)-1);
        file_hello[sizeof(file_hello)-1] = '\0';
        return 0;
    }
    if (strcmp(resolved, "/bin/script.sh") == 0) {
        strncpy(file_script, data, sizeof(file_script)-1);
        file_script[sizeof(file_script)-1] = '\0';
        return 0;
    }
    if (strcmp(resolved, "/home/user.txt") == 0) {
        strncpy(file_user, data, sizeof(file_user)-1);
        file_user[sizeof(file_user)-1] = '\0';
        return 0;
    }
    if (strcmp(resolved, "/README.txt") == 0) {
        strncpy(file_readme, data, sizeof(file_readme)-1);
        file_readme[sizeof(file_readme)-1] = '\0';
        return 0;
    }
    return -1;
}
