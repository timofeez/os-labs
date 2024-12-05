#include <gtest/gtest.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>

extern "C" {
    #include <utils.h>
    #include "parent.h"
}

// Тест функции RemoveVowels
TEST(test_remove_vowels, test_simple_case) {
    char str[] = "hello world";
    char expected[] = "hll wrld";
    RemoveVowels(str);
    ASSERT_STREQ(str, expected);
}

TEST(test_remove_vowels, test_case_insensitivity) {
    char str[] = "Hello World";
    char expected[] = "Hll Wrld";
    RemoveVowels(str);
    ASSERT_STREQ(str, expected);
}

TEST(test_remove_vowels, test_no_vowels) {
    char str[] = "bcdfg";
    char expected[] = "bcdfg";
    RemoveVowels(str);
    ASSERT_STREQ(str, expected);
}

TEST(test_remove_vowels, test_only_vowels) {
    char str[] = "aeiouAEIOU";
    char expected[] = "";
    RemoveVowels(str);
    ASSERT_STREQ(str, expected);
}

TEST(test_remove_vowels, test_special_characters) {
    char str[] = "a!e@i#o$u%";
    char expected[] = "!@#$%";
    RemoveVowels(str);
    ASSERT_STREQ(str, expected);
}


// Тест для фильтрации строк и удаления гласных
TEST(test_filter_and_remove_vowels, test_filter_and_remove_vowels) {
    const char* fileWithInput = "input.txt";

    // Входные данные с именами файлов
    const char* outputFile1 = "output1.txt";
    const char* outputFile2 = "output2.txt";

    constexpr int inputSize = 5;
    std::array<const char*, inputSize> input = {
            "first",      // длина 5 -> нечетное -> pipe1
            "second",     // длина 6 -> четное  -> pipe2
            "abcd",       // длина 4 -> четное  -> pipe2
            "efghi",      // длина 5 -> нечетное -> pipe1
            "q"           // завершение
    };

    // Создаем тестовый файл с входными данными
    {
        auto inFile = std::ofstream(fileWithInput);
        inFile << outputFile1 << '\n';
        inFile << outputFile2 << '\n';
        for (const auto& line : input) {
            inFile << line << '\n';
        }
    }

    // Открываем входной файл
    FILE* inFile = fopen(fileWithInput, "r");
    ASSERT_TRUE(inFile != nullptr);

    // Вызываем функцию Parent
    Parent("../01/child1", "../01/child2", inFile);

    // Проверяем содержимое выходных файлов
    std::ifstream file1(outputFile1);
    std::ifstream file2(outputFile2);

    ASSERT_TRUE(file1.good());
    ASSERT_TRUE(file2.good());

    std::string line;

    // Проверяем содержимое output1.txt
    std::vector<std::string> expectedFile1 = {
            "frst", "fgh"  // "first" и "efghi" без гласных
    };

    for (const auto& expected : expectedFile1) {
        ASSERT_TRUE(std::getline(file1, line));
        ASSERT_EQ(line, expected);
    }

    // Проверяем содержимое output2.txt
    std::vector<std::string> expectedFile2 = {
            "scnd", "bcd"  // "second" и "abcd" без гласных
    };

    for (const auto& expected : expectedFile2) {
        ASSERT_TRUE(std::getline(file2, line));
        ASSERT_EQ(line, expected);
    }

    // Удаляем файлы после теста
    file1.close();
    file2.close();
    fclose(inFile);

    std::filesystem::remove(fileWithInput);
    std::filesystem::remove(outputFile1);
    std::filesystem::remove(outputFile2);
}


int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
