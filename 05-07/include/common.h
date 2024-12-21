#pragma once
#include <string>
#include <vector>
#include <sstream>

inline std::vector<std::string> split(const std::string &s) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;
    while (ss >> item) {
        result.push_back(item);
    }
    return result;
}
