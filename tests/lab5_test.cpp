#include "gtest/gtest.h"
#include "tree.hpp"
#include <zmq.hpp>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

using namespace std;

TEST(ZeroMQIntegrationTest, CreateExecPingKillClient) {
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REQ);
    socket.connect("tcp://127.0.0.1:5050");

    string create_message = "create 1";
    zmq::message_t request(create_message.size());
    memcpy(request.data(), create_message.c_str(), create_message.size());
    socket.send(request, zmq::send_flags::none);

    zmq::message_t reply;
    socket.recv(reply);
    string response(static_cast<char*>(reply.data()), reply.size());
    EXPECT_EQ(response, "Ok");

    string exec_message = "exec 1 x 10";
    request.rebuild(exec_message.size());
    memcpy(request.data(), exec_message.c_str(), exec_message.size());
    socket.send(request, zmq::send_flags::none);
    socket.recv(reply);
    response = string(static_cast<char*>(reply.data()), reply.size());
    EXPECT_EQ(response, "Ok:1");

    string ping_message = "ping 1";
    request.rebuild(ping_message.size());
    memcpy(request.data(), ping_message.c_str(), ping_message.size());
    socket.send(request, zmq::send_flags::none);
    socket.recv(reply);
    response = string(static_cast<char*>(reply.data()), reply.size());
    EXPECT_EQ(response, "Ok:1");

    string kill_message = "kill 1";
    request.rebuild(kill_message.size());
    memcpy(request.data(), kill_message.c_str(), kill_message.size());
    socket.send(request, zmq::send_flags::none);
    socket.recv(reply);
    response = string(static_cast<char*>(reply.data()), reply.size());
    EXPECT_EQ(response, "Ok");
    this_thread::sleep_for(chrono::milliseconds(100));
}

TEST(ZeroMQIntegrationTest, MultipleClients) {
    zmq::context_t context(1);
    zmq::socket_t socket1(context, ZMQ_REQ);
    zmq::socket_t socket2(context, ZMQ_REQ);
    socket1.connect("tcp://127.0.0.1:5050");
    socket2.connect("tcp://127.0.0.1:5051");
    string message1 = "ping 1";
    string message2 = "ping 2";
    zmq::message_t request1(message1.size());
    zmq::message_t request2(message2.size());
    memcpy(request1.data(), message1.c_str(), message1.size());
    memcpy(request2.data(), message2.c_str(), message2.size());
    socket1.send(request1, zmq::send_flags::none);
    socket2.send(request2, zmq::send_flags::none);
    zmq::message_t reply1, reply2;
    socket1.recv(reply1);
    socket2.recv(reply2);
    string response1(static_cast<char*>(reply1.data()), reply1.size());
    string response2(static_cast<char*>(reply2.data()), reply2.size());
    EXPECT_EQ(response1, "Ok:1");
    EXPECT_EQ(response2, "Ok:2");
    this_thread::sleep_for(chrono::milliseconds(150));
}

TEST(ZeroMQIntegrationTest, HeartbeatCheck) {
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REQ);
    socket.connect("tcp://127.0.0.1:5050");

    string heartbeat_message = "heartbeat 500";
    zmq::message_t request(heartbeat_message.size());
    memcpy(request.data(), heartbeat_message.c_str(), heartbeat_message.size());
    socket.send(request, zmq::send_flags::none);

    zmq::message_t reply;
    socket.recv(reply);
    string response(static_cast<char*>(reply.data()), reply.size());
    EXPECT_EQ(response, "Available:1");

    this_thread::sleep_for(chrono::milliseconds(120));
}

TEST(TreeTest, CreateNode) {
    Tree tree;
    tree.push(1);
    vector<int> nodes = tree.get_nodes();
    EXPECT_EQ(nodes.size(), 1);
    EXPECT_EQ(nodes[0], 1);
    this_thread::sleep_for(chrono::milliseconds(80));
}

TEST(TreeTest, DeleteNode) {
    Tree tree;
    tree.push(1);
    tree.push(2);
    tree.kill(1);
    vector<int> nodes = tree.get_nodes();
    EXPECT_EQ(nodes.size(), 1);
    EXPECT_EQ(nodes[0], 2);
    this_thread::sleep_for(chrono::milliseconds(108));
}

TEST(TreeTest, GetNodes) {
    Tree tree;
    tree.push(1);
    tree.push(2);
    tree.push(3);
    vector<int> nodes = tree.get_nodes();
    EXPECT_EQ(nodes.size(), 3);
    EXPECT_EQ(nodes[0], 1);
    EXPECT_EQ(nodes[1], 2);
    EXPECT_EQ(nodes[2], 3);
    this_thread::sleep_for(chrono::milliseconds(111));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}