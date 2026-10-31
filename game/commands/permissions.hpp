#pragma once
#include "game/commands/commands.hpp"

namespace vc::commands {
// Operator-owned profile-key configuration; never populated from client packets.
// Host additionally requires authentication before using a stored role.
class PermissionStore {
public:
    explicit PermissionStore(std::filesystem::path file={});
    Permission get(std::string_view profile) const;
    bool set(std::string_view profile,Permission permission);
private:
    void persist(const std::map<std::string,Permission>& roles) const;
    std::filesystem::path file_;
    std::map<std::string,Permission> roles_;
};
}
