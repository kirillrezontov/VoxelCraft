#pragma once
#include <chrono>
#include <cstdint>

namespace vc {
// Value snapshot; worker owners guard updates/reads with their existing mutex.
struct WorkMetrics {
    std::uint64_t completed{};
    double lastMilliseconds{},totalMilliseconds{};
    void record(std::chrono::steady_clock::time_point start) {
        lastMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        totalMilliseconds+=lastMilliseconds;++completed;
    }
};
}
