/* Simple read-only embedded VFS to support `ls`, `cd` and running basic scripts.
   For demo purposes this VFS stores a small tree in static arrays. In a real kernel
   you'd implement filesystem drivers. */

#include "vfs.h"
#include "../terminal.h"
#include <string.h>

static const char* root_entries[] = {"bin","home","README.txt"};
static const char* bin_entries[] = {"hello.sh","script.sh"};
static const char* home_entries[] = {"user.txt"};

static const char* file_hello = "echo Hello from embedded script!\n";
static const char* file_script = "echo This is a test script.\n";
static const char* file_user = "User: delix\n";
static const char* file_readme = "DELIX embedded VFS demo\n";

static const char* cwd = "/";

void vfs_init(void) {
    cwd = "/";
}

const char* vfs_get_cwd(void) {
    return cwd;
}

int vfs_ls(const char* path) {
    const char* p = path ? path : cwd;
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
    if (!path || path[0]=='\0') return 0;
    if (strcmp(path, "/") == 0 || strcmp(path, "") == 0) { cwd = "/"; return 0; }
    if (strcmp(path, "/bin") == 0) { cwd = "/bin"; return 0; }
    if (strcmp(path, "/home") == 0) { cwd = "/home"; return 0; }
    print("No such directory\n");
    return -1;
}

const char* vfs_readfile(const char* path) {
    if (strcmp(path, "/bin/hello.sh") == 0) return file_hello;
    if (strcmp(path, "/bin/script.sh") == 0) return file_script;
    if (strcmp(path, "/home/user.txt") == 0) return file_user;
    if (strcmp(path, "/README.txt") == 0) return file_readme;
    return NULL;
}

int vfs_file_size(const char* path) {
    const char* c = vfs_readfile(path);
    if (!c) return -1;
    int len = 0; while (c[len]) len++; return len;
}
