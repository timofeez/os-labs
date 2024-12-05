#ifndef UTILS_H
#define UTILS_H

static inline int next_power_of_two(int n) {
    int k = 1;
    while (k < n) {
        k <<= 1;
    }
    return k;
}

#endif
