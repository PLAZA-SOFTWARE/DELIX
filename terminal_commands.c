/* terminal_commands.c - Built-in terminal commands for DELIX kernel */

#include "terminal_commands.h"
#include "terminal.h"
#include "net/net.h"
#include "net/vfs.h"

#define KERNEL_NAME    "DELIX"
#define KERNEL_VERSION "v0.3"

/* Show kernel version */
void cmd_version(const char* args) {
    (void)args;
    print(KERNEL_NAME);
    print(" Kernel ");
    print(KERNEL_VERSION);
    print("\n");
}

/* Display help with all available commands */
void cmd_help(const char* args) {
    (void)args;
    print("DELIX Kernel - Built-in commands:\n");
    int num = get_num_commands();
    for (int i = 0; i < num; i++) {
        const struct command* cmd = get_command(i);
        print("  ");
        print(cmd->usage);
        print("\n");
    }
}

/* ping: use net_ping to perform ICMP echo (stub until implemented) */
void cmd_ping(const char* args) {
    if (!args || args[0] == '\0') {
        print("Usage: ping <host>\n");
        return;
    }

    print("Pinging ");
    print(args);
    print(" ...\n");

    if (net_ping(args) == 0) {
        print("Ping successful\n");
    } else {
        print("Ping failed or not implemented\n");
    }
}

/* pkg install <github-url>
   Since kernel has no network/filesystem, record the request into pkg_requests.txt
   so a host-side script can perform actual 'git clone'. */
void cmd_pkg(const char* args) {
    if (!args || args[0] == '\0') {
        print("Usage: pkg install <github-url>\n");
        return;
    }

    /* parse subcommand */
    /* Expect: "install <url>" */
    int i = 0;
    while (args[i] && args[i] != ' ') i++;
    char sub[16];
    int j;
    for (j = 0; j < i && j < (int)sizeof(sub) - 1; j++) sub[j] = args[j];
    sub[j] = '\0';

    const char* url = (args[i] == ' ') ? &args[i + 1] : &args[i];

    if (strcmp(sub, "install") != 0) {
        print("Unknown pkg subcommand. Only 'install' supported.\n");
        return;
    }

    if (!url || url[0] == '\0') {
        print("Usage: pkg install <github-url>\n");
        return;
    }

    print("Requested install from: ");
    print(url);
    print("\n");

    /* Append to pkg_requests.txt in iso/ for host-side script to pick up */
    /* In freestanding kernel we cannot open files; simulate by printing a marker line
       that build tools can grep from serial/console output, or the Makefile can be
       extended to accept a URL argument. Here we print a special token. */

    print("[PKG_REQUEST]");
    print(url);
    print("\n");
}

/* ls: list current directory */
void cmd_ls(const char* args) {
    (void)args;
    vfs_ls(NULL);
}

/* cd: change directory */
void cmd_cd(const char* args) {
    if (!args || args[0]=='\0') {
        print("Usage: cd <path>\n");
        return;
    }
    if (vfs_cd(args) == 0) {
        print("OK\n");
    }
}

/* run: execute a "bash" script from /bin — supports simple lines starting with echo */
void cmd_run(const char* args) {
    if (!args || args[0]=='\0') {
        print("Usage: run <path>\n");
        return;
    }
    const char* content = vfs_readfile(args);
    if (!content) {
        print("No such file\n");
        return;
    }
    /* Very simple interpreter: split by lines and handle 'echo' */
    const char* p = content;
    char line[128];
    while (*p) {
        int len = 0;
        while (*p && *p != '\n' && len < (int)sizeof(line)-1) {
            line[len++] = *p++;
        }
        if (*p == '\n') p++;
        line[len] = '\0';
        /* trim leading spaces */
        int s = 0; while (line[s]==' ' || line[s]=='\t') s++;
        if (strncmp(&line[s], "echo", 4) == 0) {
            const char* msg = &line[s+4];
            while (*msg == ' ') msg++;
            print(msg); print("\n");
        } else if (line[0]) {
            print("Unknown command in script: "); print(line); print("\n");
        }
    }
}

void cmd_echo(const char* args) {
    if (!args) return; print(args); print("\n");
}

/* Command registry */
static const struct command commands[] = {
    {"version", cmd_version, "version - Show kernel version"},
    {"help",    cmd_help,    "help    - Show this help message"},
    {"ping",    cmd_ping,    "ping <host> - Basic network test"},
    {"pkg",     cmd_pkg,     "pkg install <github-url> - Request package install from GitHub"},
    {"ls",      cmd_ls,      "ls - list current directory"},
    {"cd",      cmd_cd,      "cd <path> - change directory"},
    {"run",     cmd_run,     "run <path> - execute embedded script"},
    {"echo",    cmd_echo,    "echo <text> - print text"},
};

int get_num_commands(void) {
    return sizeof(commands) / sizeof(commands[0]);
}

const struct command* get_command(int index) {
    return &commands[index];
}
