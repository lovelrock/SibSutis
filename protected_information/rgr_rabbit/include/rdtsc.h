#ifndef RDTSC_H
#define RDTSC_H

#include <cstdint>

#ifdef __x86_64__
static inline uint64_t rdtsc() {
    unsigned int lo, hi;
    __asm__ volatile ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}
#else
#include <chrono>
static inline uint64_t rdtsc() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}
#endif

#endif
