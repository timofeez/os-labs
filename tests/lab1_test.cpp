#include <gtest/gtest.h> 
#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>

extern "C" {
    #include <utils.h>  // для функции RemoveVowels
    #include <parent.h> // для функции Parent
}

// Тест для фильтрации и удаления гласных из нечетных и четных строк
TEST(test_filter_and_remove_vowels, test_filter_and_remove_vowels) {
    const char* fileWithInput = "input.txt";

    constexpr int inputSize = 5;

    // Строки для тестирования
    std::array<const char*, inputSize> input = {
            "first line",
            "second line",
            "abcd line",
            "efghi line",
            "q line"
    };

    // Создаем тестовый файл с входными строками
    {
        auto inFile = std::ofstream(fileWithInput);

        for (const auto& line : input) {
            inFile << line << '\n';
        }
    }

    auto deleter = [](FILE* file) {
        fclose(file);
    };

    // Открываем файл для чтения
    std::unique_ptr<FILE, decltype(deleter)> inFile(fopen(fileWithInput, "r"), deleter);

    // Указатели на дочерние процессы (которые будут вызваны в функции Parent)
    int pipe1[2], pipe2[2];
    if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
        perror("Pipe failed");
        exit(-1);
    }

    pid_t pid1 = fork();
    if (pid1 == 0) { // Дочерний процесс для pipe1 (нечетные строки)
        close(pipe1[1]); // Закрываем неиспользуемую часть канала

        char line[256];
        int lineNumber = 1;
        while (fgets(line, sizeof(line), inFile.get())) {
            if (lineNumber % 2 != 0) { // Нечетная строка
                RemoveVowels(line); // Удаление гласных
                write(pipe1[1], line, strlen(line) + 1);
            }
            lineNumber++;
        }
        close(pipe1[1]); // Закрываем канал после записи
        exit(0);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) { // Дочерний процесс для pipe2 (четные строки)
        close(pipe2[1]); // Закрываем неиспользуемую часть канала

        char line[256];
        int lineNumber = 1;
        while (fgets(line, sizeof(line), inFile.get())) {
            if (lineNumber % 2 == 0) { // Четная строка
                RemoveVowels(line); // Удаление гласных
                write(pipe2[1], line, strlen(line) + 1);
            }
            lineNumber++;
        }
        close(pipe2[1]); // Закрываем канал после записи
        exit(0);
    }

    close(pipe1[1]); // Закрываем каналы в родительском процессе
    close(pipe2[1]);

    // Чтение из pipe1 (нечетные строки)
    char buffer1[256];
    int bytesRead;
    std::string firstProcessedOutput;
    while ((bytesRead = read(pipe1[0], buffer1, sizeof(buffer1))) > 0) {
        firstProcessedOutput.append(buffer1, bytesRead);
    }

    // Чтение из pipe2 (четные строки)
    char buffer2[256];
    std::string secondProcessedOutput;
    while ((bytesRead = read(pipe2[0], buffer2, sizeof(buffer2))) > 0) {
        secondProcessedOutput.append(buffer2, bytesRead);
    }

    // Ожидание завершения дочерних процессов
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    // Ожидаемые результаты
    std::string firstExpectedOutput = "frst ln";  // Нечетная строка "first line"
    std::string secondExpectedOutput = "scnd ln"; // Четная строка "second line"

    // Проверка на корректность обработки
    ASSERT_EQ(firstProcessedOutput, firstExpectedOutput);
    ASSERT_EQ(secondProcessedOutput, secondExpectedOutput);

    // Удаление файлов после теста
    auto removeIfExists = [](const char* path) {
        if (std::filesystem::exists(path)) {
            std::filesystem::remove(path);
        }
    };

    removeIfExists(fileWithInput);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
