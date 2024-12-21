#ifndef MANAGER_H
#define MANAGER_H

#include <atomic>
#include <map>
#include <string>
#include <chrono>
#include <mutex>
#include <unistd.h>
#include <zmq.hpp>

struct NodeInfo {
    int id;
    int parent;
    bool available;
    std::string identity;
    pid_t pid;
    std::chrono::steady_clock::time_point last_heartbeat;
};

void controller();
void process_command(const std::string& line, std::map<int, NodeInfo>& nodes, zmq::socket_t& frontend);

extern std::atomic<int> heartbeat_interval;

#endif
