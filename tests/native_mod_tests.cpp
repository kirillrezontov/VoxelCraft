#include "game/modding/native_runtime.hpp"
#include "game/modding/runtime.hpp"
#include "game/content/services.hpp"
#include "game/content/blocks.hpp"
#include "game/commands/commands.hpp"
#include "server/host.hpp"
#include "tests/temp_directory.hpp"
#include "voxelcraft/native_mod.hpp"
#include "engine/core/paths.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <fstream>
#include <cstdlib>
#include <chrono>
#include <thread>

namespace {
void manifest(const std::filesystem::path& root,std::string id,std::string library,std::string dependencies="{}",int abi=1) {
    const auto path=root/id;std::filesystem::create_directories(path);
    std::ofstream out(path/"native.json");
    out<<"{\"id\":\""<<id<<"\",\"version\":\"1.0.0\",\"abi\":"<<abi
        <<",\"library\":{\"linux\":\""<<library<<".so\",\"windows\":\""<<library<<".dll\"},\"dependencies\":"<<dependencies<<"}";
    const auto name=library+VC_NATIVE_LIBRARY_SUFFIX;
    std::filesystem::copy_file(std::filesystem::path(VC_NATIVE_FIXTURE_DIR)/name,path/name);
}
std::string read(const std::filesystem::path& file) { std::ifstream input(file);return {std::istreambuf_iterator<char>(input),{}}; }
class Lifecycle {
public:
    explicit Lifecycle(const std::filesystem::path& file) {
        if(const auto* prior=std::getenv("VC_NATIVE_LIFECYCLE_FILE")) previous_=prior;
#ifdef _WIN32
        _putenv_s("VC_NATIVE_LIFECYCLE_FILE",vc::pathText(file).c_str());
#else
        setenv("VC_NATIVE_LIFECYCLE_FILE",file.c_str(),1);
#endif
    }
    ~Lifecycle() {
#ifdef _WIN32
        _putenv_s("VC_NATIVE_LIFECYCLE_FILE",previous_.c_str());
#else
        if(previous_.empty()) unsetenv("VC_NATIVE_LIFECYCLE_FILE");else setenv("VC_NATIVE_LIFECYCLE_FILE",previous_.c_str(),1);
#endif
    }
private: std::string previous_;
};
}
TEST(NativeSDK, HostWrapperRejectsWrongMajorTruncatedAndMissingFunctions) {
    EXPECT_THROW((void)voxelcraft::sdk::Host{nullptr},std::invalid_argument);
    const auto prefix=std::make_unique<uint32_t>(sizeof(uint32_t));
    EXPECT_THROW((void)voxelcraft::sdk::Host{reinterpret_cast<const vc_mod_host*>(prefix.get())},std::invalid_argument);
    vc_mod_host host{};host.struct_size=sizeof(host);host.abi_major=99;
    EXPECT_THROW((void)voxelcraft::sdk::Host{&host},std::invalid_argument);
    host.abi_major=1;host.struct_size=16;EXPECT_THROW((void)voxelcraft::sdk::Host{&host},std::invalid_argument);
    host.struct_size=sizeof(host);EXPECT_THROW((void)voxelcraft::sdk::Host{&host},std::invalid_argument);
}
TEST(NativeMods, CppSampleRegistersRealContentCraftingEventsAndAuthoritativeCommands) {
    auto blocks=vc::builtinBlocks();vc::NativeModRuntime runtime(VC_NATIVE_SAMPLE_ROOT,blocks);
    const auto beacon=blocks.require("native_sample:beacon");EXPECT_EQ(blocks.definition(beacon).emission,12);
    auto items=vc::builtinItems(blocks);auto recipes=vc::builtinRecipes(items);runtime.installItems(items,blocks,recipes);
    EXPECT_TRUE(items.find("native_sample:dust"));
    const auto recipe=std::find_if(recipes.begin(),recipes.end(),[](const auto& value){return value.name=="native_sample:beacon";});
    ASSERT_NE(recipe,recipes.end());vc::SurvivalInventory inventory;ASSERT_TRUE(inventory.add(items.require("voxelcraft:stone"),1,items));
    EXPECT_EQ(inventory.craft(*recipe,items),vc::CraftResult::Crafted);EXPECT_EQ(inventory.count(items.require("native_sample:beacon")),1u);
    vc::voxel::World world;world.insert(vc::voxel::Chunk({0,0,0}));
    const auto messages=runtime.dispatch({vc::ModEventKind::Load},{world,blocks,items,nullptr,{{10,10,10},{11,12,11}}});
    ASSERT_EQ(messages.size(),1u);EXPECT_NE(messages[0].find("Native SDK sample is running"),std::string::npos);
    auto commands=vc::commands::builtinCommands();runtime.installCommands(commands);
    vc::commands::Context context;context.world=&world;context.blocks=&blocks;context.items=&items;
    EXPECT_TRUE(commands.execute("native_sample:ping",context).success());
    EXPECT_EQ(commands.execute("native_sample:stamp 1 1 1",context).code,vc::commands::Code::PermissionDenied);
    context.permission=vc::commands::Permission::Administrator;
    EXPECT_TRUE(commands.execute("native_sample:stamp 1 1 1",context).success());EXPECT_EQ(world.block({1,1,1}),beacon);
    EXPECT_FALSE(commands.execute("native_sample:stamp 9999999 1 1",context).success());
    EXPECT_EQ(world.block({1,1,1}),beacon);
    EXPECT_TRUE(commands.execute("native_sample:retire",context).success());
    EXPECT_EQ(commands.execute("native_sample:ping",context).code,vc::commands::Code::Unavailable);
    EXPECT_TRUE(runtime.dispatch({vc::ModEventKind::Load},{world,blocks,items,nullptr,{}}).empty());
    runtime.shutdown();EXPECT_EQ(commands.execute("native_sample:ping",context).code,vc::commands::Code::Unavailable);
    EXPECT_TRUE(runtime.dispatch({vc::ModEventKind::Tick},{world,blocks,items,nullptr,{}}).empty());
}
TEST(NativeMods, PureCModuleRunsThroughTheSameVersionedHostTable) {
    auto blocks=vc::builtinBlocks();vc::NativeModRuntime runtime(VC_NATIVE_C_ROOT,blocks);
    auto items=vc::builtinItems(blocks);vc::voxel::World world;
    const auto messages=runtime.dispatch({vc::ModEventKind::Load},{world,blocks,items,nullptr,{}});
    ASSERT_EQ(messages.size(),1u);EXPECT_NE(messages[0].find("Pure C ABI callback executed"),std::string::npos);
}
TEST(NativeMods, DiscoveryAndDependencyRejectionDoNotExecuteNativeCode) {
    vc::test::TempDirectory temp;const auto marker=temp.path()/"lifecycle.txt";Lifecycle track(marker);
    manifest(temp.path(),"base_mod","base_mod","{\"missing\":\"1.0.0\"}");
    EXPECT_EQ(vc::NativeModRuntime::discover(temp.path()).size(),1u);EXPECT_FALSE(std::filesystem::exists(marker));
    auto blocks=vc::builtinBlocks();const auto count=blocks.size();
    EXPECT_THROW(vc::NativeModRuntime(temp.path(),blocks),std::exception);EXPECT_EQ(blocks.size(),count);EXPECT_FALSE(std::filesystem::exists(marker));
}
TEST(NativeMods, DependencyOrderCompatibleMinorAndReverseShutdownAreReal) {
    vc::test::TempDirectory temp;const auto marker=temp.path()/"lifecycle.txt";Lifecycle track(marker);
    manifest(temp.path(),"base_mod","base_mod");manifest(temp.path(),"dependent_mod","dependent_mod","{\"base_mod\":\"1.0.0\"}");
    auto blocks=vc::builtinBlocks();
    { vc::NativeModRuntime runtime(temp.path(),blocks);EXPECT_EQ(runtime.save().size(),2u); }
    const auto events=read(marker);
    EXPECT_LT(events.find("base_mod:start"),events.find("dependent_mod:query"));
    EXPECT_LT(events.find("dependent_mod:stop"),events.find("base_mod:stop"));
    EXPECT_LT(events.find("dependent_mod:destroy"),events.find("dependent_mod:unload"));
    EXPECT_LT(events.find("base_mod:destroy"),events.find("base_mod:unload"));
}
TEST(NativeMods, VersionTableRegistrationAndStartFailuresRollbackBeforePublication) {
    for(const auto* id:{"wrong_major","short_table","failed_start","duplicate_registration","overflow_blocks","missing_entry"}) {
        SCOPED_TRACE(id);vc::test::TempDirectory temp;const auto marker=temp.path()/"lifecycle.txt";Lifecycle track(marker);
        manifest(temp.path(),id,id);auto blocks=vc::builtinBlocks();const auto count=blocks.size();
        EXPECT_THROW(vc::NativeModRuntime(temp.path(),blocks),std::exception);EXPECT_EQ(blocks.size(),count);EXPECT_FALSE(blocks.find(std::string(id)+":block"));
        const auto events=read(marker);EXPECT_NE(events.find("unload"),std::string::npos);
        if(std::string_view(id)=="failed_start") { EXPECT_LT(events.find("stop"),events.find("destroy"));EXPECT_LT(events.find("destroy"),events.find("unload")); }
    }
}
TEST(NativeMods, HostRejectsForeignThreadCallsAndFailedCallbacksDiscardAllStagedEffects) {
    for(const auto* id:{"thread_guard","rollback_callback"}) {
        SCOPED_TRACE(id);vc::test::TempDirectory temp;manifest(temp.path(),id,id);
        auto blocks=vc::builtinBlocks();vc::NativeModRuntime runtime(temp.path(),blocks);auto items=vc::builtinItems(blocks);
        vc::voxel::World world;world.insert(vc::voxel::Chunk({0,0,0}));
        const auto result=runtime.dispatch({vc::ModEventKind::Load},{world,blocks,items,nullptr,{{10,10,10},{11,12,11}}});
        if(std::string_view(id)=="thread_guard") { ASSERT_EQ(result.size(),1u);EXPECT_NE(result[0].find("Owner thread enforced"),std::string::npos); }
        else { EXPECT_TRUE(result.empty());EXPECT_EQ(world.block({0,1,0}),vc::voxel::air);EXPECT_FALSE(runtime.diagnostics().empty());
            EXPECT_TRUE(runtime.dispatch({vc::ModEventKind::Load},{world,blocks,items,nullptr,{}}).empty()); }
    }
}
TEST(NativeMods, ManifestTraversalOversizeAndSymlinkEscapeAreRejectedBeforeLoad) {
    vc::test::TempDirectory temp;const auto marker=temp.path()/"marker";Lifecycle track(marker);
    manifest(temp.path(),"base_mod","base_mod");
    { std::ofstream file(temp.path()/"base_mod/native.json");file<<"{\"id\":\"base_mod\",\"version\":\"1.0.0\",\"abi\":1,\"library\":{\"linux\":\"../base_mod.so\",\"windows\":\"../base_mod.dll\"}}"; }
    EXPECT_THROW(vc::NativeModRuntime::discover(temp.path()),std::exception);EXPECT_FALSE(std::filesystem::exists(marker));
    { std::ofstream file(temp.path()/"base_mod/native.json");file<<std::string(16385,' '); }
    EXPECT_THROW(vc::NativeModRuntime::discover(temp.path()),std::exception);
    std::filesystem::remove_all(temp.path()/"base_mod");
    vc::test::TempDirectory outside;manifest(outside.path(),"base_mod","base_mod");
    std::error_code error;std::filesystem::create_directory_symlink(outside.path()/"base_mod",temp.path()/"base_mod",error);
    if(error) GTEST_SKIP()<<"Host cannot create symlinks: "<<error.message();
    EXPECT_THROW(vc::NativeModRuntime::discover(temp.path()),std::exception);EXPECT_FALSE(std::filesystem::exists(marker));
}
TEST(NativeMods, DependencyCyclesAndVersionsAreRejectedBeforeLoad) {
    vc::test::TempDirectory temp;const auto marker=temp.path()/"lifecycle.txt";Lifecycle track(marker);
    manifest(temp.path(),"base_mod","base_mod","{\"dependent_mod\":\"1.0.0\"}");
    manifest(temp.path(),"dependent_mod","dependent_mod","{\"base_mod\":\"1.0.0\"}");
    auto blocks=vc::builtinBlocks();EXPECT_THROW(vc::NativeModRuntime(temp.path(),blocks),std::exception);EXPECT_FALSE(std::filesystem::exists(marker));
    vc::test::TempDirectory versions;
    manifest(versions.path(),"base_mod","base_mod");manifest(versions.path(),"dependent_mod","dependent_mod","{\"base_mod\":\"2.0.0\"}");
    EXPECT_THROW(vc::NativeModRuntime(versions.path(),blocks),std::exception);EXPECT_FALSE(std::filesystem::exists(marker));
}
TEST(NativeMods, ExistingModRuntimePreservesNativeIdentityAndMissingModState) {
    auto blocks=vc::builtinBlocks();vc::ModRuntime mods({},blocks,VC_NATIVE_SAMPLE_ROOT);
    ASSERT_EQ(mods.size(),1u);auto saved=mods.save();ASSERT_EQ(saved.size(),1u);EXPECT_EQ(saved[0].id,"native_sample");
    saved[0].state["preserved"]="opaque";mods.restore(saved);EXPECT_EQ(mods.save()[0].state.at("preserved"),"opaque");
    auto emptyBlocks=vc::builtinBlocks();vc::ModRuntime disabled({},emptyBlocks);disabled.restore(mods.save());
    EXPECT_EQ(disabled.save()[0].state.at("preserved"),"opaque");
    auto items=vc::builtinItems(blocks);auto recipes=vc::builtinRecipes(items);mods.installItems(items,blocks,recipes);EXPECT_TRUE(items.find("native_sample:dust"));
}
TEST(DataMods, ExampleIsDeclarativeAndUsesTheSameRegistryAndCraftTransaction) {
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::path(VC_DATA_EXAMPLE_ROOT)/"signal/init.lua"));
    auto blocks=vc::builtinBlocks();vc::ModRuntime mods(VC_DATA_EXAMPLE_ROOT,blocks);
    EXPECT_EQ(mods.size(),1u);const auto beacon=blocks.require("signal:beacon");EXPECT_EQ(blocks.definition(beacon).emission,12);
    auto items=vc::builtinItems(blocks);auto recipes=vc::builtinRecipes(items);mods.installItems(items,blocks,recipes);
    EXPECT_TRUE(items.find("signal:crystal"));const auto found=std::find_if(recipes.begin(),recipes.end(),[](const auto& r){return r.name=="AMBER SIGNAL";});
    ASSERT_NE(found,recipes.end());vc::SurvivalInventory inventory;ASSERT_TRUE(inventory.add(items.require("voxelcraft:stone"),1,items));
    EXPECT_EQ(inventory.craft(*found,items),vc::CraftResult::Crafted);EXPECT_EQ(inventory.count(items.require("signal:beacon")),1u);
}
TEST(NativeMods, RealServerExecutesNativeCommandAndPersistsItsRegisteredBlock) {
    vc::test::TempDirectory temp;vc::server::HostConfig config;config.port=0;config.radius=1;config.generatorVersion=1;
    config.seed=42;config.populate=false;config.worldDirectory=temp.path();config.nativeModsDirectory=VC_NATIVE_SAMPLE_ROOT;
    {
        vc::server::Host host(config);const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(!host.ready()&&std::chrono::steady_clock::now()<deadline) { host.tick(1.0/60);std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        ASSERT_TRUE(host.ready());EXPECT_TRUE(host.console("native_sample:ping").success());
        const auto result=host.console("native_sample:stamp 0 60 0");ASSERT_TRUE(result.success());
        EXPECT_EQ(host.world().block({0,60,0}),host.blocks().require("native_sample:beacon"));EXPECT_EQ(host.save(),1u);host.shutdown();
    }
    vc::voxel::WorldStore store(temp.path());auto blocks=vc::builtinBlocks();const auto archive=store.load(blocks);ASSERT_TRUE(archive);
    EXPECT_EQ(archive->world.block({0,60,0}),blocks.require("native_sample:beacon"));
}
