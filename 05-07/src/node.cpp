#include "common.h"
#include <zmq.hpp>
#include <map>
#include <string>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "Usage: node <self_endpoint> <manager_endpoint> <id> <parent_id>\n";
        return 1;
    }

    std::string self_endpoint = argv[1];
    std::string manager_endpoint = argv[2];
    int node_id = std::stoi(argv[3]);
    int parent_id = std::stoi(argv[4]);

    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_DEALER);
    std::string ident_str = "node-" + std::to_string(node_id);
    socket.setsockopt(ZMQ_IDENTITY, ident_str.c_str(), ident_str.size());
    socket.connect(manager_endpoint.c_str());

    {
        std::stringstream ss;
        ss << "INIT_OK " << node_id << " " << parent_id << " " << ident_str;
        std::string msg = ss.str();
        zmq::message_t m(msg.size());
        memcpy(m.data(), msg.data(), msg.size());
        socket.send(m, zmq::send_flags::none);
    }

    std::map<std::string,int> local_map;
    std::atomic<bool> running(true);
    std::atomic<int> hb_interval(0);

std::thread heartbeat_thread([&]() {
    while (running) {
        int interval = hb_interval.load();
        if (interval > 0) {
            std::string hb = "HEARTBEAT " + std::to_string(node_id);
            zmq::message_t hbmsg(hb.size());
            memcpy(hbmsg.data(), hb.c_str(), hb.size());
            socket.send(hbmsg, zmq::send_flags::none);
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
});


    zmq::pollitem_t items[] = {
        {static_cast<void*>(socket), 0, ZMQ_POLLIN, 0}
    };

    while (running) {
        zmq::poll(items, 1, std::chrono::milliseconds(100));
        if (items[0].revents & ZMQ_POLLIN) {
            std::vector<std::string> frames;
            while (true) {
                zmq::message_t message;
                auto res = socket.recv(message, zmq::recv_flags::none);
                if (!res.has_value()) break;

                frames.emplace_back(static_cast<char*>(message.data()), message.size());

                int more = 0;
                size_t more_size = sizeof(more);
                socket.getsockopt(ZMQ_RCVMORE, &more, &more_size);
                if (!more) {
                    break;
                }
            }

            if (frames.empty()) continue;

            std::string command_frame;
            if (frames.size() == 1) {
                command_frame = frames[0];
            } else {
                command_frame = frames.back();
            }

            auto parts = split(command_frame);
            if (parts.empty()) continue;

            if (parts[0] == "EXEC") {
                if (parts.size() < 2) {
                    std::string reply = "RESULT Error: Wrong EXEC format";
                    zmq::message_t r(reply.size());
                    memcpy(r.data(), reply.c_str(), reply.size());
                    socket.send(r, zmq::send_flags::none);
                    continue;
                }

                std::string name = parts[1];
                if (parts.size() == 2) {
                    // Чтение
                    auto it = local_map.find(name);
                    if (it == local_map.end()) {
                        std::string reply = "RESULT '" + name + "' not found";
                        zmq::message_t r(reply.size());
                        memcpy(r.data(), reply.c_str(), reply.size());
                        socket.send(r, zmq::send_flags::none);
                    } else {
                        std::string reply = "RESULT " + std::to_string(it->second);
                        zmq::message_t r(reply.size());
                        memcpy(r.data(), reply.c_str(), reply.size());
                        socket.send(r, zmq::send_flags::none);
                    }
                } else {
                    // Запись
                    std::string value_str = parts[2];
                    int value = std::stoi(value_str);
                    local_map[name] = value;
                    std::string reply = "RESULT_OK";
                    zmq::message_t r(reply.size());
                    memcpy(r.data(), reply.c_str(), reply.size());
                    socket.send(r, zmq::send_flags::none);
                }
            } else if (parts[0] == "SET_HEARTBEAT") {
                if (parts.size() >= 2) {
                    hb_interval = std::stoi(parts[1]);
                }
            } else if (parts[0] == "PING") {
                std::string reply = "PONG " + std::to_string(node_id);
                zmq::message_t r(reply.size());
                memcpy(r.data(), reply.c_str(), reply.size());
                socket.send(r, zmq::send_flags::none);
            } else {
                std::string reply = "RESULT Error: Unknown command";
                zmq::message_t r(reply.size());
                memcpy(r.data(), reply.c_str(), reply.size());
                socket.send(r, zmq::send_flags::none);
            }
        }
    }

    running = false;
    heartbeat_thread.join();

    return 0;
}