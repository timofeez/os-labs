#include "common.h"
#include "manager.h"
#include <sstream>
#include <iostream>

// Теперь heartbeat_interval доступен через manager.h

std::vector<std::string> split(const std::string &s) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;
    while (ss >> item) {
        result.push_back(item);
    }
    return result;
}

void process_command(const std::string &line, std::map<int, NodeInfo> &nodes, zmq::socket_t &frontend) {
    std::cout << "[DEBUG] Processing command: " << line << std::endl;

    auto parts = split(line);
    if (parts.empty()) return;

    if (parts[0] == "create") {
        // Обработка команды create
    } else if (parts[0] == "heartbit") {
        if (parts.size() < 2) {
            std::cout << "Error: Wrong format\n";
            return;
        }
        int interval = std::stoi(parts[1]);
        if (interval <= 0) {
            std::cout << "Error: Interval must be positive\n";
            return;
        }

        std::cout << "[DEBUG] Setting heartbeat interval to " << interval << " ms\n";
        heartbeat_interval.store(interval);

        // Отправляем SET_HEARTBEAT всем узлам
        for (auto &kv : nodes) {
            if (kv.second.id != -1 && !kv.second.identity.empty()) {
                std::string command = "SET_HEARTBEAT " + std::to_string(interval);
                zmq::message_t identity_msg(kv.second.identity.size());
                memcpy(identity_msg.data(), kv.second.identity.data(), kv.second.identity.size());

                zmq::message_t cmd_msg(command.size());
                memcpy(cmd_msg.data(), command.data(), command.size());

                frontend.send(identity_msg, zmq::send_flags::sndmore);
                frontend.send(cmd_msg, zmq::send_flags::none);
            }
        }
        std::cout << "Ok\n";
    }
}

