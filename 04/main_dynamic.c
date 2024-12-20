#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>

typedef float (*PiFunc)(int);
typedef char* (*TransFunc)(long);

int main() {
    printf("Dynamic linking example. Enter '0' to switch between lib1 and lib2.\n");
    printf("Initially loading lib1.\n");
    printf("Commands:\n");
    printf("0          -> switch library (lib1 <-> lib2)\n");
    printf("1 K        -> calculates Pi(K)\n");
    printf("2 x        -> translates x\n");
    
    const char* lib1_path = "./liblib1.so";
    const char* lib2_path = "./liblib2.so";
    const char* current_lib = lib1_path;
    
    void* handle = dlopen(current_lib, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "Error loading initial library: %s\n", dlerror());
        return 1;
    }

    PiFunc Pi = (PiFunc)dlsym(handle, "Pi");
    TransFunc translation = (TransFunc)dlsym(handle, "translation");
    if (!Pi || !translation) {
        fprintf(stderr, "Error resolving symbols: %s\n", dlerror());
        dlclose(handle);
        return 1;
    }

    char cmd[256];
    while (1) {
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        int command;
        if (sscanf(cmd, "%d", &command) != 1) continue;
        
        if (command == 0) {
            
            dlclose(handle);
            if (strcmp(current_lib, lib1_path) == 0) {
                current_lib = lib2_path;
            } else {
                current_lib = lib1_path;
            }
            
            handle = dlopen(current_lib, RTLD_LAZY);
            if (!handle) {
                fprintf(stderr, "Error loading %s: %s\n", current_lib, dlerror());
                return 1;
            }
            Pi = (PiFunc)dlsym(handle, "Pi");
            translation = (TransFunc)dlsym(handle, "translation");
            if (!Pi || !translation) {
                fprintf(stderr, "Error resolving symbols: %s\n", dlerror());
                dlclose(handle);
                return 1;
            }
            printf("Switched to %s\n", current_lib);
        } else if (command == 1) {
            int K;
            if (sscanf(cmd, "1 %d", &K) == 1) {
                float result = Pi(K);
                printf("Pi(%d) = %f\n", K, result);
            }
        } else if (command == 2) {
            long x;
            if (sscanf(cmd, "2 %ld", &x) == 1) {
                char* str = translation(x);
                if (str) {
                    printf("translation(%ld) = %s\n", x, str);
                    free(str);
                }
            }
        }
    }
    if (handle) dlclose(handle);
    return 0;
}
