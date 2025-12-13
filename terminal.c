/* terminal.c - VGA terminal with robust US QWERTY keyboard input */

#include "terminal.h"
#include <string.h>

#define VGA_BUFFER ((volatile short*)0xB8000)
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

static size_t terminal_row = 0;
static size_t terminal_column = 0;

/* Simple I/O delay */
static inline void io_wait(void) {
    for (volatile int i = 0; i < 100; i++);
}

/* Update hardware cursor position */
static void update_cursor(void) {
    unsigned short pos = (unsigned short)(terminal_row * VGA_WIDTH + terminal_column);
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
}

/* Clear screen and reset cursor */
void terminal_clear(void) {
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_BUFFER[i] = (short)(' ' | (0x07 << 8));
    }
    terminal_row = 0;
    terminal_column = 0;
    update_cursor();
}

/* Initialize terminal */
void terminal_initialize(void) {
    terminal_clear();
}

/* Output a single character */
void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
    } else if (c == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
            VGA_BUFFER[terminal_row * VGA_WIDTH + terminal_column] = (short)(' ' | (0x07 << 8));
        }
    } else {
        VGA_BUFFER[terminal_row * VGA_WIDTH + terminal_column] = (short)(c | (0x07 << 8));
        terminal_column++;
        if (terminal_column >= VGA_WIDTH) {
            terminal_column = 0;
            terminal_row++;
        }
    }

    /* Scroll if needed */
    if (terminal_row >= VGA_HEIGHT) {
        for (size_t y = 1; y < VGA_HEIGHT; y++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                VGA_BUFFER[(y - 1) * VGA_WIDTH + x] = VGA_BUFFER[y * VGA_WIDTH + x];
            }
        }
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (short)(' ' | (0x07 << 8));
        }
        terminal_row = VGA_HEIGHT - 1;
    }

    update_cursor();
}

/* Print a null-terminated string */
void print(const char* str) {
    while (*str) {
        terminal_putchar(*str++);
    }
}

/* Special codes returned by terminal_getchar for arrow keys */
#define KEY_UP    -100
#define KEY_DOWN  -101
#define KEY_LEFT  -102
#define KEY_RIGHT -103

//Blocking getchar implementing ctrl combos and extended keys
int terminal_getchar(void) {
    int ctrl = 0;
    int shift = 0;
    while (1) {
        unsigned char status;
        /* wait until output buffer has data */
        do { status = inb(0x64); } while (!(status & 1));
        unsigned char scancode = inb(0x60);
        /* Handle extended prefix 0xE0 for arrow keys */
        if (scancode == 0xE0) {
            /* read next scancode byte */
            do { status = inb(0x64); } while (!(status & 1));
            unsigned char code2 = inb(0x60);
            unsigned char keydown2 = !(code2 & 0x80);
            if (!keydown2) continue;
            unsigned char code = code2 & 0x7F;
            switch (code) {
                case 0x48: return KEY_UP;
                case 0x50: return KEY_DOWN;
                case 0x4B: return KEY_LEFT;
                case 0x4D: return KEY_RIGHT;
                default: continue;
            }
        }
        unsigned char keydown = !(scancode & 0x80);
        unsigned char code = scancode & 0x7F;

        // Update modifier state
        if (code == 0x1D) { // Left Ctrl
            ctrl = keydown;
            continue;
        }
        if (code == 0x2A || code == 0x36) { // Shift keys
            shift = keydown;
            continue;
        }

        if (!keydown) continue; // ignore key releases

        // Map scancode to ASCII (simple US layout)
        char c = 0;
        switch (code) {
            case 0x10: c = 'a'; break; case 0x11: c = 'w'; break; case 0x12: c = 'e'; break; case 0x13: c = 'r'; break;
            case 0x14: c = 't'; break; case 0x15: c = 'y'; break; case 0x16: c = 'u'; break; case 0x17: c = 'i'; break;
            case 0x18: c = 'o'; break; case 0x19: c = 'p'; break; case 0x1A: c = '['; break; case 0x1B: c = ']'; break;
            case 0x1E: c = 'q'; break; case 0x1F: c = 's'; break; case 0x20: c = 'd'; break; case 0x21: c = 'f'; break;
            case 0x22: c = 'g'; break; case 0x23: c = 'h'; break; case 0x24: c = 'j'; break; case 0x25: c = 'k'; break;
            case 0x26: c = 'l'; break; case 0x27: c = ';'; break; case 0x28: c = '\''; break; case 0x29: c = '`'; break;
            case 0x2C: c = 'z'; break; case 0x2D: c = 'x'; break; case 0x2E: c = 'c'; break; case 0x2F: c = 'v'; break;
            case 0x30: c = 'b'; break; case 0x31: c = 'n'; break; case 0x32: c = 'm'; break; case 0x33: c = ','; break;
            case 0x34: c = '.'; break; case 0x35: c = '/'; break; case 0x2B: c = '\\'; break;
            case 0x02: c = '1'; break; case 0x03: c = '2'; break; case 0x04: c = '3'; break; case 0x05: c = '4'; break;
            case 0x06: c = '5'; break; case 0x07: c = '6'; break; case 0x08: c = '7'; break; case 0x09: c = '8'; break;
            case 0x0A: c = '9'; break; case 0x0B: c = '0'; break; case 0x0C: c = '-'; break; case 0x0D: c = '='; break;
            case 0x39: c = ' '; break;
            case 0x1C: return '\n'; // Enter
            case 0x0E: return 0x08; // Backspace
            default: c = 0; break;
        }

        if (!c) continue;

        if (ctrl) {
            // Ctrl-letter -> control code
            if (c >= 'a' && c <= 'z') return (c - 'a') + 1;
            if (c >= 'A' && c <= 'Z') return (c - 'A') + 1;
        }

        if (shift) {
            // shifted characters for letters and punctuation
            if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
            else {
                switch (c) {
                    case '1': c = '!'; break; case '2': c = '@'; break; case '3': c = '#'; break; case '4': c = '$'; break;
                    case '5': c = '%'; break; case '6': c = '^'; break; case '7': c = '&'; break; case '8': c = '*'; break;
                    case '9': c = '('; break; case '0': c = ')'; break; case '-': c = '_'; break; case '=': c = '+'; break;
                    case '[': c = '{'; break; case ']': c = '}'; break; case '\\': c = '|'; break; case ';': c = ':'; break;
                    case '\'\'': c = '"'; break; case ',': c = '<'; break; case '.': c = '>'; break; case '/': c = '?'; break;
                    case '`': c = '~'; break;
                    default: break;
                }
            }
        }

        return (int)c;
    }
}

