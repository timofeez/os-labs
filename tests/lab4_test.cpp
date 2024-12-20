#include <gtest/gtest.h>
#include <cstdlib>
#include <cmath>

extern "C" {
    #include <dlfcn.h>
    #include "contract.h"
}

TEST(Pi_second, test) {
    static void *library = dlopen("../04/liblib2.so", RTLD_LAZY);

    if (library == NULL) {
        fprintf(stderr, "Ошибка загрузки библиотеки: %s\n", dlerror());
        exit(-1);
    }

    float (*DynamicPi)(int) = (float(*)(int))dlsym(library, "Pi");

    constexpr int K = 10000;
    const float pi = DynamicPi(K);
    ASSERT_TRUE(abs(pi - M_PI) < 0.01);
}

// Перевод в двоичную
TEST(TestLibrary1, TestTranslationBinaryLib1) {
    void* handle = dlopen("../04/liblib1.so", RTLD_LAZY);
    ASSERT_NE(handle, nullptr) << "Ошибка загрузки библиотеки: " << dlerror();

    auto translation_func = (char* (*)(long))dlsym(handle, "translation");
    ASSERT_NE(translation_func, nullptr) << "Не удалось найти функцию translation";

    long x = 5;
    char* result = translation_func(x);
    EXPECT_STREQ(result, "101");
    free(result);

    dlclose(handle);
}

TEST(TestLibrary2, TestTranslationTernaryLib2) {
    void* handle = dlopen("../04/liblib2.so", RTLD_LAZY);
    ASSERT_NE(handle, nullptr) << "Ошибка загрузки библиотеки: " << dlerror();

    auto translation_func = (char* (*)(long))dlsym(handle, "translation");
    ASSERT_NE(translation_func, nullptr) << "Не удалось найти функцию translation";

    long x = 5;
    char* result = translation_func(x);
    EXPECT_STREQ(result, "12"); // 5 в троичной системе
    free(result);

    dlclose(handle);
}


int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}