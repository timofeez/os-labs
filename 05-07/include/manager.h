#pragma once

#include <string>
#include <chrono>
#include <unistd.h> // For pid_t

struct NodeInfo {
    int id;                     // Unique identifier for the node
    int parent;                 // Parent node ID
    std::string identity;       // Identity string for the node
    bool available;             // Availability status of the node
    std::chrono::steady_clock::time_point last_heartbeat; // Last heartbeat timestamp
    pid_t pid;                  // Process ID of the node
};

// Function declaration for the controller
void controller();