#include "utils.h"
#include <ctype.h>
#include <string.h>

void RemoveVowels(char* str) {
    char* src = str;
    char* dst = str;
    while (*src) {
        char lower = tolower((unsigned char)*src);
        if (lower != 'a' && lower != 'e' && lower != 'i' && lower != 'o' && lower != 'u') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}
