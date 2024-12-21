#include "common.h"
#include "manager.h"
#include <zmq.hpp>
#include <iostream>
#include <string>
#include <map>
#include <set>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>



static std::map<int, NodeInfo> nodes;
static int next_port = 5557;
static std::atomic<int> heartbeat_interval(0);
static std::atomic<bool> run_monitor(true);
static std::mutex nodes_mutex;

void controller() {
    zmq::context_t context(1);
    zmq::socket_t frontend(context, ZMQ_ROUTER);
    frontend.bind("tcp://*:5560");

    std::thread monitor_thread([&]() {
        while (run_monitor) {
            if (heartbeat_interval > 0) {
                int timeout_ms = 4 * heartbeat_interval.load();
                auto now = std::chrono::steady_clock::now();
                std::lock_guard<std::mutex> lock(nodes_mutex);
                for (auto &kv : nodes) {
                    if (kv.second.id != -1) {
                        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(now - kv.second.last_heartbeat).count();
                        if (dur > timeout_ms && kv.second.available) {
                            kv.second.available = false;
                            std::cout << "Heartbit: node " << kv.second.id << " is unavailable now\n";
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });

    {
        NodeInfo managerNode;
        managerNode.id = -1;
        managerNode.parent = -1;
        managerNode.identity = "manager";
        managerNode.available = true;
        managerNode.last_heartbeat = std::chrono::steady_clock::now();
        managerNode.pid = getpid();
        nodes[-1] = managerNode;
    }

    zmq::pollitem_t items[] = {
        {static_cast<void*>(frontend), 0, ZMQ_POLLIN, 0},
        {nullptr, STDIN_FILENO, ZMQ_POLLIN, 0}
    };

    while (true) {
        zmq::poll(items, 2, std::chrono::milliseconds(100));

        if (items[1].revents & ZMQ_POLLIN) {
            std::string line;
            if (!std::getline(std::cin, line)) {
                break;
            }
            if (line.empty()) continue;

            auto parts = split(line);
            if (parts.empty()) continue;

            if (parts[0] == "create") {
                if (parts.size() < 2) {
                    std::cout << "Error: Wrong format\n";
                    continue;
                }
                int new_id = std::stoi(parts[1]);
                int parent_id = -1;
                if (parts.size() >= 3) {
                    parent_id = std::stoi(parts[2]);
                }

                {
                    std::lock_guard<std::mutex> lock(nodes_mutex);
                    if (nodes.find(new_id) != nodes.end()) {
                        std::cout << "Error: Already exists\n";
                        continue;
                    }
                    if (parent_id != -1 && nodes.find(parent_id) == nodes.end()) {
                        std::cout << "Error: Parent not found\n";
                        continue;
                    }

                    int port = next_port++;
                    std::string node_endpoint = "tcp://127.0.0.1:" + std::to_string(port);

                    pid_t pid = fork();
                    if (pid == 0) {
                        execl("./node", "node", node_endpoint.c_str(), "tcp://127.0.0.1:5560",
                              std::to_string(new_id).c_str(), std::to_string(parent_id).c_str(), (char*)NULL);
                        _exit(1);
                    } else if (pid < 0) {
                        std::cout << "Error: Can't fork\n";
                        continue;
                    } else {
                        NodeInfo ni;
                        ni.id = new_id;
                        ni.parent = parent_id;
                        ni.identity = "";
                        ni.available = false;
                        ni.last_heartbeat = std::chrono::steady_clock::now();
                        ni.pid = pid;
                        nodes[new_id] = ni;
                        std::cout << "Ok: " << pid << "\n";
                    }
                }

            } else if (parts[0] == "exec") {
                if (parts.size() < 3) {
                    std::cout << "Error:id: Wrong format\n";
                    continue;
                }
                int id = std::stoi(parts[1]);
                std::string name = parts[2];
                std::string value;
                if (parts.size() >= 4) {
                    value = parts[3];
                }

                std::lock_guard<std::mutex> lock(nodes_mutex);
                auto it = nodes.find(id);
                if (it == nodes.end()) {
                    std::cout << "Error:" << id << ": Not found\n";
                    continue;
                }
                if (!it->second.available || it->second.identity.empty()) {
                    std::cout << "Error:" << id << ": Node is unavailable\n";
                    continue;
                }

                std::string cmd = "EXEC " + name;
                if (!value.empty()) {
                    cmd += " " + value;
                }

                zmq::message_t identity(it->second.identity.size());
                memcpy(identity.data(), it->second.identity.data(), it->second.identity.size());
                zmq::message_t cmd_msg(cmd.size());
                memcpy(cmd_msg.data(), cmd.data(), cmd.size());

                frontend.send(identity, zmq::send_flags::sndmore);
                frontend.send(cmd_msg, zmq::send_flags::none);

            } else if (parts[0] == "heartbit") {
                if (parts.size() < 2) {
                    std::cout << "Error: Wrong format\n";
                    continue;
                }
                int time_ms = std::stoi(parts[1]);
                heartbeat_interval.store(time_ms);

                {
                    std::lock_guard<std::mutex> lock(nodes_mutex);
                    for (auto &kv : nodes) {
                        if (kv.second.id != -1 && !kv.second.identity.empty()) {
                            std::string cmd = "SET_HEARTBEAT " + std::to_string(time_ms);
                            zmq::message_t identity(kv.second.identity.size());
                            memcpy(identity.data(), kv.second.identity.data(), kv.second.identity.size());
                            zmq::message_t cmd_msg(cmd.size());
                            memcpy(cmd_msg.data(), cmd.c_str(), cmd.size());

                            frontend.send(identity, zmq::send_flags::sndmore);
                            frontend.send(cmd_msg, zmq::send_flags::none);
                        }
                    }
                }
                std::cout << "Ok\n";

            } else if (parts[0] == "ping") {
                if (parts.size() < 2) {
                    std::cout << "Error: Wrong format\n";
                    continue;
                }
                int id = std::stoi(parts[1]);
                std::string node_identity;
                {
                    std::lock_guard<std::mutex> lock(nodes_mutex);
                    auto it = nodes.find(id);
                    if (it == nodes.end() || it->second.identity.empty()) {
                        std::cout << "Ok: 0\n";
                        continue;
                    }
                    node_identity = it->second.identity;
                }

                {
                    std::string cmd = "PING";
                    zmq::message_t identity_msg(node_identity.size());
                    memcpy(identity_msg.data(), node_identity.data(), node_identity.size());
                    zmq::message_t cmd_msg(cmd.size());
                    memcpy(cmd_msg.data(), cmd.data(), cmd.size());

                    frontend.send(identity_msg, zmq::send_flags::sndmore);
                    frontend.send(cmd_msg, zmq::send_flags::none);
                }

                bool got_pong = false;
                auto start = std::chrono::steady_clock::now();
                while (true) {
                    zmq::pollitem_t poll_items[] = {
                        {static_cast<void*>(frontend), 0, ZMQ_POLLIN, 0}
                    };
                    zmq::poll(poll_items, 1, std::chrono::milliseconds(100));

                    if (poll_items[0].revents & ZMQ_POLLIN) {
                        zmq::message_t resp_identity_msg, resp_data_msg;
                        if (!frontend.recv(resp_identity_msg))
                            break;
                        if (!frontend.recv(resp_data_msg))
                            break;

                        std::string resp_identity(static_cast<char*>(resp_identity_msg.data()), resp_identity_msg.size());
                        std::string resp_data(static_cast<char*>(resp_data_msg.data()), resp_data_msg.size());
                        auto resp_parts = split(resp_data);

                        if (!resp_parts.empty() && resp_parts[0] == "PONG" && resp_parts.size() >= 2) {
                            int pong_id = std::stoi(resp_parts[1]);
                            if (pong_id == id && resp_identity == node_identity) {
                                got_pong = true;
                                break;
                            } 
                        }
                    }

                    auto now = std::chrono::steady_clock::now();
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() > 500) {
                        break;
                    }
                }

                if (got_pong) {
                    std::cout << "Ok: 1\n";
                    std::lock_guard<std::mutex> lock(nodes_mutex);
                    auto it = nodes.find(id);
                    if (it != nodes.end()) {
                        it->second.last_heartbeat = std::chrono::steady_clock::now();
                        it->second.available = true;
                    }
                } else {
                    std::cout << "Ok: 0\n";
                    std::lock_guard<std::mutex> lock(nodes_mutex);
                    auto it = nodes.find(id);
                    if (it != nodes.end()) {
                        it->second.available = false;
                    }
                }

            } else {
                std::cout << "Error: Unknown command\n";
            }
        }

        if (items[0].revents & ZMQ_POLLIN) {
            zmq::message_t identity_msg;
            zmq::message_t data_msg;
            if (!frontend.recv(identity_msg)) continue;
            if (!frontend.recv(data_msg)) continue;

            std::string identity(static_cast<char*>(identity_msg.data()), identity_msg.size());
            std::string data(static_cast<char*>(data_msg.data()), data_msg.size());

            auto parts = split(data);
            if (parts.empty()) continue;

            if (parts[0] == "INIT_OK") {
                if (parts.size() >= 4) {
                    int nid = std::stoi(parts[1]);
                    std::string ident = parts[3];
                    std::lock_guard<std::mutex> lock(nodes_mutex);
                    auto it = nodes.find(nid);
                    if (it != nodes.end()) {
                        it->second.identity = ident;
                        it->second.available = true;
                        it->second.last_heartbeat = std::chrono::steady_clock::now();
                    }
                }
            } else if (parts[0] == "HEARTBEAT") {
                if (parts.size() >= 2) {
                    int hid = std::stoi(parts[1]);
                    std::lock_guard<std::mutex> lock(nodes_mutex);
                    auto it = nodes.find(hid);
                    if (it != nodes.end()) {
                        it->second.last_heartbeat = std::chrono::steady_clock::now();
                        it->second.available = true;
                    }
                }
            } else if (parts[0] == "RESULT" || parts[0] == "RESULT_OK") {
                int found_id = -1;
                {
                    std::lock_guard<std::mutex> lock(nodes_mutex);
                    for (auto &kv : nodes) {
                        if (kv.second.identity == identity) {
                            found_id = kv.second.id;
                            break;
                        }
                    }
                }
                if (found_id != -1) {
                    if (parts[0] == "RESULT_OK") {
                        std::cout << "Ok:" << found_id << "\n";
                    } else {
                        std::cout << "Ok:" << found_id << ": ";
                        for (size_t i = 1; i < parts.size(); i++) {
                            std::cout << parts[i] << " ";
                        }
                        std::cout << "\n";
                    }
                }
            } else if (parts[0] == "PONG") {
                if (parts.size() >= 2) {
                    int hid = std::stoi(parts[1]);
                    std::lock_guard<std::mutex> lock(nodes_mutex);
                    auto it = nodes.find(hid);
                    if (it != nodes.end()) {
                        it->second.last_heartbeat = std::chrono::steady_clock::now();
                    }
                }
            }
        }
    }

    run_monitor = false;
    monitor_thread.join();
}

