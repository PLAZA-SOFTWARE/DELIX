/* terminal_commands.c - Built-in terminal commands for DELIX kernel */

#include "terminal_commands.h"
#include "terminal.h"
#include "net/vfs.h"
#include <string.h>
#include <stdint.h>

#define KERNEL_NAME    "DELIX"
#define KERNEL_VERSION "v0.3"

/* Simple script variables */
#define MAX_VARS 16
#define VAR_NAME_LEN 16
#define VAR_VAL_LEN 64
static char var_names[MAX_VARS][VAR_NAME_LEN];
static char var_values[MAX_VARS][VAR_VAL_LEN];
static int var_count = 0;

static void vars_clear(void) {
    for (int i = 0; i < MAX_VARS; i++) {
        var_names[i][0] = '\0';
        var_values[i][0] = '\0';
    }
    var_count = 0;
}

static void vars_set(const char* name, const char* value) {
    if (!name || !value) return;
    for (int i = 0; i < var_count; i++) {
        if (strncmp(var_names[i], name, VAR_NAME_LEN) == 0) {
            strncpy(var_values[i], value, VAR_VAL_LEN - 1);
            var_values[i][VAR_VAL_LEN - 1] = '\0';
            return;
        }
    }
    if (var_count < MAX_VARS) {
        strncpy(var_names[var_count], name, VAR_NAME_LEN - 1);
        var_names[var_count][VAR_NAME_LEN - 1] = '\0';
        strncpy(var_values[var_count], value, VAR_VAL_LEN - 1);
        var_values[var_count][VAR_VAL_LEN - 1] = '\0';
        var_count++;
    }
}

static const char* vars_get(const char* name) {
    if (!name) return NULL;
    for (int i = 0; i < var_count; i++) {
        if (strncmp(var_names[i], name, VAR_NAME_LEN) == 0) return var_values[i];
    }
    return NULL;
}

/* Expand $VAR in input into output buffer. Returns 0 on success, -1 on overflow. */
static int expand_vars(char* out, int out_size, const char* in) {
    int oi = 0;
    for (int ii = 0; in[ii] != '\0'; ii++) {
        if (in[ii] == '$') {
            /* parse name */
            ii++;
            if (in[ii] == '\0') break;
            char name[VAR_NAME_LEN]; int ni = 0;
            while (in[ii] && ((in[ii] >= 'A' && in[ii] <= 'Z') || (in[ii] >= 'a' && in[ii] <= 'z') || (in[ii] >= '0' && in[ii] <= '9') || in[ii]=='_') && ni < VAR_NAME_LEN-1) {
                name[ni++] = in[ii++];
            }
            name[ni] = '\0';
            ii--; /* step back one since loop will increment */
            const char* val = vars_get(name);
            if (!val) val = "";
            for (int k = 0; val[k]; k++) {
                if (oi >= out_size - 1) return -1;
                out[oi++] = val[k];
            }
        } else {
            if (oi >= out_size - 1) return -1;
            out[oi++] = in[ii];
        }
    }
    out[oi] = '\0';
    return 0;
}

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

