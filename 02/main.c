#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include "bitonic_sort.h"
#include "utils.h"
#include "bitonic_data.h"
#include "globals.h"


int main(int argc, char* argv[]) {
    if (argc == 3) {
        if (pthread_mutex_init(&mutex, NULL) != 0) {
            perror("Could not initialize mutex!\n");
            exit(-1);
        }

        maxCountOfThreads = atoi(argv[1]);

        int *array = NULL;
        int n = 0;

        if (*argv[2] == 'i') {
            printf("Введите количество элементов в массиве: ");
            scanf("%d", &n);

            array = (int*)malloc(n * sizeof(int));
            printf("Введите элементы массива:\n");
            for (int i = 0; i < n; i++) {
                scanf("%d", &array[i]);
            }
        } else {
            srand((unsigned int)time(NULL));
            n = rand() % 1000 + 1;

            array = (int *)malloc(n * sizeof(int));
            for (int i = 0; i < n; i++) {
                array[i] = rand() % 10000;
            }
            printf("Исходный массив:\n");
            for (int i = 0; i < n; i++) {
                printf("%d\n", array[i]);
            }
            printf("\n");
        }

        
        int new_n = next_power_of_two(n);
        int *padded_array = (int*)malloc(new_n * sizeof(int));
        for (int i = 0; i < n; i++) {
            padded_array[i] = array[i];
        }
        
        for (int i = n; i < new_n; i++) {
            padded_array[i] = INT_MAX;
        }

        free(array); 

        BitonicData data = {padded_array, 0, new_n, 1};
        bitonic_sort(&data);

        printf("Отсортированный массив:\n");
        for (int i = 0; i < n; i++) {
            printf("%d\n", padded_array[i]);
        }
        printf("\n");

        free(padded_array);
        pthread_mutex_destroy(&mutex);
    } else {
        printf("Использование: ./bitonic_sort <максимальное количество потоков> <i/r>\ni - ввести ");
        printf("произвольный массив\nr - сгенерировать случайный массив\n");
    }

    return 0;
}
