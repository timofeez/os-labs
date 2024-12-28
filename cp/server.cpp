#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <thread>
#include <cstring>
#include <fcntl.h>      // O_CREAT, O_RDWR
#include <sys/mman.h>   // shm_open, mmap, etc
#include <sys/stat.h>   // fchmod
#include <semaphore.h>
#include <unistd.h>     // ftruncate, close
#include <errno.h>
#include <sstream>
#include <algorithm>
#include <chrono>

// --------------------------------------------------
// Константы и глобальные объекты
// --------------------------------------------------

// Это «канал» для начального логина.
static const char* LOGIN_SHM_NAME         = "/mychat_login_shm";
static const char* LOGIN_SEM_SERVER_NAME  = "/mychat_login_sem_server";
static const char* LOGIN_SEM_CLIENT_NAME  = "/mychat_login_sem_client";

// Максимальный размер строки-команды для обмена
static const size_t MAX_CMD_LEN = 512;

// Структура для сегмента "логин"
struct LoginSegment {
    bool  requestReady;       // Клиент установит true, сервер после чтения сбросит
    bool  responseReady;      // Сервер установит true, клиент сбросит
    char  buffer[MAX_CMD_LEN];// Команда / ответ
};

// Структура для канала отдельного клиента
struct ClientSegment {
    bool  requestReady;       // Клиент -> сервер
    bool  responseReady;      // Сервер -> клиент
    char  buffer[MAX_CMD_LEN];// Текст команды / ответа
};

// --------------------------------------------------
// Глобальные структуры данных (хранятся на сервере)
// --------------------------------------------------
std::map<std::string, bool> logged_in;                  // nickname -> isLogged
std::map<std::string, std::vector<std::string>> groups; // groupName -> list_of_nicknames

// Для истории (при желании можно хранить любой формат)
std::map<std::string, std::pair<std::string, std::string>> history_of_messages; 
//  nickname -> { lastRecipientOrGroup, messageHistory }

// Связка nickname -> pid, и обратно pid -> nickname
std::map<std::string, pid_t> nickname_to_pid;
std::map<pid_t, std::string> pid_to_nickname;

// -------------------------------------
// Доп. структура, чтобы сохранить всё,
// что нужно для отправки клиенту
// -------------------------------------
struct ClientInfo {
    pid_t pid;
    std::string nickname;
    ClientSegment* seg;
    sem_t* semServer;
    sem_t* semClient;
};

std::map<pid_t, ClientInfo> g_clients; // pid -> ClientInfo

// --------------------------------------------------
// Функции для отладочного вывода
// --------------------------------------------------
static void debug_log(const std::string& msg) {
    std::cerr << "[SERVER DEBUG] " << msg << std::endl;
}

// --------------------------------------------------
// Утилиты для открытия/закрытия/создания памяти и семафоров
// --------------------------------------------------

/**
 * Генерируем имя для памяти, связанной с клиентом
 */
static std::string make_client_shm_name(pid_t pid) {
    return "/mychatshm_" + std::to_string(pid);
}

/**
 * Генерируем имена для семафоров сервера/клиента
 */
static std::string make_sem_server_name(pid_t pid) {
    return "/sem_server_" + std::to_string(pid);
}
static std::string make_sem_client_name(pid_t pid) {
    return "/sem_client_" + std::to_string(pid);
}

/**
 * Создаём (или открываем) разделяемую память для клиента
 */
