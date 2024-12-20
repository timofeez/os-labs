#include <stdlib.h>
#include <string.h>

// Формула Валлиса
float Pi(const int K) {
    float pi = 1.0;
    for (int i = 1; i <= K; i++) {
        pi *= (4.0 * i * i) / (4.0 * i * i - 1);
    }
    return pi * 2;
}

// Реализация 2 функции перевода в другую систему счисления (троичная)
char* translation(long x) {
    char* result = (char*)malloc(65); 
    if (!result) return NULL;
    result[64] = '\0';
    int index = 63;
    do {
        result[index--] = '0' + (x % 3);
        x /= 3;
    } while (x > 0);
    return strdup(&result[index + 1]);
}