#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace vc::profile {
enum class Stage : unsigned {
    poll, dispatch, events, runtime, bindings, rebuild, style, layout, animation,
    drawList, comparison, software, allocation, upload, synchronization, acquire,
    recording, submit, present, wholeFrame, count
};
constexpr auto stageCount=static_cast<unsigned>(Stage::count);
struct Sample {
    std::array<std::uint64_t,stageCount> nanoseconds{},calls{};
    std::uint64_t uploadBytes{},queueIdleCalls{},deviceIdleCalls{};
    double gpuMilliseconds{};
    bool gpuTimingAvailable{};
};
// UI-thread profiler. Disabled by default; VC_UI_PROFILE=/path/raw.jsonl enables
// buffered raw samples in normal gameplay. Tests can enable it without a file.
bool enabled();
bool active();
void enable(bool value);
void record(Stage,std::uint64_t nanoseconds);
void uploadBytes(std::uint64_t);
void queueIdle();
void deviceIdle();
void gpuTime(double milliseconds,bool available);
const Sample& lastFrame();
void writeSample(std::ostream&,const Sample&);
class Scope {
    Stage stage_;
    bool active_;
    std::chrono::steady_clock::time_point begin_;
public:
    explicit Scope(Stage stage);
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
    ~Scope();
};
class Frame {
    bool enabled_;
    std::uint64_t poll_;
    std::chrono::steady_clock::time_point begin_;
public:
    explicit Frame(std::uint64_t pollNanoseconds=0);
    Frame(const Frame&)=delete;
    Frame& operator=(const Frame&)=delete;
    ~Frame();
};
}
