#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vc::utf8 {
struct Codepoint { char32_t value{};std::size_t begin{},end{}; };
// Strict UTF-8: reject overlong forms, surrogate code points and values > U+10FFFF.
std::vector<Codepoint> decode(std::string_view text);
bool valid(std::string_view text);
std::size_t previous(std::string_view text,std::size_t byte);
std::size_t next(std::string_view text,std::size_t byte);
std::string lower(std::string_view text);
std::string truncate(std::string_view text,std::size_t maxBytes);
std::string singleLine(std::string_view text,std::size_t maxBytes);
}
