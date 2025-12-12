/* Simple read-write embedded VFS to support `ls`, `cd`, `run`, and a tiny editor 'nova'.
   This VFS stores a small tree in static arrays and allows writing to files (overwrite).
*/

#include "vfs.h"
#include "../terminal.h"
#include <string.h>

static const char* root_entries[] = {"bin","home","README.txt"};
static const char* bin_entries[] = {"hello.sh","script.sh"};
static const char* home_entries[] = {"user.txt"};

static char file_hello[128] = "echo Hello from embedded script!\n";
static char file_script[128] = "echo This is a test script.\n";
static char file_user[128] = "User: delix\n";
static char file_readme[128] = "DELIX embedded VFS demo\n";

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

int vfs_writefile(const char* path, const char* data) {
    if (strcmp(path, "/bin/hello.sh") == 0) {
        strncpy(file_hello, data, sizeof(file_hello)-1);
        file_hello[sizeof(file_hello)-1] = '\0';
        return 0;
    }
    if (strcmp(path, "/bin/script.sh") == 0) {
        strncpy(file_script, data, sizeof(file_script)-1);
        file_script[sizeof(file_script)-1] = '\0';
        return 0;
    }
    if (strcmp(path, "/home/user.txt") == 0) {
        strncpy(file_user, data, sizeof(file_user)-1);
        file_user[sizeof(file_user)-1] = '\0';
        return 0;
    }
    if (strcmp(path, "/README.txt") == 0) {
        strncpy(file_readme, data, sizeof(file_readme)-1);
        file_readme[sizeof(file_readme)-1] = '\0';
        return 0;
    }
    return -1;
}
