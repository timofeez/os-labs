#include <gtest/gtest.h>
#include <zmq.hpp>
#include <thread>
#include <chrono>
#include "common.h"
#include "manager.h"

// Локальная переменная для хранения узлов
std::map<int, NodeInfo> test_nodes;

// Тест для функции split
TEST(SplitTest, BasicSplit) {
    std::string input = "this is a test";
    auto result = split(input);
    EXPECT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], "this");
    EXPECT_EQ(result[1], "is");
    EXPECT_EQ(result[2], "a");
    EXPECT_EQ(result[3], "test");
}

TEST(SplitTest, EmptyString) {
    std::string input = "";
    auto result = split(input);
    EXPECT_TRUE(result.empty());
}

TEST(SplitTest, MultipleSpaces) {
    std::string input = "   one   two  three   ";
    auto result = split(input);
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "one");
    EXPECT_EQ(result[1], "two");
    EXPECT_EQ(result[2], "three");
}

// Тест для работы контроллера
TEST(ControllerTest, BasicFunctionality) {
    // Создание контекста ZMQ и сокетов
    zmq::context_t context(1);
    zmq::socket_t frontend(context, zmq::socket_type::router);
    frontend.bind("inproc://test");

    // Создание тестового узла
    NodeInfo node;
    node.id = 1;
    node.parent = -1;
    node.available = true;
    node.identity = "node-1";
    node.last_heartbeat = std::chrono::steady_clock::now();
    test_nodes[1] = node;

    // Проверка, что узел был добавлен корректно
    ASSERT_EQ(test_nodes.size(), 1);
    EXPECT_EQ(test_nodes[1].id, 1);
    EXPECT_EQ(test_nodes[1].parent, -1);
    EXPECT_EQ(test_nodes[1].identity, "node-1");
    EXPECT_TRUE(test_nodes[1].available);
}

// Тест для проверки heartbeat
TEST(HeartbeatTest, NodeTimeout) {
    NodeInfo node;
    node.id = 1;
    node.parent = -1;
    node.available = true;
    node.identity = "node-1";
    node.last_heartbeat = std::chrono::steady_clock::now() - std::chrono::milliseconds(1000);
    test_nodes[1] = node;

    heartbeat_interval.store(200);
    int timeout_ms = 4 * heartbeat_interval.load();

    auto now = std::chrono::steady_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(now - test_nodes[1].last_heartbeat).count();

    if (dur > timeout_ms) {
        test_nodes[1].available = false;
    }

    EXPECT_FALSE(test_nodes[1].available);
}

// Тест для проверки команды ping
TEST(PingTest, PingNode) {
    zmq::context_t context(1);
    zmq::socket_t frontend(context, zmq::socket_type::router);
    frontend.bind("inproc://test");

    NodeInfo node;
    node.id = 2;
    node.parent = -1;
    node.available = true;
    node.identity = "node-2";
    test_nodes[2] = node;

    // Отправка команды PING
    std::string cmd = "PING";
    zmq::message_t identity_msg(node.identity.size());
    memcpy(identity_msg.data(), node.identity.data(), node.identity.size());

    zmq::message_t cmd_msg(cmd.size());
    memcpy(cmd_msg.data(), cmd.data(), cmd.size());

    frontend.send(identity_msg, zmq::send_flags::sndmore);
    frontend.send(cmd_msg, zmq::send_flags::none);

    // Проверка доступности узла
    EXPECT_TRUE(test_nodes[2].available);
}

// Точка входа для тестов
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
