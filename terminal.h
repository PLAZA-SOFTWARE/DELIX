/* terminal.h - Terminal interface for DELIX kernel */

#ifndef TERMINAL_H
#define TERMINAL_H

#include <stddef.h>

/* Hardware I/O functions (implemented in kernel.c) */
void outb(unsigned short port, unsigned char val);
unsigned char inb(unsigned short port);
void outl(unsigned short port, unsigned int val);
unsigned int inl(unsigned short port);

/* Terminal control */
void terminal_initialize(void);
void terminal_clear(void);
void terminal_putchar(char c);
void print(const char* str);
void readline(char* buffer, int maxlen);

/* Get a single character (returns ASCII; supports ctrl combinations and backspace)
   Blocks until a key is pressed. */
int terminal_getchar(void);

#endif
