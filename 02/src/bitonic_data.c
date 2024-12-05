#include "bitonic_data.h"
#include <stdlib.h>
#include <stdio.h>


BitonicData* create_bitonic_data(int *array, int low, int count, int direction) {
    BitonicData *data = (BitonicData *)malloc(sizeof(BitonicData));
    if (!data) {
        perror("Failed to allocate memory for BitonicData");
        return NULL;
    }
    data->array = array;
    data->low = low;
    data->count = count;
    data->direction = direction;
    return data;
}


void free_bitonic_data(BitonicData *data) {
    if (data) {
        free(data);
    }
}


void print_bitonic_data(BitonicData *data) {
    if (!data || !data->array) {
        printf("Invalid BitonicData\n");
        return;
    }
    printf("Array: ");
    for (int i = data->low; i < data->low + data->count; i++) {
        printf("%d ", data->array[i]);
    }
    printf("\n");
}
