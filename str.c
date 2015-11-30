#include <lib.h>

size_t
strlen(const char* str) {
    size_t retval;
    for(retval = 0; *str != '\0'; ++str)
        retval++;

    return retval;
}

char*
strchr(const char* s, char c) {
    for (; *s != c; ++s)
        if (*s == '\0')
            return 0;

    return (char*) s;
}

char*
strstr(const char* str, const char* sub) {
    char *a, *b;

    b = (char*) sub;
    if (*b == 0)
        return (char*) str;

    for (; *str != 0; ++str) {
        if (*str != *b)
            continue;

        a = (char*) str;
        while (1) {
            if (*b == 0)
                return (char*) str;
            if (*a++ != *b++)
                break;
        }
        b = (char*) sub;
    }
    return NULL;
}

int
strcmp(const char* s1, const char* s2) {
    for ( ; *s1 == *s2; ++s1, ++s2)
    if (*s1 == '\0')
        return 0;

    return ((*(unsigned char*) s1 < *(unsigned char*) s2) ? -1 : +1);
}

char*
strcat(char* dst, const char* src) {
    strcpy(&dst[strlen(dst)], src);

    return dst;
}

char*
strcpy(char* dst, const char* src) {
    char* s = dst;

    while ((*s++ = *src++) != 0);

    return dst;
}

int
strncmp(const char* s1, const char* s2, size_t n) {
    for (; n > 0; ++s1, ++s2, --n) {
        if (*s1 != *s2)
            return ((*(unsigned char*) s1 < *(unsigned char*) s2) ? -1 : +1);
        if (*s1 == '\0')
            return 0;
    }

    return 0;
}

char*
strncat(char* dst, const char* src, size_t n) {
    if (n != 0) {
        char* d = dst;

        while (*d != 0)
            d++;
        do
            if ((*d++ = *src++) == 0)
                break;
        while (--n != 0);
        *d = 0;
    }

    return dst;
}

char*
strncpy(char* dst, const char* src, size_t n) {
    char* s = dst;
    while (n-- > 0 && *src != '\0')
        *s++ = *src++;

    while (n-- > 0)
        *s++ = '\0';

    return dst;
}