#include <stddef.h>
#include <stdarg.h>

/* Minimal C string/memory helpers for freestanding kernel */

size_t strlen(const char *s) {
    size_t n = 0;
    while (s && *s++) n++;
    return n;
}

int strncmp(const char *a, const char *b, size_t n) {
    if (n == 0) return 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca != cb) return (int)ca - (int)cb;
        if (ca == '\0') return 0;
    }
    return 0;
}

int strcmp(const char *a, const char *b) {
    size_t i = 0;
    while (a[i] && b[i]) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca != cb) return (int)ca - (int)cb;
        i++;
    }
    return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    char ch = (char)c;
    for (; *s; s++) {
        if (*s == ch) last = s;
    }
    if (ch == '\0') return (char *)s; /* pointer to terminator */
    return (char *)last;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char*)dest;
    const unsigned char *s = (const unsigned char*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char*)s;
    for (size_t i = 0; i < n; i++) p[i] = (unsigned char)c;
    return s;
}

/* Minimal snprintf implementation supporting %s, %c and literal %%. Returns number of characters that would have been written. */
int snprintf(char *str, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    size_t written = 0;
    size_t avail = (size > 0) ? size - 1 : 0; /* reserve for NUL */
    char *out = str;

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            if (written < avail) out[written] = *p;
            written++;
            continue;
        }
        p++; /* skip % */
        if (*p == 's') {
            const char *s = va_arg(ap, const char*);
            if (!s) s = "(null)";
            for (const char *q = s; *q; q++) {
                if (written < avail) out[written] = *q;
                written++;
            }
        } else if (*p == 'c') {
            int c = va_arg(ap, int);
            if (written < avail) out[written] = (char)c;
            written++;
        } else if (*p == '%') {
            if (written < avail) out[written] = '%';
            written++;
        } else {
            /* unsupported specifier: treat as literal */
            if (written < avail) out[written] = '%';
            written++;
            if (*p) {
                if (written < avail) out[written] = *p;
                written++;
            }
        }
    }

    if (size > 0) {
        if (written < size) out[written] = '\0';
        else out[size-1] = '\0';
    }
    va_end(ap);
    return (int)written;
}
