#include "engine/network/channel.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace vc::net {
void FrameDecoder::feed(std::span<const std::uint8_t> bytes) {
    const auto used=bufferedBytes();
    if(used>maxBufferedBytes||bytes.size()>maxBufferedBytes-used) throw std::runtime_error("TCP inbound buffer limit exceeded");
    buffer_.insert(buffer_.end(),bytes.begin(),bytes.end());std::size_t consumed=0;
    while(buffer_.size()-consumed>=4) {
        std::uint32_t length=0;
        for(unsigned i=0;i<4;++i) length|=static_cast<std::uint32_t>(buffer_[consumed+i])<<(i*8);
        if(!length||length>maxFrameBytes) throw std::runtime_error("Invalid TCP frame length");
        if(buffer_.size()-consumed-4<length) break;
        if(ready_.size()>=maxQueuedFrames) throw std::runtime_error("TCP queued message limit exceeded");
        const auto begin=buffer_.begin()+static_cast<std::ptrdiff_t>(consumed+4);
        ready_.emplace_back(begin,begin+length);readyBytes_+=length;consumed+=4+length;
    }
    buffer_.erase(buffer_.begin(),buffer_.begin()+static_cast<std::ptrdiff_t>(consumed));
}
void FrameDecoder::finish() const { if(!buffer_.empty()) throw std::runtime_error("Truncated TCP frame at disconnect"); }
std::optional<binary::Bytes> FrameDecoder::take() {
    if(ready_.empty()) return {};
    auto frame=std::move(ready_.front());ready_.pop_front();readyBytes_-=frame.size();return frame;
}
bool FramedConnection::queue(std::span<const std::uint8_t> payload) {
    if(!open()) throw std::logic_error("Cannot queue on a closed connection");
    if(payload.empty()||payload.size()>maxFrameBytes) throw std::invalid_argument("TCP message exceeds frame limits");
    if(payload.size()+4>maxBufferedBytes-outgoingBytes_||outgoing_.size()>=maxQueuedFrames) return false;
    binary::Bytes frame(payload.size()+4);const auto size=static_cast<std::uint32_t>(payload.size());
    for(unsigned i=0;i<4;++i) frame[i]=static_cast<std::uint8_t>((size>>(i*8))&255);
    std::copy(payload.begin(),payload.end(),frame.begin()+4);outgoing_.push_back(std::move(frame));outgoingBytes_+=payload.size()+4;return true;
}
void FramedConnection::pump(std::size_t budget) {
    if(!budget||budget>maxFrameBytes) throw std::invalid_argument("TCP pump budget must be 1..4 MiB");
    if(!open()) return;
    try {
        std::size_t sent=0;
        while(!outgoing_.empty()&&sent<budget) {
            const auto remaining=std::span<const std::uint8_t>(outgoing_.front()).subspan(offset_);
            const auto result=stream_.send(remaining.first(std::min(remaining.size(),budget-sent)));
            if(result.closed) { close();return; }
            if(!result.bytes) break;
            sent+=result.bytes;sentBytes_+=result.bytes;offset_+=result.bytes;outgoingBytes_-=result.bytes;
            if(offset_==outgoing_.front().size()) { outgoing_.pop_front();offset_=0; }
        }
        std::array<std::uint8_t,16384> buffer{};std::size_t received=0;
        while(received<budget) {
            const auto result=stream_.receive(std::span(buffer).first(std::min(buffer.size(),budget-received)));
            if(result.closed) { incoming_.finish();stream_.close();outgoing_.clear();outgoingBytes_=offset_=0;return; }
            if(!result.bytes) break;
            incoming_.feed(std::span(buffer).first(result.bytes));received+=result.bytes;receivedBytes_+=result.bytes;
        }
    } catch(...) {
        // A framing/transport failure is terminal. In particular a truncated EOF
        // must not leave an apparently open socket occupying a server slot.
        close();throw;
    }
}
void FramedConnection::close() { stream_.close();outgoing_.clear();outgoingBytes_=offset_=0; }
}
