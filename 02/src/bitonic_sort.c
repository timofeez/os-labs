#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "bitonic_sort.h"
#include "utils.h"
#include "bitonic_data.h"
#include "globals.h"


#define MIN_COUNT_FOR_THREADS 10000 

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
        bitonic_merge(array, low + mid, mid, direction);
    }
}

void* bitonic_sort_thread(void* arg) {
    BitonicData* data = (BitonicData*)arg;

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

        int create_left_thread = 0, create_right_thread = 0;
        pthread_t left_thread, right_thread;

        if (count >= MIN_COUNT_FOR_THREADS) {
            pthread_mutex_lock(&mutex);
            if (countOfActiveThreads < maxCountOfThreads) {
                countOfActiveThreads++;
                create_left_thread = 1;
            }
            if (countOfActiveThreads < maxCountOfThreads) {
                countOfActiveThreads++;
                create_right_thread = 1;
            }
            pthread_mutex_unlock(&mutex);
        }

        if (create_left_thread) {
            BitonicData *left_data = malloc(sizeof(BitonicData));
            if (!left_data) {
                perror("Failed to allocate memory for thread data");
                exit(EXIT_FAILURE);
            }
            left_data->array = array;
            left_data->low = low;
            left_data->count = mid;
            left_data->direction = 1;

            if (pthread_create(&left_thread, NULL, bitonic_sort_thread, left_data) != 0) {
                perror("Failed to create thread");
                bitonic_sort_recursive(array, low, mid, 1);
                pthread_mutex_lock(&mutex);
                countOfActiveThreads--;
                pthread_mutex_unlock(&mutex);
                free(left_data);
            }
        } else {
            bitonic_sort_recursive(array, low, mid, 1);
        }

        if (create_right_thread) {
            BitonicData *right_data = malloc(sizeof(BitonicData));
            if (!right_data) {
                perror("Failed to allocate memory for thread data");
                exit(EXIT_FAILURE);
            }
            right_data->array = array;
            right_data->low = low + mid;
            right_data->count = mid;
            right_data->direction = 0;

            if (pthread_create(&right_thread, NULL, bitonic_sort_thread, right_data) != 0) {
                perror("Failed to create thread");
                bitonic_sort_recursive(array, low + mid, mid, 0);
                pthread_mutex_lock(&mutex);
                countOfActiveThreads--;
                pthread_mutex_unlock(&mutex);
                free(right_data);
            }
        } else {
            bitonic_sort_recursive(array, low + mid, mid, 0);
        }

        if (create_left_thread) {
            pthread_join(left_thread, NULL);
        }

        if (create_right_thread) {
            pthread_join(right_thread, NULL);
        }

        bitonic_merge(array, low, count, direction);
    }
}

void bitonic_sort(BitonicData *data) {
    bitonic_sort_recursive(data->array, data->low, data->count, data->direction);
}
