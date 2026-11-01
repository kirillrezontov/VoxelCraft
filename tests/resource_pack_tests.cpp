#include "engine/resources/catalog.hpp"
#include "game/ui/resource_theme.hpp"
#include "tests/temp_directory.hpp"
#include <gtest/gtest.h>
#include <fstream>

namespace {
void write(const std::filesystem::path& path,std::string_view value) {
    std::filesystem::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out<<value;
}
void pack(const std::filesystem::path& root,std::string_view id,int priority=0) {
    std::filesystem::create_directories(root/"resources/voxelcraft");
    write(root/"resource-pack.json","{\"id\":\""+std::string(id)+"\",\"name\":\"Test pack\",\"version\":\"1.0.0\",\"priority\":"+std::to_string(priority)+"}");
}
std::string bytes(const std::optional<std::vector<std::uint8_t>>& value) {
    if(!value) return {};
    return {value->begin(),value->end()};
}
}
TEST(ResourcePacks, StablePrioritiesNamespaceResolutionAndBuiltinFallback) {
    vc::test::TempDirectory temp;const auto builtin=temp.path()/"builtin";
    write(builtin/"voxelcraft/example.svg","builtin");pack(temp.path()/"zeta","zeta",10);pack(temp.path()/"alpha","alpha",10);
    write(temp.path()/"zeta/resources/voxelcraft/example.svg","zeta");write(temp.path()/"alpha/resources/voxelcraft/example.svg","alpha");
    vc::ResourceCatalog resources(builtin);resources.activate({temp.path()/"zeta",temp.path()/"alpha"});
    EXPECT_EQ(bytes(resources.load("voxelcraft:example.svg")),"alpha");
    EXPECT_EQ(bytes(resources.loadBuiltin("voxelcraft:example.svg")),"builtin");
    pack(temp.path()/"zeta","zeta",11);resources.activate({temp.path()/"alpha",temp.path()/"zeta"});
    EXPECT_EQ(bytes(resources.load("voxelcraft:example.svg")),"zeta");
    resources.activate({});EXPECT_EQ(bytes(resources.load("voxelcraft:example.svg")),"builtin");EXPECT_FALSE(resources.load("voxelcraft:missing.xml"));
}
TEST(ResourcePacks, FailedActivationKeepsPreviousProvidersAndRevision) {
    vc::test::TempDirectory temp;write(temp.path()/"builtin/voxelcraft/file.css","default");pack(temp.path()/"good","good");
    write(temp.path()/"good/resources/voxelcraft/file.css","selected");vc::ResourceCatalog resources(temp.path()/"builtin");resources.activate({temp.path()/"good"});
    const auto revision=resources.revision();pack(temp.path()/"bad","bad");write(temp.path()/"bad/resource-pack.json","{\"invalid\":true}");
    EXPECT_THROW(resources.activate({temp.path()/"good",temp.path()/"bad"}),std::exception);
    EXPECT_EQ(resources.revision(),revision);EXPECT_EQ(bytes(resources.load("voxelcraft:file.css")),"selected");
    EXPECT_THROW(resources.activate({temp.path()/"good",temp.path()/"good"}),std::exception);
    EXPECT_THROW(resources.activate(std::vector<std::filesystem::path>(33,temp.path()/"good")),std::exception);
}
TEST(ResourcePacks, InvalidIdsRejectTraversalAbsolutePathsAndAmbiguousSegments) {
    vc::test::TempDirectory temp;std::filesystem::create_directory(temp.path()/"builtin");vc::ResourceCatalog resources(temp.path()/"builtin");
    for(const auto* id:{"../secret",":file","app:/file","app:../file","app:a/../file","app:a//b","app:a/","app:C:\\secret","app:a:b","http://host/file"}) {
        SCOPED_TRACE(id);EXPECT_FALSE(vc::ResourceCatalog::validIdentifier(id));EXPECT_THROW(resources.load(id),std::exception);
    }
    EXPECT_TRUE(vc::ResourceCatalog::validIdentifier("voxelcraft:ui/main-menu.xml"));
}
TEST(ResourcePacks, ExactReadBudgetAndOneBeyondFallsBackWithoutLargeAllocation) {
    vc::test::TempDirectory temp;write(temp.path()/"builtin/voxelcraft/value","ok");pack(temp.path()/"pack","pack");
    write(temp.path()/"pack/resources/voxelcraft/value","12345678");vc::ResourceCatalog resources(temp.path()/"builtin");resources.activate({temp.path()/"pack"});
    EXPECT_EQ(bytes(resources.load("voxelcraft:value",8)),"12345678");
    write(temp.path()/"pack/resources/voxelcraft/value","123456789");EXPECT_EQ(bytes(resources.load("voxelcraft:value",8)),"ok");
    EXPECT_FALSE(resources.takeDiagnostics().empty());EXPECT_THROW(resources.load("voxelcraft:value",vc::ResourceCatalog::maxResourceBytes+1),std::exception);
}
TEST(ResourcePacks, EscapingAssetAndProviderSymlinksAreRejected) {
    vc::test::TempDirectory temp;write(temp.path()/"builtin/voxelcraft/value","builtin");write(temp.path()/"secret","secret");pack(temp.path()/"pack","pack");
    std::error_code error;std::filesystem::create_symlink(temp.path()/"secret",temp.path()/"pack/resources/voxelcraft/value",error);
    if(error) GTEST_SKIP()<<"Host cannot create symlinks: "<<error.message();
    vc::ResourceCatalog resources(temp.path()/"builtin");resources.activate({temp.path()/"pack"});
    EXPECT_EQ(bytes(resources.load("voxelcraft:value")),"builtin");EXPECT_FALSE(resources.takeDiagnostics().empty());
    std::filesystem::remove_all(temp.path()/"pack/resources");std::filesystem::create_directory_symlink(temp.path()/"builtin",temp.path()/"pack/resources");
    EXPECT_THROW(resources.activate({temp.path()/"pack"}),std::exception);
}
TEST(ResourcePacks, InvalidThemeUsesValidatedBuiltinAndNeverEnablesUnknownFeatures) {
    vc::test::TempDirectory temp;write(temp.path()/"builtin/voxelcraft/legacy-theme.json","{\"accent\":[0.2,0.3,0.4,1]}");pack(temp.path()/"pack","pack");
    vc::ResourceCatalog resources(temp.path()/"builtin");resources.activate({temp.path()/"pack"});
    for(const auto* text:{"{broken","{\"url\":\"https://example.invalid\"}","{\"accent\":[1e999,0,0,1]}","{\"accent\":[-1,0,0,1]}"}) {
        write(temp.path()/"pack/resources/voxelcraft/legacy-theme.json",text);std::vector<std::string> diagnostics;
        const auto theme=vc::loadResourceTheme(resources,diagnostics);EXPECT_EQ(theme.accent,glm::vec4(0.2f,0.3f,0.4f,1));EXPECT_FALSE(diagnostics.empty());
    }
    write(temp.path()/"pack/resources/voxelcraft/legacy-theme.json","{\"accent\":[1,0.5,0,1]}");std::vector<std::string> diagnostics;
    EXPECT_EQ(vc::loadResourceTheme(resources,diagnostics).accent,glm::vec4(1,0.5f,0,1));EXPECT_TRUE(diagnostics.empty());
}
