/* kernel.c - DELIX kernel entry point */

#include "terminal.h"
#include "terminal_commands.h"

/* Write a byte to an I/O port */
void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Read a byte from an I/O port */
unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Use strcmp from the runtime string helpers */
extern int strcmp(const char* a, const char* b);

/* Find first space or end */
static int find_space(const char* s) {
    int i = 0;
    while (s[i] && s[i] != ' ') i++;
    return i;
}

/* Kernel main entry point */
void kernel_main(void) {
    terminal_initialize();
    /* initialize VFS */
    extern void vfs_init(void);
    vfs_init();

    print("Hello from DELIX v0.4!\n");
    print("> ");

    char input[128];
    while (1) {
        readline(input, sizeof(input));

        /* Skip empty input */
        if (strcmp(input, "") == 0) {
            print("> ");
            continue;
        }

        /* Parse command and args */
        int cmd_len = find_space(input);
        char cmd[32];
        int i;
        for (i = 0; i < cmd_len && i < (int)sizeof(cmd) - 1; i++) cmd[i] = input[i];
        cmd[i] = '\0';

        const char* args = NULL;
        if (input[cmd_len] == ' ') args = &input[cmd_len + 1];

        /* Search for matching command */
        int found = 0;
        int num = get_num_commands();
        for (int j = 0; j < num; j++) {
            const struct command* c = get_command(j);
            if (strcmp(cmd, c->name) == 0) {
                c->func(args);
                found = 1;
                break;
            }
        }

        /* Handle unknown command */
        if (!found) {
            print("Unknown command. Type 'help' for list of available commands.\n");
        }

        print("> ");
    }
}