// Simple command history for readline
#define HISTORY_COUNT 32
#define HISTORY_LEN 128
static char history[HISTORY_COUNT][HISTORY_LEN];
static int history_count = 0; /* number of stored entries */

//Reading keyboard with history and arrow handling
void readline(char* buffer, int maxlen) {
    int pos = 0;
    buffer[0] = '\0';
    int hist_index = history_count; /* position: history_count means current input */

    while (1) {
        int ch = terminal_getchar();

        if (ch == KEY_UP) {
            if (history_count == 0) continue;
            if (hist_index > 0) hist_index--;
            /* clear current buffer from screen */
            while (pos > 0) { terminal_putchar('\b'); pos--; }
            /* copy history entry */
            int i = 0; while (history[hist_index][i] && i < maxlen - 1) { buffer[i] = history[hist_index][i]; terminal_putchar(buffer[i]); i++; }
            pos = i; buffer[pos] = '\0';
            continue;
        }
        if (ch == KEY_DOWN) {
            if (history_count == 0) continue;
            if (hist_index < history_count) hist_index++;
            if (hist_index == history_count) {
                /* clear buffer */
                while (pos > 0) { terminal_putchar('\b'); pos--; }
                buffer[0] = '\0';
                continue;
            }
            /* show history[hist_index] */
            while (pos > 0) { terminal_putchar('\b'); pos--; }
            int i = 0; while (history[hist_index][i] && i < maxlen - 1) { buffer[i] = history[hist_index][i]; terminal_putchar(buffer[i]); i++; }
            pos = i; buffer[pos] = '\0';
            continue;
        }

        if (ch == '\n') {
            buffer[pos] = '\0';
            print("\n");
            /* store in history if non-empty and not duplicate of last */
            if (pos > 0) {
                if (history_count == 0 || strcmp(history[(history_count-1) % HISTORY_COUNT], buffer) != 0) {
                    /* append */
                    int idx = history_count % HISTORY_COUNT;
                    int i = 0; while (buffer[i] && i < HISTORY_LEN-1) { history[idx][i] = buffer[i]; i++; }
                    history[idx][i] = '\0';
                    history_count++;
                    if (history_count > HISTORY_COUNT) history_count = HISTORY_COUNT; /* cap but maintain circular buffer? simplified */
                }
            }
            return;
        }
        if (ch == 0x08) { // backspace
            if (pos > 0) {
                pos--; terminal_putchar('\b');
            }
            continue;
        }
        if (pos < maxlen - 1) {
            buffer[pos++] = (char)ch;
            buffer[pos] = '\0';
            terminal_putchar((char)ch);
        }
    }
}
