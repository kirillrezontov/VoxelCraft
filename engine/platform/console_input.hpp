#pragma once
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace vc {
struct ConsoleLine { std::string text;bool tooLong{}; };
class ConsoleLines {
public:
    explicit ConsoleLines(std::size_t maximum=1024):maximum_(maximum) {}
    void feed(std::string_view bytes);
    void finish();
    std::vector<ConsoleLine> take(std::size_t budget=8);
private:
    void line();
    std::size_t maximum_;
    std::string buffer_;
    std::deque<ConsoleLine> ready_;
    bool overflow_{};
};
// Polls native stdin without a reader thread. EOF/unavailable stdin disables
// polling. No game/SDL/Vulkan ownership enters this platform service.
class ConsoleInput {
public:
    ConsoleInput();
    ~ConsoleInput();
    ConsoleInput(const ConsoleInput&)=delete;
    ConsoleInput& operator=(const ConsoleInput&)=delete;
    std::vector<ConsoleLine> poll();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
