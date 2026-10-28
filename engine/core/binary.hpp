#pragma once
#include <bit>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace vc::binary {
using Bytes = std::vector<std::uint8_t>;
inline std::uint64_t checksum(std::span<const std::uint8_t> bytes) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (auto byte : bytes) { hash ^= byte; hash *= 1099511628211ULL; }
    return hash;
}
class Writer {
public:
    Bytes bytes;
    void u16(std::uint16_t value) { integer(value,2); }
    void u32(std::uint32_t value) { integer(value,4); }
    void u64(std::uint64_t value) { integer(value,8); }
    void i64(std::int64_t value) { u64(std::bit_cast<std::uint64_t>(value)); }
    void f64(double value) { static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559); u64(std::bit_cast<std::uint64_t>(value)); }
    void raw(std::string_view text) { if(!text.empty()) bytes.insert(bytes.end(),text.begin(),text.end()); }
    void text(std::string_view text) {
        if (text.size()>std::numeric_limits<std::uint16_t>::max()) throw std::invalid_argument("Binary string exceeds format limit");
        u16(static_cast<std::uint16_t>(text.size())); raw(text);
    }
    void seal() { u64(checksum(bytes)); }
private:
    void integer(std::uint64_t value,unsigned size) {
        for(unsigned i=0;i<size;++i) { bytes.push_back(static_cast<std::uint8_t>(value & 255)); value >>= 8; }
    }
};
class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
    static Reader checked(std::span<const std::uint8_t> bytes) {
        if(bytes.size()<8) throw std::runtime_error("Truncated checksummed data");
        Reader trailer(bytes.last(8));
        const auto payload=bytes.first(bytes.size()-8);
        if(checksum(payload)!=trailer.u64()) throw std::runtime_error("Save checksum mismatch");
        return Reader(payload);
    }
    std::uint16_t u16() { return static_cast<std::uint16_t>(integer(2)); }
    std::uint32_t u32() { return static_cast<std::uint32_t>(integer(4)); }
    std::uint64_t u64() { return integer(8); }
    std::int64_t i64() { return std::bit_cast<std::int64_t>(u64()); }
    double f64() { return std::bit_cast<double>(u64()); }
    std::string raw(std::size_t size) {
        if(size>bytes_.size()-offset_) throw std::runtime_error("Truncated save data");
        if(size==0) return {};
        const auto part=bytes_.subspan(offset_,size); offset_+=size;
        return {reinterpret_cast<const char*>(part.data()),part.size()};
    }
    std::string text() { return raw(u16()); }
    void magic(std::string_view expected) { if(raw(expected.size())!=expected) throw std::runtime_error("Invalid save file signature"); }
    void finish() const { if(offset_!=bytes_.size()) throw std::runtime_error("Unexpected trailing save data"); }
private:
    std::uint64_t integer(unsigned size) {
        if(size>bytes_.size()-offset_) throw std::runtime_error("Truncated save integer");
        std::uint64_t value{};
        for(unsigned i=0;i<size;++i) value |= static_cast<std::uint64_t>(bytes_[offset_++]) << (8*i);
        return value;
    }
    std::span<const std::uint8_t> bytes_;
    std::size_t offset_{};
};
}
