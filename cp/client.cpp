#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>
#include <errno.h>
#include <thread>

// --------------------------------------------------
// Те же константы, что и в server.cpp
// --------------------------------------------------
static const char* LOGIN_SHM_NAME         = "/mychat_login_shm";
static const char* LOGIN_SEM_SERVER_NAME  = "/mychat_login_sem_server";
static const char* LOGIN_SEM_CLIENT_NAME  = "/mychat_login_sem_client";

static const size_t MAX_CMD_LEN = 512;

// Структура для сегмента "логин"
struct LoginSegment {
    bool  requestReady;
    bool  responseReady;
    char  buffer[MAX_CMD_LEN];
};

// Структура для канала отдельного клиента
struct ClientSegment {
    bool  requestReady;
    bool  responseReady;
    char  buffer[MAX_CMD_LEN];
};

// --------------------------------------------------
// Утилиты для генерации имён памяти и семафоров
// (должны совпадать с серверными)
// --------------------------------------------------
static std::string make_client_shm_name(pid_t pid) {
    return "/mychatshm_" + std::to_string(pid);
}
static std::string make_sem_server_name(pid_t pid) {
    return "/sem_server_" + std::to_string(pid);
}
static std::string make_sem_client_name(pid_t pid) {
    return "/sem_client_" + std::to_string(pid);
}

// --------------------------------------------------
// Утилита для отладочного вывода
// --------------------------------------------------
static void debug_log(const std::string& msg) {
    std::cerr << "[CLIENT DEBUG] " << msg << std::endl;
}

// --------------------------------------------------
// main
// --------------------------------------------------
int main() 
{
    // 1) Подключаемся к логин-сегменту
    int loginFd = shm_open(LOGIN_SHM_NAME, O_RDWR, 0666);
    if (loginFd < 0) {
        std::cerr << "Cannot open login shm, maybe server not started, errno=" << errno << std::endl;
        return 1;
    }
    void* loginAddr = mmap(nullptr, sizeof(LoginSegment), PROT_READ|PROT_WRITE, MAP_SHARED, loginFd, 0);
    close(loginFd);
    if (loginAddr == MAP_FAILED) {
        std::cerr << "mmap login failed, errno=" << errno << std::endl;
        return 1;
    }
    LoginSegment* loginSeg = reinterpret_cast<LoginSegment*>(loginAddr);

    // 2) Открываем семафоры
    sem_t* loginSemServer = sem_open(LOGIN_SEM_SERVER_NAME, 0);
    if (loginSemServer == SEM_FAILED) {
        std::cerr << "sem_open(loginSemServer) failed, errno=" << errno << std::endl;
        return 1;
    }
    sem_t* loginSemClient = sem_open(LOGIN_SEM_CLIENT_NAME, 0);
    if (loginSemClient == SEM_FAILED) {
        std::cerr << "sem_open(loginSemClient) failed, errno=" << errno << std::endl;
        return 1;
    }

    // Спросим логин
    std::cout << "Enter login: ";
    std::string nickname;
    std::cin >> nickname;
    // Очистим возможный '\n'
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // Формируем запрос логина: "login <pid> <nickname>"
    pid_t pid = getpid();
    {
        std::ostringstream oss;
        oss << "login " << pid << " " << nickname;
        std::string req = oss.str();

        strncpy(loginSeg->buffer, req.c_str(), MAX_CMD_LEN-1);
        loginSeg->buffer[MAX_CMD_LEN-1] = '\0';
        loginSeg->requestReady = true;
        // «будим» сервер
        sem_post(loginSemServer);

        // Ждём ответ
        sem_wait(loginSemClient);
        if (!loginSeg->responseReady) {
            std::cerr << "No response from server" << std::endl;
            return 1;
        }
        std::string resp = loginSeg->buffer;
        loginSeg->responseReady = false;

        if (resp[0] == '0') {
            std::cout << "Login is already used or cannot login." << std::endl;
            return 0;
        } else if (resp[0] == '1') {
            std::cout << "Login successful!" << std::endl;
        } else {
            std::cout << "Unknown response from server: " << resp << std::endl;
            return 1;
        }
    }

    // 3) Подключаемся к своей памяти и семафорам
    std::string shmName = make_client_shm_name(pid);
    int fd = shm_open(shmName.c_str(), O_RDWR, 0666);
    if (fd < 0) {
        std::cerr << "Cannot open client shm, errno=" << errno << std::endl;
        return 1;
    }
    void* addr = mmap(nullptr, sizeof(ClientSegment), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (addr == MAP_FAILED) {
        std::cerr << "mmap client segment failed, errno=" << errno << std::endl;
        return 1;
    }
    ClientSegment* seg = reinterpret_cast<ClientSegment*>(addr);

    std::string semServerName = make_sem_server_name(pid);
    std::string semClientName = make_sem_client_name(pid);

    sem_t* semServer = sem_open(semServerName.c_str(), 0);
    if (semServer == SEM_FAILED) {
        std::cerr << "sem_open(semServer) failed, errno=" << errno << std::endl;
        return 1;
    }
    sem_t* semClient = sem_open(semClientName.c_str(), 0);
    if (semClient == SEM_FAILED) {
        std::cerr << "sem_open(semClient) failed, errno=" << errno << std::endl;
        return 1;
    }

    // 4) Запустим поток, который слушает ответы от сервера
    bool stopListener = false;
    std::thread listener([&]() {
        while (!stopListener) {
            // Ждём «ответ» от сервера
            if (sem_wait(semClient) == 0) {
                if (!seg->responseReady) {
                    continue;
                }
                std::string response = seg->buffer;
                seg->responseReady = false;

                // Выводим на экран
                if (response == "exit") {
                    std::cout << "Server closed your connection." << std::endl;
                    stopListener = true;
                    break;
                } else if (response.rfind("no client", 0) == 0) {
                    std::cout << "We didn't find this user." << std::endl;
                } else if (response.rfind("no group", 0) == 0) {
                    std::cout << "We didn't find this group." << std::endl;
                } else {
                    // Всё остальное просто выводим
                    std::cout << "Server: " << response << std::endl;
                }
            }
        }
        debug_log("Listener thread finished.");
    });

    // 5) Основной цикл ввода команд
    while (true) {
        std::cout << "Enter command: ";
        std::string line;
        if (!std::getline(std::cin, line)) {
            // EOF или ошибка
            break;
        }
        if (line.empty()) {
            continue;
        }

        // Отправляем серверу
        strncpy(seg->buffer, line.c_str(), MAX_CMD_LEN-1);
        seg->buffer[MAX_CMD_LEN-1] = '\0';
        seg->requestReady = true;
        sem_post(semServer);

        // Если это exit, то сами завершим после отправки
        if (line.rfind("exit", 0) == 0) {
            // подождём чуть-чуть, пока listener прочитает "exit"
            break;
        }
    }

    // Выходим
    stopListener = true;
    sem_post(semClient); // «расшевелим» listener, если он в ожидании
    listener.join();

    // Закрываем и размапим
    munmap(seg, sizeof(ClientSegment));
    munmap(loginSeg, sizeof(LoginSegment));

    return 0;
}
