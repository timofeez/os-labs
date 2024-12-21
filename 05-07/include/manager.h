#ifndef MANAGER_H
#define MANAGER_H

#include <atomic>
#include <map>
#include <string>
#include <chrono>
#include <mutex>
#include <unistd.h> // для pid_t

// Объявляем heartbeat_interval, чтобы использовать его снаружи

struct NodeInfo {
    int id;
    int parent;
    bool available;
    std::string identity;
    pid_t pid;
    std::chrono::steady_clock::time_point last_heartbeat;
};

void controller();

#endif // MANAGER_H
