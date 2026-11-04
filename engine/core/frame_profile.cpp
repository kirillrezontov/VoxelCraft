#include "engine/core/frame_profile.hpp"
#include <cstdlib>
#include <fstream>
#include <stdexcept>

namespace vc::profile {
namespace {
constexpr std::array names{"poll","dispatch","events","runtime","bindings","rebuild","style","layout","animation",
    "draw_list","comparison","software","allocation","upload","synchronization","acquire","recording","submit","present","whole_frame"};
static_assert(names.size()==stageCount);
struct State {
    bool enabled{},active{};
    Sample current,last;
    std::ofstream output;
    State() {
        if(const auto* path=std::getenv("VC_UI_PROFILE");path&&*path) {
            output.open(path);
            if(!output) throw std::runtime_error("Cannot open VC_UI_PROFILE output");
            enabled=true;
        }
    }
};
State& state() { static thread_local State value;return value; }
std::uint64_t elapsed(std::chrono::steady_clock::time_point begin) {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now()-begin).count());
}
}
bool enabled() { return state().enabled; }
bool active() { return state().active; }
void enable(bool value) { state().enabled=value; }
void record(Stage stage,std::uint64_t ns) {
    auto& s=state();if(!s.active) return;
    const auto i=static_cast<unsigned>(stage);s.current.nanoseconds[i]+=ns;++s.current.calls[i];
}
void uploadBytes(std::uint64_t bytes) { if(active()) state().current.uploadBytes+=bytes; }
void queueIdle() { if(active()) ++state().current.queueIdleCalls; }
void deviceIdle() { if(active()) ++state().current.deviceIdleCalls; }
void gpuTime(double ms,bool available) {
    if(active()) { state().current.gpuMilliseconds=ms;state().current.gpuTimingAvailable=available; }
}
const Sample& lastFrame() { return state().last; }
void writeSample(std::ostream& out,const Sample& sample) {
    out<<"{\"cpu_ns\":{";
    for(unsigned i=0;i<stageCount;++i) out<<(i?",":"")<<'"'<<names[i]<<"\":"<<sample.nanoseconds[i];
    out<<"},\"calls\":{";
    for(unsigned i=0;i<stageCount;++i) out<<(i?",":"")<<'"'<<names[i]<<"\":"<<sample.calls[i];
    out<<"},\"upload_bytes\":"<<sample.uploadBytes<<",\"queue_idle_calls\":"<<sample.queueIdleCalls
       <<",\"device_idle_calls\":"<<sample.deviceIdleCalls<<",\"gpu_ms\":";
    if(sample.gpuTimingAvailable) out<<sample.gpuMilliseconds;else out<<"null";
    out<<'}';
}
Scope::Scope(Stage stage):stage_(stage),active_(active()) { if(active_) begin_=std::chrono::steady_clock::now(); }
Scope::~Scope() { if(active_) record(stage_,elapsed(begin_)); }
Frame::Frame(std::uint64_t poll):enabled_(enabled()),poll_(poll) {
    if(enabled_) {
        auto& s=state();s.current={};s.active=true;begin_=std::chrono::steady_clock::now();
        record(Stage::poll,poll);
    }
}
Frame::~Frame() {
    if(!enabled_) return;
    auto& s=state();record(Stage::wholeFrame,elapsed(begin_)+poll_);s.active=false;s.last=s.current;
    if(s.output.is_open()) { writeSample(s.output,s.last);s.output<<'\n'; }
}
}
