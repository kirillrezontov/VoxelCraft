#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace vc::net {
struct Endpoint { std::string host;std::uint16_t port{25565}; };
Endpoint parseEndpoint(std::string_view text,std::uint16_t defaultPort=25565);
}
