#ifndef GLOBALS_H
#define GLOBALS_H

#include <pthread.h>

extern pthread_mutex_t mutex;
extern int countOfActiveThreads;
extern int maxCountOfThreads;

#endif // GLOBALS_H
