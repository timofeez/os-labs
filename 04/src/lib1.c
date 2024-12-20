#include <math.h>
#include <stdlib.h>
#include <string.h>

// Ряд Лейбница
float Pi(const int K) {
  float pi = 0.0;
  int sign = 1;
  for (int i = 0; i < K; i++) {
    pi += sign * 4.0 / (2 * i + 1);
    sign = -sign;
  }
  return pi;
}

// Реализация 1 функции перевода в другую систему счисления (двоичная)
char* translation(long x) {
    char* result = (char*)malloc(65);
    if (!result) return NULL;
    result[64] = '\0';
    int index = 63;
    do {
        result[index--] = '0' + (x % 2);
        x /= 2;
    } while (x > 0);
    return strdup(&result[index + 1]);
}