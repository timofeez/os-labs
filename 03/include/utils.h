#ifndef UTILS_H
#define UTILS_H

const static int MAX_BUFFER = 256;

typedef struct {
    char buffer[MAX_BUFFER];
    int updated; 
    int target;  
} SharedMemory;

void RemoveVowels(char *str);

#endif