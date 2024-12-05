#include <gtest/gtest.h>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <iostream>

extern "C" {
    #include "../02/include/utils.h"
    #include "../02/include/bitonic_sort.h"
    #include "../02/include/bitonic_data.h"
     #include "../02/include/globals.h"

}


TEST(BitonicSort, BasicSorting) {
    int input[] = {10, 30, 20, 5, 25, 15, 35, 0};
    int expected[] = {0, 5, 10, 15, 20, 25, 30, 35};

    BitonicData data;
    data.array = (int*)malloc(sizeof(int) * 8);
    data.low = 0;
    data.count = 8;
    data.direction = 1; 

    for (int i = 0; i < 8; ++i) {
        data.array[i] = input[i];
    }

    bitonic_sort(&data);

    for (int i = 0; i < 8; ++i) {
        ASSERT_EQ(data.array[i], expected[i]);
    }

    free(data.array);
}


TEST(BitonicSort, EmptyArray) {
    BitonicData data;
    data.array = nullptr;
    data.low = 0;
    data.count = 0;
    data.direction = 1;

    bitonic_sort(&data);

    ASSERT_EQ(data.array, nullptr);
}


TEST(BitonicSort, SingleElement) {
    BitonicData data;
    data.array = (int*)malloc(sizeof(int));
    data.array[0] = 42;
    data.low = 0;
    data.count = 1;
    data.direction = 1;

    bitonic_sort(&data);

    ASSERT_EQ(data.array[0], 42);

    free(data.array);
}


TEST(BitonicSort, ParallelCorrectnessAndPerformance) {
    if (pthread_mutex_init(&mutex, nullptr) != 0) {
        perror("Failed to initialize mutex!\n");
    }

    const int size = 1000000;
    BitonicData dataConsistent;
    dataConsistent.array = (int*)malloc(sizeof(int) * size);
    dataConsistent.low = 0;
    dataConsistent.count = size;
    dataConsistent.direction = 1;

    BitonicData dataParallel;
    dataParallel.array = (int*)malloc(sizeof(int) * size);
    dataParallel.low = 0;
    dataParallel.count = size;
    dataParallel.direction = 1;

    for (int i = 0; i < size; ++i) {
        dataConsistent.array[i] = size - i;
        dataParallel.array[i] = size - i;
    }

    
    maxCountOfThreads = 0;
    auto startConsistent = std::chrono::high_resolution_clock::now();
    bitonic_sort(&dataConsistent);
    auto endConsistent = std::chrono::high_resolution_clock::now();

    
    maxCountOfThreads = 4; 
    auto startParallel = std::chrono::high_resolution_clock::now();
    bitonic_sort(&dataParallel);
    auto endParallel = std::chrono::high_resolution_clock::now();

    
    for (int i = 0; i < size; ++i) {
        ASSERT_EQ(dataConsistent.array[i], dataParallel.array[i]);
    }

    
    auto serialDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endConsistent - startConsistent).count();
    auto parallelDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endParallel - startParallel).count();

    std::cout << "Serial duration: " << serialDuration << " ms\n";
    std::cout << "Parallel duration: " << parallelDuration << " ms\n";

    ASSERT_TRUE(parallelDuration < serialDuration);

    free(dataConsistent.array);
    free(dataParallel.array);
    pthread_mutex_destroy(&mutex);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
