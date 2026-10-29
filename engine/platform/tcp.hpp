#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace vc::net {
struct IoResult {
    std::size_t bytes{};
    bool closed{}; // Zero bytes without closed means the socket would block.
};
class TcpListener;
// Owning, nonblocking stream. Native socket/WinSock details stay in this layer.
class TcpStream {
public:
    TcpStream();
    ~TcpStream();
    TcpStream(TcpStream&&) noexcept;
    TcpStream& operator=(TcpStream&&) noexcept;
    TcpStream(const TcpStream&)=delete;
    TcpStream& operator=(const TcpStream&)=delete;
    static TcpStream connect(std::string_view host,std::uint16_t port,
        std::chrono::milliseconds timeout=std::chrono::seconds(5));
    IoResult receive(std::span<std::uint8_t> buffer);
    IoResult send(std::span<const std::uint8_t> buffer);
    bool open() const;
    void close();
private:
    friend class TcpListener;
    struct Impl;
    explicit TcpStream(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};
class TcpListener {
public:
    // Port zero asks the OS for an available port (useful for tests).
    explicit TcpListener(std::string_view address,std::uint16_t port);
    ~TcpListener();
    TcpListener(TcpListener&&) noexcept;
    TcpListener& operator=(TcpListener&&) noexcept;
    TcpListener(const TcpListener&)=delete;
    TcpListener& operator=(const TcpListener&)=delete;
    std::optional<TcpStream> accept();
    std::uint16_t port() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
