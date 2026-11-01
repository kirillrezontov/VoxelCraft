#pragma once
#include "engine/voxel/registry.hpp"
#include "game/gameplay/items.hpp"
#include "game/modding/save_data.hpp"
#include <filesystem>
#include <memory>
#include <span>

namespace vc {
struct ModContext;
struct ModEvent;
namespace commands { class Registry; }
struct NativeModInfo {
    std::string id,version;
    std::filesystem::path directory,library;
    std::map<std::string,std::string,std::less<>> dependencies;
};
// Metadata discovery never opens a library. Construction is explicit trusted load.
class NativeModRuntime {
public:
    static std::vector<NativeModInfo> discover(const std::filesystem::path& root);
    NativeModRuntime(const std::filesystem::path& root,voxel::BlockRegistry& blocks,
        std::span<const SavedMod> dataMods={},const ItemRegistry* dataItems=nullptr);
    ~NativeModRuntime();
    NativeModRuntime(const NativeModRuntime&)=delete;
    NativeModRuntime& operator=(const NativeModRuntime&)=delete;
    void installItems(ItemRegistry&,const voxel::BlockRegistry&,std::vector<Recipe>&) const;
    void installCommands(commands::Registry&) const;
    std::vector<std::string> dispatch(const ModEvent&,ModContext);
    std::vector<SavedMod> save() const;
    void restore(std::span<const SavedMod> saved);
    std::vector<std::string> diagnostics();
    bool contains(std::string_view id) const;
    void shutdown(); // Owner thread, outside dispatch; no hot-unload promise.
private:
    struct State;
    std::shared_ptr<State> state_; // Commands retain weak guards, never DLL ownership.
};
}
