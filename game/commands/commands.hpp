#pragma once
#include "engine/voxel/storage.hpp"
#include "game/player/controller.hpp"
#include "game/simulation/creatures.hpp"
#include "game/simulation/day_cycle.hpp"
#include <functional>
#include <map>
#include <span>
#include <stdexcept>

namespace vc::commands {
inline constexpr std::size_t maxCommandBytes=1024,maxArguments=16,maxResultLines=16,maxResultBytes=240;
enum class Permission : std::uint32_t { Player,Moderator,Administrator,ServerConsole };
enum class Code : std::uint32_t { Success,UnknownCommand,MissingArgument,InvalidArgument,InvalidTarget,PermissionDenied,Unavailable,InternalFailure };
std::string_view permissionName(Permission permission);
struct Result {
    Code code{Code::Success};
    std::vector<std::string> lines;
    bool success() const { return code==Code::Success; }
    static Result ok(std::string message);
    static Result error(Code code,std::string message);
};
class Error:public std::runtime_error {
public:
    Error(Code value,std::string message):std::runtime_error(std::move(message)),code(value) {}
    Code code;
};
struct Parsed { std::string name;std::vector<std::string> arguments; };
Parsed parse(std::string_view text);
double coordinate(std::string_view text,double relativeTo);
std::uint64_t unsignedNumber(std::string_view text,std::uint64_t maximum);
struct Player {
    std::string name;
    PlayerController* controller{};
    SurvivalState* survival{};
    bool* survivalMode{};
    std::array<std::string,9>* hotbar{};
    std::uint32_t* selectedSlot{};
    std::optional<glm::dvec3>* spawnPoint{};
    std::uint64_t* poseRevision{};
    Permission permission{Permission::Player};
};
class Registry;
struct Context {
    Permission permission{Permission::Player};
    std::string issuer;
    glm::dvec3 origin{}; // Feet of the issuer, or server anchor.
    voxel::World* world{};
    const voxel::BlockRegistry* blocks{};
    ItemRegistry* items{};
    LivingWorld* living{};
    DayCycle* day{};
    voxel::WorldMetadata metadata;
    std::vector<Player> players;
    // Consult generated/retained terrain without mutating it or blocking for jobs.
    std::function<voxel::BlockId(voxel::Position)> sample;
    std::function<Result()> save;
    std::function<void(std::string_view)> broadcast;
    std::function<Result(std::string_view,std::string_view)> kick;
    std::function<Result(std::string_view,Permission)> setPermission;
    const Registry* registry{};
};
Player& target(Context& context,std::string_view name);
bool safeFeet(const Context& context,glm::dvec3 feet);
void teleport(Player& player,glm::dvec3 feet);
enum class ArgumentKind { Text,Player,Item,Entity,Mode,Coordinate,Number,Time };
struct Argument { std::string name;ArgumentKind kind{};bool optional{}; };
struct Definition {
    std::string name;
    std::vector<std::string> aliases;
    std::string description,usage;
    Permission permission{Permission::Player};
    std::vector<Argument> arguments;
    std::size_t minimum{},maximum{};
    std::function<Result(Context&,std::span<const std::string>)> execute;
    std::function<bool(const Context&)> available{};
};
struct CompletionContext {
    Permission permission{Permission::Player};
    const ItemRegistry* items{};
    std::vector<std::string> players;
    bool self{true},administration{};
};
struct Completion { std::string label,replacement; };
class Registry {
public:
    void add(Definition definition);
    const Definition* find(std::string_view name) const;
    Result execute(std::string_view command,Context& context) const noexcept;
    std::vector<Completion> complete(std::string_view prefix,const CompletionContext& context) const;
    const std::map<std::string,Definition>& definitions() const { return definitions_; }
private:
    std::map<std::string,Definition> definitions_;
    std::map<std::string,std::string> aliases_;
};
Registry builtinCommands();
}
