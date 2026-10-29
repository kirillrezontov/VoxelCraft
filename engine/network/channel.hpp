#pragma once
#include "engine/core/binary.hpp"
#include "engine/platform/tcp.hpp"
#include <deque>

namespace vc::net {
inline constexpr std::size_t maxFrameBytes=4*1024*1024;
inline constexpr std::size_t maxBufferedBytes=8*1024*1024;
inline constexpr std::size_t maxQueuedFrames=256;
// Four-byte little-endian length followed by a nonempty payload. TCP packet
// boundaries are deliberately irrelevant; malformed/oversized streams fail.
class FrameDecoder {
public:
    void feed(std::span<const std::uint8_t> bytes);
    void finish() const;
    std::optional<binary::Bytes> take();
    std::size_t bufferedBytes() const { return buffer_.size()+readyBytes_; }
private:
    binary::Bytes buffer_;
    std::deque<binary::Bytes> ready_;
    std::size_t readyBytes_{};
};
class FramedConnection {
public:
    explicit FramedConnection(TcpStream stream):stream_(std::move(stream)) {}
    // False is bounded backpressure; no payload bytes were queued in that case.
    bool queue(std::span<const std::uint8_t> payload);
    // Both directions have a per-pump byte budget, and never block for I/O.
    void pump(std::size_t budget=256*1024);
    std::optional<binary::Bytes> take() { return incoming_.take(); }
    bool open() const { return stream_.open(); }
    std::size_t queuedBytes() const { return outgoingBytes_; }
    void close();
private:
    TcpStream stream_;
    FrameDecoder incoming_;
    std::deque<binary::Bytes> outgoing_;
    std::size_t offset_{},outgoingBytes_{};
};
}
