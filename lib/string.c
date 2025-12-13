#include <stddef.h>

/* Minimal C string/memory helpers for freestanding kernel */

size_t strlen(const char *s) {
    size_t n = 0;
    while (s && *s++) n++;
    return n;
}

int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return *(unsigned char*)a - *(unsigned char*)b;
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

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
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
