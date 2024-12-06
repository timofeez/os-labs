#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <utils.h>

#define MAX_BUFFER 256

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        exit(1);
    }

    const char* filename = argv[1];
    FILE* file = fopen(filename, "a");
    if (!file) {
        perror("Failed to open file");
        exit(1);
    }

    int shm_fd;
    SharedMemory* shared_memory;

    
    shm_fd = shm_open("/shared_memory", O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        exit(1);
    }

    
    shared_memory = mmap(0, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_memory == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }

    printf("%s: процесс запущен.\n", argv[0]);

    while (1) {
        
        while (shared_memory->updated == 0) {
            usleep(100);
        }

        
        if (strcmp(shared_memory->buffer, "q") == 0 && shared_memory->target == 0) {
            printf("%s: сигнал завершения получен. Завершаю работу.\n", argv[0]);
            break;
        }

        
        if ((strcmp(argv[0], "child1") == 0 && shared_memory->target == 1) ||
            (strcmp(argv[0], "child2") == 0 && shared_memory->target == 2)) {
            char buffer[MAX_BUFFER];
            strncpy(buffer, shared_memory->buffer, MAX_BUFFER);

            printf("%s: получено '%s'. Обрабатываю.\n", argv[0], buffer);
            RemoveVowels(buffer);
            fprintf(file, "%s\n", buffer);
            fflush(file);
            printf("%s: записано '%s' в файл.\n", argv[0], buffer);

            
            shared_memory->updated = 0;
        }
    }

    fclose(file);
    munmap(shared_memory, sizeof(SharedMemory));
    close(shm_fd);
    printf("%s: завершение.\n", argv[0]);

    return 0;
}
