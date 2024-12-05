#include <gtest/gtest.h>
#include <cstdlib>
#include <ctime>

extern "C" {
    #include "../02/include/utils.h"
    #include "../02/include/bitonic_sort.h"
    #include "../02/include/bitonic_data.h"

    // Объявляем глобальные переменные, если они используются в реализации
    int countOfActiveThreads = 0;
    int maxCountOfThreads = 0;
    pthread_mutex_t mutex;
}

// Тест базовой сортировки
TEST(BitonicSort, BasicSorting) {
    int input[] = {10, 30, 20, 5, 25, 15, 35, 0};
    int expected[] = {0, 5, 10, 15, 20, 25, 30, 35};

    BitonicData data;
    data.array = (int*)malloc(sizeof(int) * 8);
    data.low = 0;
    data.count = 8;
    data.direction = 1; // 1 для сортировки по возрастанию

    for (int i = 0; i < 8; ++i) {
        data.array[i] = input[i];
    }

    bitonic_sort(&data);

    for (int i = 0; i < 8; ++i) {
        ASSERT_EQ(data.array[i], expected[i]);
    }

    free(data.array);
}

// Тест сортировки пустого массива
TEST(BitonicSort, EmptyArray) {
    BitonicData data;
    data.array = nullptr;
    data.low = 0;
    data.count = 0;
    data.direction = 1;

    bitonic_sort(&data);

    ASSERT_EQ(data.array, nullptr);
}

// Тест сортировки массива с одним элементом
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

// Тест производительности и корректности параллельной сортировки
TEST(BitonicSort, ParallelCorrectnessAndPerformance) {
    if (pthread_mutex_init(&mutex, nullptr) != 0) {
        perror("Не удалось инициализировать мьютекс!\n");
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

    // Однопоточная сортировка
    maxCountOfThreads = 0;
    clock_t startConsistent = clock();
    bitonic_sort(&dataConsistent);
    clock_t endConsistent = clock();

    // Параллельная сортировка
    maxCountOfThreads = 4; // Укажите нужное количество потоков
    clock_t startParallel = clock();
    bitonic_sort(&dataParallel);
    clock_t endParallel = clock();

    // Проверяем корректность сортировки
    for (int i = 0; i < size; ++i) {
        ASSERT_EQ(dataConsistent.array[i], dataParallel.array[i]);
    }

    // Проверяем, что параллельная сортировка быстрее
    ASSERT_TRUE((endParallel - startParallel) < (endConsistent - startConsistent));

    free(dataConsistent.array);
    free(dataParallel.array);
    pthread_mutex_destroy(&mutex);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
