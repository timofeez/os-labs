#include <string.h>
#include <ctype.h>
#include "utils.h"

void RemoveVowels(char *str) {
    int writeIndex = 0;
    int len = strlen(str);

    for (int i = 0; i < len; i++) {
        char c = tolower(str[i]); // Приводим символ к нижнему регистру для проверки
        if (c != 'a' && c != 'e' && c != 'i' && c != 'o' && c != 'u') {
            str[writeIndex++] = str[i]; // Копируем символ, если он не гласный
        }
    }

    str[writeIndex] = '\0'; // Завершаем строку
}
