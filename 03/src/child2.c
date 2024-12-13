#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>

#include <utils.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        exit(1);
    }

    FILE *file = fopen(argv[1], "w");
    if (!file) {
        perror("Failed to open file");
        exit(1);
    }

    sem_t *semRead2 = sem_open("/sem_read2", O_RDWR);
    sem_t *semWrite = sem_open("/sem_write", O_RDWR);

    if (semWrite == SEM_FAILED || semRead2 == SEM_FAILED) {
        perror("semaphore's opening error");
        exit(-1);
    }

    int sharedFd = shm_open("/memory", O_RDWR, 0666);
    if (sharedFd == -1) {
        perror("shared memory connection error");
        exit(-1);
    }

    char *sharedMemory = mmap(NULL, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, sharedFd, 0);
    if (sharedMemory == MAP_FAILED) {
        perror("memory mapping error");
        exit(-1);
    }

    while (1) {
        sem_wait(semRead2);
        if (!strcmp(sharedMemory, "q")) {
            break;
        }

        RemoveVowels(sharedMemory);
        fprintf(file, "%s\n", sharedMemory);
        fflush(file);
        sem_post(semWrite);
    }

    fclose(file);

    sem_close(semRead2);
    sem_close(semWrite);

    munmap(sharedMemory, MEM_SIZE);
    close(sharedFd);

    return 0;
}