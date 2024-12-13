#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>

#include <parent.h>
#include <utils.h>

void Parent(const char* pathToChild1, const char* pathToChild2, FILE* stream) {
    sem_unlink("/sem_read1");
    sem_unlink("/sem_read2");
    sem_unlink("/sem_write");

    sem_t *semRead1 = sem_open("/sem_read1", O_CREAT, 0666, 0);
    sem_t *semRead2 = sem_open("/sem_read2", O_CREAT, 0666, 0);
    sem_t *semWrite = sem_open("/sem_write", O_CREAT, 0666, 0);

    if (semWrite == SEM_FAILED || semRead1 == SEM_FAILED || semRead2 == SEM_FAILED) {
        perror("semaphore's creating error");
        exit(-1);
    }

    int sharedFd = shm_open("/memory", O_CREAT | O_RDWR, 0666);
    if (sharedFd == -1) {
        perror("shared memory creating error");
        exit(-1);
    }
    ftruncate(sharedFd, MEM_SIZE);

    char *sharedMemory = mmap(NULL, MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, sharedFd, 0);
    if (sharedMemory == MAP_FAILED) {
        perror("memory mapping error");
        exit(-1);
    }

    char fileName1[MAX_BUFFER];
    char fileName2[MAX_BUFFER];

    fgets(fileName1, MAX_BUFFER, stream);
    fileName1[strcspn(fileName1, "\n")] = 0;
    
    fgets(fileName2, MAX_BUFFER, stream);
    fileName2[strcspn(fileName2, "\n")] = 0;

    pid_t pid1 = fork();

    if (pid1 == -1) {
        perror("fork failed");
        exit(-1);
    }

    if (pid1 == 0) {
        execl(pathToChild1, pathToChild1, fileName1, NULL);
        perror("execl failed");
        exit(-1);
    }

    pid_t pid2 = fork();

    if (pid2 == -1) {
        perror("fork failed");
        exit(-1);
    }

    if (pid2 == 0) {
        execl(pathToChild2, pathToChild2, fileName2, NULL);
        perror("execl failed");
        exit(-1);
    }

    sem_post(semWrite);
    while (1) {
        sem_wait(semWrite);
        fgets(sharedMemory, MEM_SIZE, stream);
        sharedMemory[strcspn(sharedMemory, "\n")] = 0;

        if (!strcmp(sharedMemory, "q")) {
            sem_post(semRead1);
            sem_post(semRead2);
            break;
        }

        if (strlen(sharedMemory) % 2 == 1) {
            sem_post(semRead1);
        } else {
            sem_post(semRead2);
        }
    }

    wait(NULL);
    wait(NULL);

    sem_close(semRead1);
    sem_unlink("/sem_read1");
    sem_close(semRead2);
    sem_unlink("/sem_read2");
    sem_close(semWrite);
    sem_unlink("/sem_write");

    munmap(sharedMemory, MEM_SIZE);
    close(sharedFd);
    shm_unlink("/memory");
}