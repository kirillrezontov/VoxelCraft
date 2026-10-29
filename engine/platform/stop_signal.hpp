#pragma once
#include <csignal>

namespace vc {
// Main-thread scope for a console application's SIGINT/SIGTERM handlers. The
// handler writes sig_atomic_t and reinstalls itself; persistence/logging stay in
// the loop. Reinstallation also supports C runtimes that reset the handler.
class StopSignal {
public:
    StopSignal();
    ~StopSignal();
    StopSignal(const StopSignal&)=delete;
    StopSignal& operator=(const StopSignal&)=delete;
    bool requested() const;
    void reset();
private:
    using Handler=void(*)(int);
    Handler previousInterrupt_{},previousTerminate_{};
};
}
