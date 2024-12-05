#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "bitonic_sort.h"
#include "utils.h" 


extern pthread_mutex_t mutex;
extern int countOfActiveThreads;
extern int maxCountOfThreads;


void compare_and_swap(int *array, int i, int j, int direction);
void bitonic_merge(int *array, int low, int count, int direction);
void* bitonic_sort_thread(void* arg);
void bitonic_sort_recursive(int *array, int low, int count, int direction);




void compare_and_swap(int *array, int i, int j, int direction) {
    if ((direction == 1 && array[i] > array[j]) || (direction == 0 && array[i] < array[j])) {
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}


void bitonic_merge(int *array, int low, int count, int direction) {
    if (count > 1) {
        int mid = count / 2;

        for (int i = low; i < low + mid; i++) {
            compare_and_swap(array, i, i + mid, direction);
        }

        bitonic_merge(array, low, mid, direction);
        bitonic_merge(array, low + mid, count - mid, direction);
    }
}


void* bitonic_sort_thread(void* arg) {
    BitonicData* data = (BitonicData*)arg;

    printf("Thread sorting: low=%d, count=%d, direction=%d\n", data->low, data->count, data->direction);

    bitonic_sort_recursive(data->array, data->low, data->count, data->direction);

    pthread_mutex_lock(&mutex);
    countOfActiveThreads--;
    pthread_mutex_unlock(&mutex);
    free(data); 
    return NULL;
}


void bitonic_sort_recursive(int *array, int low, int count, int direction) {
    if (count > 1) {
        int mid = count / 2;

        
        BitonicData *left_data = malloc(sizeof(BitonicData));
        BitonicData *right_data = malloc(sizeof(BitonicData));

        if (!left_data || !right_data) {
            perror("Failed to allocate memory for thread data");
            exit(EXIT_FAILURE);
        }

        *left_data = (BitonicData){array, low, mid, 1}; 
        *right_data = (BitonicData){array, low + mid, count - mid, 0}; 

        pthread_t left_thread, right_thread;
        int create_left_thread = 0, create_right_thread = 0;

        pthread_mutex_lock(&mutex);
        if (countOfActiveThreads < maxCountOfThreads) {
            countOfActiveThreads++;
            create_left_thread = pthread_create(&left_thread, NULL, bitonic_sort_thread, left_data) == 0;
        } else {
            create_left_thread = 0;
        }
        if (countOfActiveThreads < maxCountOfThreads) {
            countOfActiveThreads++;
            create_right_thread = pthread_create(&right_thread, NULL, bitonic_sort_thread, right_data) == 0;
        } else {
            create_right_thread = 0;
        }
        pthread_mutex_unlock(&mutex);

        
        if (!create_left_thread) {
            bitonic_sort_recursive(array, low, mid, 1);
            free(left_data);
        }
        if (!create_right_thread) {
            bitonic_sort_recursive(array, low + mid, count - mid, 0);
            free(right_data);
        }

        
        if (create_left_thread) pthread_join(left_thread, NULL);
        if (create_right_thread) pthread_join(right_thread, NULL);

        
        bitonic_merge(array, low, count, direction);
    }
}


void bitonic_sort(BitonicData *data) {
    printf("Starting bitonic sort: low=%d, count=%d, direction=%d\n", data->low, data->count, data->direction);
    bitonic_sort_recursive(data->array, data->low, data->count, data->direction);
}