ClientSegment* create_client_segment(pid_t pid) {
    std::string shmName = make_client_shm_name(pid);

    // Создаём сегмент
    shm_unlink(shmName.c_str()); // на случай, если остался от предыдущего запуска
    int fd = shm_open(shmName.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        std::cerr << "shm_open failed for " << shmName << ", errno=" << errno << std::endl;
        return nullptr;
    }
    // Ресайзим под нашу структуру
    if (ftruncate(fd, sizeof(ClientSegment)) == -1) {
        std::cerr << "ftruncate failed, errno=" << errno << std::endl;
        close(fd);
        return nullptr;
    }
    // Мапим
    void* addr = mmap(nullptr, sizeof(ClientSegment), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (addr == MAP_FAILED) {
        std::cerr << "mmap failed, errno=" << errno << std::endl;
        return nullptr;
    }

    // Инициализация полей
    ClientSegment* seg = reinterpret_cast<ClientSegment*>(addr);
    seg->requestReady  = false;
    seg->responseReady = false;
    seg->buffer[0]     = '\0';
    return seg;
}

/**
 * Создаём именованные семафоры для канала клиента
 */
bool create_client_semaphores(pid_t pid, sem_t*& semServer, sem_t*& semClient) {
    std::string semServerName = make_sem_server_name(pid);
    std::string semClientName = make_sem_client_name(pid);

    // На случай, если семафоры остались от предыдущего запуска, их нужно удалить
    sem_unlink(semServerName.c_str());
    sem_unlink(semClientName.c_str());

    semServer = sem_open(semServerName.c_str(), O_CREAT, 0666, 0); 
    if (semServer == SEM_FAILED) {
        std::cerr << "sem_open(server) failed for pid=" << pid << ", errno=" << errno << std::endl;
        return false;
    }
    semClient = sem_open(semClientName.c_str(), O_CREAT, 0666, 0);
    if (semClient == SEM_FAILED) {
        std::cerr << "sem_open(client) failed for pid=" << pid << ", errno=" << errno << std::endl;
        sem_close(semServer);
        sem_unlink(semServerName.c_str());
        return false;
    }
    return true;
}

/**
 * Утилита отправки сообщения (ответа) конкретному клиенту
 * - seg – это сегмент памяти получателя
 * - semClient – семафор, «будим» получателя
 * - msg – строка, которую кладём в buffer
 */
static void send_response(ClientSegment* seg, sem_t* semClient, const std::string& msg) {
    // Записываем ответ
    strncpy(seg->buffer, msg.c_str(), MAX_CMD_LEN - 1);
    seg->buffer[MAX_CMD_LEN - 1] = '\0';
    seg->responseReady = true;
    // «будим» клиента
    sem_post(semClient);
}

/**
 * Сохранение истории для личного сообщения
 */
static void history_save(const std::string& login_sender, 
                         const std::string& login_accepter, 
                         const std::string& message) 
{
    std::cout << message << " to " << login_accepter << std::endl;
    history_of_messages[login_sender].first  = login_accepter;
    history_of_messages[login_sender].second += ("\n" + message);
}

/**
 * Сохранение истории для группового сообщения
 */
static void history_group_save(const std::string& login_sender, 
                               const std::string& group, 
                               const std::string& message) 
{
    std::cout << message << " in " << group << " group" << std::endl;
    history_of_messages[login_sender].first  = group;
    history_of_messages[login_sender].second += ("\n" + message);
}

/**
 * Реальная доставка личного сообщения получателю
 * (чтобы получатель увидел на своём клиенте).
 */
static void deliver_message_to_user(const std::string& recipient, const std::string& text) {
    // Узнаем pid
    if (nickname_to_pid.find(recipient) == nickname_to_pid.end()) {
        return; // не знаем такого
    }
    pid_t rec_pid = nickname_to_pid[recipient];
    if (g_clients.find(rec_pid) == g_clients.end()) {
        return; // нет такого
    }
    ClientInfo &ci = g_clients[rec_pid];
    // отправим в его сегмент
    send_response(ci.seg, ci.semClient, text);
}

/**
 * Доставка в группу: шлём всем участникам группы
 */
static void deliver_message_to_group(const std::string& groupName, const std::string& text) {
    if (groups.find(groupName) == groups.end()) {
        return;
    }
    auto &vec = groups[groupName];
    for (auto &user : vec) {
        deliver_message_to_user(user, text);
    }
}

/**
 * Обработка команды от клиента
 * cmdLine – это всё, что клиент записал: "send user message...", "addgroup group", etc
 */
static void process_command(pid_t pid, ClientSegment* seg, sem_t* semClient, const std::string& cmdLine) {
    debug_log("Processing command from pid=" + std::to_string(pid) + ": [" + cmdLine + "]");

    // Парсим команду
    std::istringstream iss(cmdLine);
    std::string command;
    iss >> command;

    // Узнаем логин отправителя
    std::string sender = pid_to_nickname[pid];

    if (command == "send") {
        std::string recipient;
        iss >> recipient;
        // Считаем весь оставшийся текст как сообщение
        std::string message;
        std::getline(iss, message);
        if (!message.empty() && message[0] == ' ') {
            message.erase(message.begin());
        }

        if (recipient.empty()) {
            // Нет получателя
            send_response(seg, semClient, "no client");
            return;
        }
        // Проверяем, есть ли такой логин
        if (logged_in.find(recipient) != logged_in.end() && logged_in[recipient]) {
            // Сохраняем в истории
            history_save(sender, recipient, message);
            // Отправляем реальному получателю "Message from <sender>: <message>"
            std::string finalText = "Message from " + sender + ": " + message;
            deliver_message_to_user(recipient, finalText);

            // А отправителю скажем, что доставка успешно
            send_response(seg, semClient, "Message delivered to " + recipient);
        } else {
            send_response(seg, semClient, "no client");
        }

    } else if (command == "gs") {
        // group send
        std::string groupName;
        iss >> groupName;
        std::string message;
        std::getline(iss, message);
        if (!message.empty() && message[0] == ' ') {
            message.erase(message.begin());
        }

        if (groupName.empty()) {
            send_response(seg, semClient, "no group");
            return;
        }
        auto it = groups.find(groupName);
        if (it != groups.end() && !it->second.empty()) {
            history_group_save(sender, groupName, message);
            // Рассылаем сообщение всем участникам группы
            std::string finalText = "Group message from " + sender + " to " + groupName + ": " + message;
            deliver_message_to_group(groupName, finalText);

            // Ответ отправителю
            send_response(seg, semClient, "Message delivered to group " + groupName);
        } else {
            send_response(seg, semClient, "no group");
        }

    } else if (command == "addgroup") {
        std::string groupName;
        iss >> groupName;
        if (groupName.empty()) {
            send_response(seg, semClient, "Incorrect group name");
            return;
        }
        auto &vec = groups[groupName];
        // Проверяем, нет ли уже в группе данного пользователя
        if (std::find(vec.begin(), vec.end(), sender) != vec.end()) {
            // уже есть
            send_response(seg, semClient, "you are already a member " + groupName + " group");
        } else {
            vec.push_back(sender);
            std::cout << "User " << sender << " joined to " << groupName << " group" << std::endl;
            std::cout << "Users in " << groupName << ": ";
            for (auto &u : vec) {
                std::cout << u << " ";
            }
            std::cout << std::endl;
            send_response(seg, semClient, "group - " + groupName + ", added to your group list");
        }

    } else if (command == "leavegroup") {
        std::string groupName;
        iss >> groupName;
        if (groupName.empty()) {
            send_response(seg, semClient, "no group");
            return;
        }
        auto it = groups.find(groupName);
        if (it == groups.end()) {
            send_response(seg, semClient, "no group");
        } else {
            auto &vec = it->second;
            auto pos = std::find(vec.begin(), vec.end(), sender);
            if (pos != vec.end()) {
                vec.erase(pos);
                send_response(seg, semClient, "leaved " + groupName);
            } else {
                send_response(seg, semClient, "you are not in group " + groupName);
            }
        }

    } else if (command == "grouplist") {
        std::string answer;
        for (auto &g : groups) {
            for (auto &user : g.second) {
                if (user == sender) {
                    answer += (g.first + " ");
                }
            }
        }
        if (answer.empty()) {
            send_response(seg, semClient, "you have no groups");
        } else {
            send_response(seg, semClient, "your groups: " + answer);
        }

    } else if (command == "exit") {
        logged_in[sender] = false;
        send_response(seg, semClient, "exit");
        debug_log("User " + sender + " (pid="+std::to_string(pid)+") has exited.");

    } else {
        // неизвестная команда
        send_response(seg, semClient, "Unknown command: " + command);
    }
}

// --------------------------------------------------
// Основная логика
// --------------------------------------------------
int main()
{
    std::cout << "Server started. Waiting for clients...\n";

    // 1) Создаём/очищаем сегмент для логина
    shm_unlink(LOGIN_SHM_NAME); 
    int loginFd = shm_open(LOGIN_SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (loginFd < 0) {
        std::cerr << "shm_open failed for login, errno=" << errno << std::endl;
        return 1;
    }
    if (ftruncate(loginFd, sizeof(LoginSegment)) == -1) {
        std::cerr << "ftruncate failed for login, errno=" << errno << std::endl;
        close(loginFd);
        return 1;
    }
    void* loginAddr = mmap(nullptr, sizeof(LoginSegment), PROT_READ|PROT_WRITE, MAP_SHARED, loginFd, 0);
    close(loginFd);
    if (loginAddr == MAP_FAILED) {
        std::cerr << "mmap for login failed, errno=" << errno << std::endl;
        return 1;
    }
    LoginSegment* loginSeg = reinterpret_cast<LoginSegment*>(loginAddr);
    loginSeg->requestReady  = false;
    loginSeg->responseReady = false;
    loginSeg->buffer[0]     = '\0';

    // 2) Создаём/очищаем семафоры для логина
    sem_unlink(LOGIN_SEM_SERVER_NAME);
    sem_unlink(LOGIN_SEM_CLIENT_NAME);
    sem_t* loginSemServer = sem_open(LOGIN_SEM_SERVER_NAME, O_CREAT, 0666, 0);
    if (loginSemServer == SEM_FAILED) {
        std::cerr << "sem_open(loginSemServer) failed, errno=" << errno << std::endl;
        return 1;
    }
    sem_t* loginSemClient = sem_open(LOGIN_SEM_CLIENT_NAME, O_CREAT, 0666, 0);
    if (loginSemClient == SEM_FAILED) {
        std::cerr << "sem_open(loginSemClient) failed, errno=" << errno << std::endl;
        return 1;
    }

    // 3) В отдельном потоке «слушатель» логинов
    std::thread loginListener([&]() {
        while (true) {
            sem_wait(loginSemServer); 
            if (!loginSeg->requestReady) {
                continue;
            }
            std::string req = loginSeg->buffer;
            loginSeg->requestReady = false;

            // парсим "login <pid> <nickname>"
            std::istringstream iss(req);
            std::string cmd;
            iss >> cmd;
            if (cmd != "login") {
                // не тот запрос
                strncpy(loginSeg->buffer, "0", MAX_CMD_LEN-1);
                loginSeg->responseReady = true;
                sem_post(loginSemClient);
                continue;
            }
            pid_t clientPid = 0;
            iss >> clientPid;
            std::string nickname;
            iss >> nickname;

            debug_log("Got login request from pid=" + std::to_string(clientPid) + " nickname=" + nickname);

            // проверяем, не залогинен ли уже
            if (logged_in.find(nickname) != logged_in.end() && logged_in[nickname]) {
                // уже залогинен
                strncpy(loginSeg->buffer, "0", MAX_CMD_LEN-1); // сигнал «логин занят»
                loginSeg->buffer[MAX_CMD_LEN-1] = '\0';
                loginSeg->responseReady = true;
                sem_post(loginSemClient);
                continue;
            }

            // Ок, «регистрируем»
            logged_in[nickname] = true;
            nickname_to_pid[nickname] = clientPid;
            pid_to_nickname[clientPid] = nickname;

            // Создаём память и семафоры для этого клиента
            ClientSegment* cseg = create_client_segment(clientPid);
            if (!cseg) {
                strncpy(loginSeg->buffer, "0", MAX_CMD_LEN-1);
                loginSeg->responseReady = true;
                sem_post(loginSemClient);
                continue;
            }
            sem_t* semServer = nullptr;
            sem_t* semClient = nullptr;
            if (!create_client_semaphores(clientPid, semServer, semClient)) {
                munmap(cseg, sizeof(ClientSegment));
                strncpy(loginSeg->buffer, "0", MAX_CMD_LEN-1);
                loginSeg->responseReady = true;
                sem_post(loginSemClient);
                continue;
            }

            // Успешно: говорим клиенту "1"
            strncpy(loginSeg->buffer, "1", MAX_CMD_LEN-1);
            loginSeg->buffer[MAX_CMD_LEN-1] = '\0';
            loginSeg->responseReady = true;
            sem_post(loginSemClient);

            std::cout << "User " << nickname << " logged in with pid " << clientPid << std::endl;

            // Заполним нашу глобальную map
            ClientInfo ci;
            ci.pid = clientPid;
            ci.nickname = nickname;
            ci.seg = cseg;
            ci.semServer = semServer;
            ci.semClient = semClient;
            g_clients[clientPid] = ci;

            // Теперь в отдельном потоке слушаем конкретно этого клиента
            std::thread clientWorker([=]() {
                debug_log("Start thread for pid=" + std::to_string(clientPid));
                while (true) {
                    sem_wait(semServer); 
                    if (!cseg->requestReady) {
                        continue;
                    }
                    std::string cmdLine = cseg->buffer;
                    cseg->requestReady = false;

                    // Если команда начинается на "exit" – по условию, 
                    // мы отправим "exit" клиенту и завершим
                    if (cmdLine.rfind("exit", 0) == 0) {
                        // обрабатываем exit (process_command сделает send_response)
                        process_command(clientPid, cseg, semClient, cmdLine);
                        debug_log("Stop thread for pid=" + std::to_string(clientPid));
                        // Удалим из g_clients
                        g_clients.erase(clientPid);
                        // Закрываем ресурсы
                        munmap(cseg, sizeof(ClientSegment));
                        sem_close(semServer);
                        sem_close(semClient);
                        // sem_unlink(...) — при необходимости
                        break;
                    }

                    // обрабатываем команду
                    process_command(clientPid, cseg, semClient, cmdLine);
                }
            });
            clientWorker.detach();
        }
    });
    loginListener.detach();

    // Сервер не завершается
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