/* nova: simple editor. Usage: nova <path> — opens file for editing; save with :w and exit with :q */
void cmd_nova(const char* args) {
    if (!args || args[0] == '\0') {
        print("Usage: nova <path>\n");
        return;
    }
    const char* content = vfs_readfile(args);
    char buffer[1024];
    if (content) {
        int i = 0; while (content[i] && i < (int)sizeof(buffer)-1) { buffer[i] = content[i]; i++; }
        buffer[i] = '\0';
    } else {
        buffer[0] = '\0';
    }

    print("-- NOVA editor (type :w to save, :q to quit) --\n");
    print(buffer); print("\n");

    /* Very simple line editor: read lines from user until :q. If :w issued, write buffer to file. */
    char line[128];
    int pos = strlen(buffer);
    while (1) {
        print(":");
        /* read user input into line via readline — reuse terminal.readline (not exposed), so use readline from terminal.h */
        readline(line, sizeof(line));
        if (strcmp(line, ":q") == 0) {
            print("Exiting nova\n");
            break;
        } else if (strcmp(line, ":w") == 0) {
            if (vfs_writefile(args, buffer) == 0) print("Saved.\n"); else print("Save failed.\n");
        } else {
            /* append line to buffer */
            int l = 0; while (line[l]) l++;
            if (pos + l + 2 < (int)sizeof(buffer)) {
                buffer[pos++] = '\n';
                for (int k=0;k<l;k++) buffer[pos++] = line[k];
                buffer[pos] = '\0';
            } else {
                print("Buffer full\n");
            }
        }
    }
}

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

    vars_clear();

    const char* p = content;
    char line[128];
    char expanded[256];
    while (*p) {
        int len = 0;
        while (*p && *p != '\n' && len < (int)sizeof(line)-1) {
            line[len++] = *p++;
        }
        if (*p == '\n') p++;
        line[len] = '\0';
        /* trim leading spaces */
        int s = 0; while (line[s]==' ' || line[s]=='\t') s++;
        /* skip empty or comment */
        if (line[s] == '\0' || line[s] == '#') continue;

        /* variable assignment NAME=value (no spaces around = allowed) */
        int eq = -1;
        for (int k = s; line[k]; k++) if (line[k] == '=' && line[k-1] != ' ' && line[k+1] != ' ') { eq = k; break; }
        if (eq != -1) {
            /* parse name */
            char name[VAR_NAME_LEN]; int ni = 0;
            for (int k = s; k < eq && ni < VAR_NAME_LEN-1; k++) name[ni++] = line[k];
            name[ni] = '\0';
            /* parse value (rest), trim leading spaces */
            int vstart = eq + 1; while (line[vstart] == ' ' || line[vstart]=='\t') vstart++;
            char val[VAR_VAL_LEN]; int vi = 0;
            for (int k = vstart; line[k] && vi < VAR_VAL_LEN-1; k++) val[vi++] = line[k];
            val[vi] = '\0';
            vars_set(name, val);
            continue;
        }

        /* commands: echo, print, cd, ls, run, set */
        if (strncmp(&line[s], "echo", 4) == 0 && (line[s+4] == ' ' || line[s+4] == '\0')) {
            const char* msg = &line[s+4];
            while (*msg == ' ') msg++;
            if (expand_vars(expanded, sizeof(expanded), msg) == 0) {
                print(expanded); print("\n");
            } else {
                print("Expansion overflow\n");
            }
            continue;
        }
        if (strncmp(&line[s], "print", 5) == 0 && (line[s+5] == ' ' || line[s+5] == '\0')) {
            const char* msg = &line[s+5];
            while (*msg == ' ') msg++;
            if (expand_vars(expanded, sizeof(expanded), msg) == 0) {
                print(expanded); print("\n");
            } else {
                print("Expansion overflow\n");
            }
            continue;
        }
        if (strncmp(&line[s], "cd", 2) == 0 && (line[s+2] == ' ' || line[s+2] == '\0')) {
            const char* path = &line[s+2]; while (*path == ' ') path++;
            if (vfs_cd(path) == 0) print("OK\n"); else print("No such directory\n");
            continue;
        }
        if (strncmp(&line[s], "ls", 2) == 0 && (line[s+2] == ' ' || line[s+2] == '\0')) {
            vfs_ls(NULL);
            continue;
        }
        if (strncmp(&line[s], "run", 3) == 0 && (line[s+3] == ' ' || line[s+3] == '\0')) {
            const char* path = &line[s+3]; while (*path == ' ') path++;
            /* nested run: read file and interpret lines (simple recursion) */
            const char* nested = vfs_readfile(path);
            if (!nested) { print("No such file\n"); continue; }
            /* recursively interpret by calling cmd_run on path */
            cmd_run(path);
            continue;
        }
        if (strncmp(&line[s], "set", 3) == 0 && (line[s+3] == ' ' || line[s+3] == '\0')) {
            /* list variables */
            for (int i=0;i<var_count;i++) {
                print(var_names[i]); print("="); print(var_values[i]); print("\n");
            }
            continue;
        }

        /* unknown */
        print("Unknown command in script: "); print(&line[s]); print("\n");
    }
}

void cmd_echo(const char* args) { if (!args) return; print(args); print("\n"); }

/* Command registry */
static const struct command commands[] = {
    {"version", cmd_version, "version - Show kernel version"},
    {"help",    cmd_help,    "help    - Show this help message"},
    {"ls",      cmd_ls,      "ls - list current directory"},
    {"cd",      cmd_cd,      "cd <path> - change directory"},
    {"nova",    cmd_nova,    "nova <path> - open simple editor"},
    {"run",     cmd_run,     "run <path> - execute embedded script"},
    {"echo",    cmd_echo,    "echo <text> - print text"},
};

int get_num_commands(void) { return sizeof(commands) / sizeof(commands[0]); }
const struct command* get_command(int index) { return &commands[index]; }
