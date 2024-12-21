#include <gtest/gtest.h>
#include <zmq.hpp>
#include <thread>
#include <chrono>
#include <iostream>
#include <string>
#include <map>
#include <atomic>
#include <mutex>
#include <unistd.h>
#include <signal.h>
#include "manager.h"
#include "common.h"
// Подключаем код manager.cpp и node.cpp
// Предполагается, что код manager.cpp и node.cpp включен в проект
// и доступен через заголовочные файлы или прямое включение.
