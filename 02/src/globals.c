#include <pthread.h>
#include <globals.h>

pthread_mutex_t mutex;
int countOfActiveThreads = 0;
int maxCountOfThreads = 0;
