#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <utils.h>

#define MAX_BUFFER 256
#define SHARED_MEMORY_SIZE 1024


void Parent(const char* pathToChild1, const char* pathToChild2, FILE* stream) {
    int shm_fd;
    SharedMemory* shared_memory;

    
    shm_fd = shm_open("/shared_memory", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        exit(1);
    }

    
    if (ftruncate(shm_fd, sizeof(SharedMemory)) == -1) {
        perror("ftruncate failed");
        exit(1);
    }

    
    shared_memory = mmap(0, sizeof(SharedMemory), PROT_WRITE | PROT_READ, MAP_SHARED, shm_fd, 0);
    if (shared_memory == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }

    
    shared_memory->updated = 0;

    char fileName1[MAX_BUFFER], fileName2[MAX_BUFFER];

    
    printf("Введите имя файла для child1: ");
    fgets(fileName1, MAX_BUFFER, stream);
    fileName1[strcspn(fileName1, "\n")] = 0;

    printf("Введите имя файла для child2: ");
    fgets(fileName2, MAX_BUFFER, stream);
    fileName2[strcspn(fileName2, "\n")] = 0;

    pid_t pid1, pid2;

    
    if ((pid1 = fork()) == 0) {
        execl(pathToChild1, "child1", fileName1, NULL);
        perror("execl failed");
        exit(1);
    }

    
    if ((pid2 = fork()) == 0) {
        execl(pathToChild2, "child2", fileName2, NULL);
        perror("execl failed");
        exit(1);
    }

    char input[MAX_BUFFER];
    while (1) {
        printf("Введите строку (или 'q' для выхода): ");
        fgets(input, MAX_BUFFER, stream);
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "q") == 0) {
            printf("Parent: отправляем сигнал завершения дочерним процессам.\n");
            strncpy(shared_memory->buffer, "q", MAX_BUFFER);
            shared_memory->target = 0;
            shared_memory->updated = 1;

            usleep(500000); 
            break;
        }

        
        strncpy(shared_memory->buffer, input, MAX_BUFFER);
        shared_memory->target = (strlen(input) % 2 == 1) ? 1 : 2; 
        shared_memory->updated = 1;

        printf("Parent: отправлено '%s' в %s.\n", input, shared_memory->target == 1 ? "child1" : "child2");

        
        while (shared_memory->updated == 1) {
            usleep(100);
        }
        printf("Parent: обработка завершена.\n");
    }

    
    wait(NULL);
    wait(NULL);

    
    munmap(shared_memory, sizeof(SharedMemory));
    shm_unlink("/shared_memory");
    printf("Parent: завершение программы.\n");
}
