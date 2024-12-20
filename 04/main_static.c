#include <stdio.h>
#include <stdlib.h>
#include "contract.h"

int main() {
    printf("Static linking example (using lib1 by default)\n");
    printf("Enter commands:\n");
    printf("1 K   -> calculates Pi(K)\n");
    printf("2 x   -> translates x to another system\n");
    printf("Ctrl+C to exit.\n");
    
    char cmd[256];
    while (1) {
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        int command;
        if (sscanf(cmd, "%d", &command) != 1) continue;
        
        if (command == 1) {
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
    return 0;
}
