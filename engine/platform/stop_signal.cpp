#include "engine/platform/stop_signal.hpp"
#include <stdexcept>

namespace vc {
namespace {
volatile std::sig_atomic_t stopped=0;
bool installed=false;
void stop(int signal) { stopped=1;(void)std::signal(signal,stop); }
}
StopSignal::StopSignal() {
    if(installed) throw std::logic_error("Stop signal scope is already installed");
    previousInterrupt_=std::signal(SIGINT,stop);
    if(previousInterrupt_==SIG_ERR) throw std::runtime_error("Cannot install interrupt handler");
    previousTerminate_=std::signal(SIGTERM,stop);
    if(previousTerminate_==SIG_ERR) { (void)std::signal(SIGINT,previousInterrupt_);throw std::runtime_error("Cannot install termination handler"); }
    stopped=0;installed=true;
}
StopSignal::~StopSignal() { (void)std::signal(SIGINT,previousInterrupt_);(void)std::signal(SIGTERM,previousTerminate_);installed=false; }
bool StopSignal::requested() const { return stopped!=0; }
void StopSignal::reset() { stopped=0; }
}
