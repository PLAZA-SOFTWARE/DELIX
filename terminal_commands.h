#ifndef TERMINAL_COMMANDS_H
#define TERMINAL_COMMANDS_H


void cmd_help(const char* args);


void cmd_version(const char* args);

void cmd_ping(const char* args);

void cmd_pkg(const char* args);

void cmd_ls(const char* args);
void cmd_cd(const char* args);
void cmd_run(const char* args);
void cmd_echo(const char* args);

/* Command descriptor */
struct command {
    const char* name;        // e.g. "version"
    void (*func)(const char* args);      // handler function accepts arguments
    const char* usage;       // e.g. "version - Show kernel version"
};

/* Get total number of registered commands */
int get_num_commands(void);

/* Get command descriptor by index */
const struct command* get_command(int index);

#endif
