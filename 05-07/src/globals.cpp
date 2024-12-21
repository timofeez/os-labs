#include "common.h"
#include "manager.h"
#include <map>
#include <mutex>
#include <atomic>

// Определения глобальных переменных
std::map<int, NodeInfo> nodes;
std::atomic<int> heartbeat_interval(0);
std::atomic<bool> run_monitor(true);
std::mutex nodes_mutex;