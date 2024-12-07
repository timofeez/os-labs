#ifndef UTILS_H
#define UTILS_H

#define MAX_BUFFER 256  

typedef struct {
    char buffer[MAX_BUFFER];  
    int updated; 
    int target;  
} SharedMemory;

void RemoveVowels(char *str);

#endif
