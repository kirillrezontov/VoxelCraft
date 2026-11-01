#pragma once
#include <coroutine>
#include <exception>
#include <stdexcept>
#include <utility>

namespace vc::client {
enum class SessionExit { Menu, Quit, Completed };
// One allocation per session; ordinary locals own all state across frame yields.
// Single main-thread resume. Destroying a suspended frame unwinds/join its owners.
class GameSession {
public:
    struct promise_type;
    using Handle = std::coroutine_handle<promise_type>;
    struct promise_type {
        SessionExit exit{SessionExit::Menu};
        std::exception_ptr failure;
        GameSession get_return_object() { return GameSession(Handle::from_promise(*this)); }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_value(SessionExit value) noexcept { exit=value; }
        void unhandled_exception() noexcept { failure=std::current_exception(); }
    };
    GameSession() = default;
    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;
    GameSession(GameSession&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    GameSession& operator=(GameSession&& other) noexcept {
        if(this!=&other) { reset();handle_=std::exchange(other.handle_, {}); }
        return *this;
    }
    ~GameSession() { reset(); }
    void resume() { if(handle_&&!handle_.done()) handle_.resume(); }
    bool done() const { return !handle_||handle_.done(); }
    SessionExit result() const {
        if(!handle_||!handle_.done()) throw std::logic_error("Session result is not ready");
        if(handle_.promise().failure) std::rethrow_exception(handle_.promise().failure);
        return handle_.promise().exit;
    }
    void reset() noexcept { if(handle_) { handle_.destroy();handle_={}; } }
private:
    explicit GameSession(Handle handle) : handle_(handle) {}
    Handle handle_{};
};
}
