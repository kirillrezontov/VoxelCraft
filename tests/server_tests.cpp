#include "game/commands/permissions.hpp"
#include "server/host.hpp"
#include "game/network/client.hpp"
#include "game/player/save_state.hpp"
#include "game/player/spawn.hpp"
#include "game/content/blocks.hpp"
#include "tests/temp_directory.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <fstream>

namespace {
vc::server::HostConfig config() {
    vc::server::HostConfig result;result.port=0;result.radius=1;result.generatorVersion=1;result.seed=42;result.populate=false;return result;
}
template<class Ready> bool settle(vc::server::Host& host,Ready ready,std::span<vc::netgame::RemoteSession* const> clients={}) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    do {
        for(auto* client:clients) client->pump();
        host.tick(1.0/60);
        for(auto* client:clients) client->pump();
        if(ready()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while(std::chrono::steady_clock::now()<end);
    return false;
}
void createFlyingWorld(const std::filesystem::path& path) {
    const auto blocks=vc::builtinBlocks();const vc::voxel::TerrainGenerator generator(42,{},blocks,1);
    vc::PlayerSaveState state;state.camera.position=vc::findSpawn(generator,blocks);state.camera.position.y+=2;
    state.camera.pitch=-1.55;state.camera.yaw=0;state.flying=true;
    vc::PlayerProfile a;a.name="Alice";a.camera=state.camera;a.flying=true;a.selectedSlot=2;
    vc::PlayerProfile b=a;b.name="Bob";b.camera.position.x+=4;
    state.profiles={a,b};vc::voxel::WorldStore store(path);store.save(vc::voxel::World{},blocks,{42,{},1},vc::encodePlayer(state));
}
void createSurvivalWorld(const std::filesystem::path& path,bool falling=false) {
    const auto blocks=vc::builtinBlocks();vc::voxel::World world({0,31});
    for(int x=-1;x<=1;++x) for(int z=-1;z<=1;++z) {
        vc::voxel::Chunk chunk({x,0,z});
        for(int a=0;a<32;++a) for(int b=0;b<32;++b) chunk.set({a,0,b},blocks.require("voxelcraft:stone"));
        world.insert(std::move(chunk));
    }
    world.setBlock({0,2,0},blocks.require("voxelcraft:log"));
    vc::PlayerSaveState state;state.survival=true;state.camera.position={0.5,2.62,3.5};state.camera.yaw=0;state.camera.pitch=0;
    vc::PlayerProfile alice;alice.name="Alice";alice.camera=state.camera;alice.health=12;alice.items[4]={"voxelcraft:raw_meat",2,0};
    if(falling) { alice.camera.position={6.5,30.62,6.5};alice.health=20; }
    vc::PlayerProfile bob;bob.name="Bob";bob.camera=state.camera;bob.camera.position={4.5,2.62,3.5};
    state.profiles={alice,bob};vc::voxel::WorldStore store(path);store.save(world,blocks,{42,{0,31},1},vc::encodePlayer(state));
}
unsigned count(const vc::netgame::RemoteSession& client,std::string_view name) {
    if(!client.state().survival) return 0;
    unsigned total=0;for(const auto& item:client.state().survival->items) if(item.identifier==name) total+=item.count;return total;
}
}
TEST(ServerHost, RealClientsShareEditsMotionAndPersistentIndependentProfiles) {
    vc::test::TempDirectory temp;createFlyingWorld(temp.path());auto settings=config();settings.worldDirectory=temp.path();
    glm::dvec3 finalPosition{};vc::voxel::Position placed{};
    {
        vc::server::Host host(settings);ASSERT_TRUE(settle(host,[&]{return host.ready();}));
        auto aBlocks=vc::builtinBlocks(),bBlocks=vc::builtinBlocks();
        vc::netgame::RemoteSession alice("127.0.0.1",host.port(),"Alice",host.fingerprint(),aBlocks);
        vc::netgame::RemoteSession bob("127.0.0.1",host.port(),"Bob",host.fingerprint(),bBlocks);
        const std::array clients{&alice,&bob};
        ASSERT_TRUE(settle(host,[&]{return alice.welcome()&&bob.welcome()&&alice.world().chunks().size()==27&&bob.world().chunks().size()==27;},clients));
        EXPECT_EQ(host.playerCount(),2u);EXPECT_NE(alice.welcome()->id,bob.welcome()->id);EXPECT_EQ(alice.world().chunks().size(),host.world().chunks().size());
        const auto hit=vc::voxel::raycast(host.world(),alice.camera().position,alice.camera().forward());ASSERT_TRUE(hit);
        const auto target=vc::voxel::offsetPosition(hit->block,hit->normal);ASSERT_TRUE(target);placed=*target;
        vc::netgame::Hotbar hotbar;hotbar.names[0]="voxelcraft:lamp";alice.hotbar(hotbar);
        vc::netgame::Input action;action.flags=vc::netgame::Active|vc::netgame::Place;action.yaw=0;action.pitch=-1.55;alice.input(action);
        ASSERT_TRUE(settle(host,[&]{return host.world().block(placed)==host.blocks().require("voxelcraft:lamp")&&
            alice.world().block(placed)==aBlocks.require("voxelcraft:lamp")&&bob.world().block(placed)==bBlocks.require("voxelcraft:lamp");},clients));
        const auto before=alice.camera().position;const auto bobBefore=bob.camera().position;
        action.flags=vc::netgame::Active;action.pitch=0;action.forward=1;
        for(int i=0;i<30;++i) { alice.input(action);alice.pump();host.tick(1.0/60);alice.pump();bob.pump(); }
        action.forward=0;alice.input(action);
        ASSERT_TRUE(settle(host,[&]{
            for(const auto& peer:host.players()) if(peer.id==alice.welcome()->id) return peer.camera.position==alice.camera().position;
            return false;
        },clients));
        EXPECT_LT(alice.camera().position.z,before.z-2);EXPECT_EQ(bob.camera().position,bobBefore);
        finalPosition=alice.camera().position;alice.requestSave();
        ASSERT_TRUE(settle(host,[&]{for(const auto& profile:host.profiles()) if(profile.name=="Alice") return profile.camera.position.z<before.z-2;return false;},clients));
        alice.disconnect();bob.disconnect();ASSERT_TRUE(settle(host,[&]{return host.playerCount()==0;}));
        EXPECT_GE(host.save(),2u);
    }
    {
        vc::server::Host reopened(settings);ASSERT_TRUE(settle(reopened,[&]{return reopened.ready();}));auto blocks=vc::builtinBlocks();
        vc::netgame::RemoteSession alice("127.0.0.1",reopened.port(),"ALICE",reopened.fingerprint(),blocks);const std::array clients{&alice};
        ASSERT_TRUE(settle(reopened,[&]{return alice.welcome()!=nullptr;},clients));
        EXPECT_EQ(alice.welcome()->profile.name,"Alice");EXPECT_EQ(alice.camera().position,finalPosition);
        EXPECT_EQ(alice.welcome()->profile.hotbar[0],"voxelcraft:lamp");EXPECT_TRUE(alice.flying());
        ASSERT_TRUE(settle(reopened,[&]{return alice.world().loaded(placed);},clients));EXPECT_EQ(alice.world().block(placed),blocks.require("voxelcraft:lamp"));
    }
}
TEST(ServerHost, ContentMismatchAndDuplicateNamesAreRejectedWithoutExtraProfiles) {
    auto settings=config();vc::server::Host host(settings);ASSERT_TRUE(settle(host,[&]{return host.ready();}));
    auto wrongBlocks=vc::builtinBlocks();vc::netgame::RemoteSession wrong("127.0.0.1",host.port(),"Wrong",host.fingerprint()^1,wrongBlocks);
    const std::array wrongClients{&wrong};EXPECT_THROW(settle(host,[]{return false;},wrongClients),std::runtime_error);EXPECT_EQ(host.playerCount(),0u);EXPECT_TRUE(host.profiles().empty());
    auto firstBlocks=vc::builtinBlocks();vc::netgame::RemoteSession first("127.0.0.1",host.port(),"Alice",host.fingerprint(),firstBlocks);const std::array firstClient{&first};
    ASSERT_TRUE(settle(host,[&]{return first.welcome()!=nullptr;},firstClient));
    auto duplicateBlocks=vc::builtinBlocks();vc::netgame::RemoteSession duplicate("127.0.0.1",host.port(),"ALICE",host.fingerprint(),duplicateBlocks);
    const std::array duplicateClients{&first,&duplicate};EXPECT_THROW(settle(host,[]{return false;},duplicateClients),std::runtime_error);
    EXPECT_EQ(host.playerCount(),1u);EXPECT_EQ(host.profiles().size(),1u);
}
TEST(ServerHost, InitialCommandTimeoutStopsMotionAndUnknownPacketsDoNotKillHost) {
    vc::test::TempDirectory temp;createFlyingWorld(temp.path());auto settings=config();settings.worldDirectory=temp.path();vc::server::Host host(settings);
    ASSERT_TRUE(settle(host,[&]{return host.ready();}));auto blocks=vc::builtinBlocks();
    vc::netgame::RemoteSession alice("127.0.0.1",host.port(),"Alice",host.fingerprint(),blocks);const std::array clients{&alice};
    ASSERT_TRUE(settle(host,[&]{return alice.welcome()!=nullptr;},clients));
    vc::netgame::Input input;input.flags=vc::netgame::Active;input.forward=1;alice.input(input);
    for(int i=0;i<150;++i) { alice.pump();host.tick(1.0/60);alice.pump(); }
    const auto stopped=host.players().front().camera.position;
    for(int i=0;i<60;++i) { alice.pump();host.tick(1.0/60);alice.pump(); }
    EXPECT_EQ(host.players().front().camera.position,stopped);
    vc::net::FramedConnection invalid(vc::net::TcpStream::connect("127.0.0.1",host.port()));
    EXPECT_TRUE(invalid.queue(vc::netgame::encodeControl(vc::netgame::Message::Save)));invalid.pump();
    for(int i=0;i<5;++i) host.tick(1.0/60);
    EXPECT_EQ(host.playerCount(),1u);EXPECT_GT(host.ticks(),200u);EXPECT_THROW(host.tick(1),std::invalid_argument);
}
TEST(ServerSurvival, ServerTimesMiningCraftsTransfersConsumesAndPersistsPrivateInventories) {
    vc::test::TempDirectory temp;createSurvivalWorld(temp.path());auto settings=config();settings.worldDirectory=temp.path();
    std::array<vc::SavedItemStack,36> finalItems;
    {
        vc::server::Host host(settings);auto aBlocks=vc::builtinBlocks(),bBlocks=vc::builtinBlocks();
        vc::netgame::RemoteSession alice("127.0.0.1",host.port(),"Alice",host.fingerprint(),aBlocks);
        vc::netgame::RemoteSession bob("127.0.0.1",host.port(),"Bob",host.fingerprint(),bBlocks);const std::array clients{&alice,&bob};
        ASSERT_TRUE(settle(host,[&]{return alice.welcome()&&bob.welcome()&&alice.state().survival&&bob.state().survival&&alice.world().chunks().size()==9;},clients));
        ASSERT_TRUE(alice.welcome()->survival);EXPECT_EQ(alice.state().survival->health,12u);EXPECT_EQ(count(alice,"voxelcraft:raw_meat"),2u);EXPECT_EQ(count(bob,"voxelcraft:raw_meat"),0u);
        vc::netgame::Input input;input.flags=vc::netgame::Active|vc::netgame::ToggleFlight|vc::netgame::Destroy;input.yaw=0;input.pitch=0;
        alice.input(input);ASSERT_TRUE(settle(host,[&]{return host.ticks()%3==0;},clients));
        EXPECT_FALSE(host.players().front().flying);EXPECT_NE(host.world().block({0,2,0}),vc::voxel::air);EXPECT_EQ(count(alice,"voxelcraft:log"),0u);
        const auto drive=[&](int ticks) {
            for(int i=0;i<ticks;++i) { alice.input(input);alice.pump();bob.pump();host.tick(1.0/60);alice.pump();bob.pump(); }
        };
        input.flags=vc::netgame::Active|vc::netgame::AttackHeld;drive(40);
        ASSERT_TRUE(alice.state().survival->mining);EXPECT_GT(alice.state().survival->miningProgress,0.3);EXPECT_LT(alice.state().survival->miningProgress,0.8);
        input.flags=vc::netgame::Active;drive(4);EXPECT_FALSE(alice.state().survival->mining);
        input.flags=vc::netgame::Active|vc::netgame::AttackHeld;drive(80);
        input.flags=vc::netgame::Active;drive(3);
        EXPECT_EQ(host.world().block({0,2,0}),vc::voxel::air);EXPECT_EQ(count(alice,"voxelcraft:log"),1u);EXPECT_EQ(count(bob,"voxelcraft:log"),0u);
        alice.inventory({vc::InventoryActionKind::Craft,0,0,0});
        ASSERT_TRUE(settle(host,[&]{return count(alice,"voxelcraft:planks")==4;},clients));EXPECT_EQ(count(alice,"voxelcraft:log"),0u);
        alice.inventory({vc::InventoryActionKind::MoveToBackpack,0});
        ASSERT_TRUE(settle(host,[&]{return alice.state().survival->items[9].identifier=="voxelcraft:planks";},clients));
        alice.inventory({vc::InventoryActionKind::TransferOne,9,2});
        ASSERT_TRUE(settle(host,[&]{return alice.state().survival->items[2].count==1;},clients));EXPECT_EQ(alice.state().survival->items[9].count,3u);
        alice.inventory({vc::InventoryActionKind::Swap,9,0});
        ASSERT_TRUE(settle(host,[&]{return alice.state().survival->items[0].count==3;},clients));
        input.slot=4;input.flags=vc::netgame::Active|vc::netgame::Place;alice.input(input);
        ASSERT_TRUE(settle(host,[&]{return alice.state().survival->health==15;},clients));EXPECT_EQ(count(alice,"voxelcraft:raw_meat"),1u);
        // A valid craft request cannot mint resources when ingredients are absent.
        alice.inventory({vc::InventoryActionKind::Craft,0,0,2});
        ASSERT_TRUE(settle(host,[&]{for(const auto& notice:alice.takeNotices()) if(notice=="Missing materials") return true;return false;},clients));
        EXPECT_EQ(count(alice,"voxelcraft:wooden_pickaxe"),0u);
        finalItems=alice.state().survival->items;alice.disconnect();bob.disconnect();ASSERT_TRUE(settle(host,[&]{return host.playerCount()==0;}));host.save();
    }
    {
        vc::server::Host host(settings);auto blocks=vc::builtinBlocks();vc::netgame::RemoteSession alice("127.0.0.1",host.port(),"ALICE",host.fingerprint(),blocks);const std::array clients{&alice};
        ASSERT_TRUE(settle(host,[&]{return alice.welcome()!=nullptr;},clients));EXPECT_TRUE(alice.welcome()->survival);
        EXPECT_EQ(alice.welcome()->profile.health,15u);EXPECT_EQ(alice.welcome()->profile.items,finalItems);EXPECT_EQ(alice.welcome()->profile.selectedSlot,4u);
    }
}
TEST(ServerSurvival, FallDeathFreezesMovementAndRespawnRetainsInventory) {
    vc::test::TempDirectory temp;createSurvivalWorld(temp.path(),true);auto settings=config();settings.worldDirectory=temp.path();vc::server::Host host(settings);
    auto blocks=vc::builtinBlocks();vc::netgame::RemoteSession alice("127.0.0.1",host.port(),"Alice",host.fingerprint(),blocks);const std::array clients{&alice};
    ASSERT_TRUE(settle(host,[&]{return alice.welcome()&&alice.state().survival&&alice.state().survival->health==0;},clients));
    const auto before=host.players().front().camera.position;const auto items=alice.state().survival->items;
    vc::netgame::Input input;input.flags=vc::netgame::Active|vc::netgame::ToggleFlight;input.forward=1;alice.input(input);
    for(int i=0;i<30;++i) { alice.pump();host.tick(1.0/60);alice.pump(); }
    EXPECT_EQ(host.players().front().camera.position,before);EXPECT_EQ(alice.state().survival->items,items);
    input.flags=vc::netgame::Respawn;input.forward=0;alice.input(input);
    ASSERT_TRUE(settle(host,[&]{return alice.state().survival->health==20;},clients));EXPECT_EQ(alice.state().survival->items,items);EXPECT_FALSE(alice.flying());
    EXPECT_GT(alice.camera().position.y,1);EXPECT_NE(alice.camera().position,before);
}
TEST(ServerSurvival, CreativeInventoryFabricationDisconnectsOnlyTheOffendingPeer) {
    auto settings=config();settings.survival=true;vc::server::Host host(settings);auto aBlocks=vc::builtinBlocks(),bBlocks=vc::builtinBlocks();
    vc::netgame::RemoteSession alice("127.0.0.1",host.port(),"Alice",host.fingerprint(),aBlocks);
    vc::netgame::RemoteSession bob("127.0.0.1",host.port(),"Bob",host.fingerprint(),bBlocks);const std::array clients{&alice,&bob};
    ASSERT_TRUE(settle(host,[&]{return alice.welcome()&&bob.welcome();},clients));
    vc::netgame::Hotbar fabricated;fabricated.names[0]="voxelcraft:lamp";alice.hotbar(fabricated);
    ASSERT_TRUE(settle(host,[&]{return !alice.open();},clients));EXPECT_TRUE(bob.open());EXPECT_EQ(host.playerCount(),1u);
    EXPECT_EQ(count(bob,"voxelcraft:lamp"),0u);
}
TEST(ServerHost, AsynchronousSaveFailureKeepsPeersAndEditsAvailableForRetry) {
    vc::test::TempDirectory temp;createFlyingWorld(temp.path());auto settings=config();settings.worldDirectory=temp.path();
    vc::voxel::Position placed;
    {
        vc::server::Host host(settings);auto blocks=vc::builtinBlocks();vc::netgame::RemoteSession alice("127.0.0.1",host.port(),"Alice",host.fingerprint(),blocks);const std::array clients{&alice};
        ASSERT_TRUE(settle(host,[&]{return alice.welcome()&&alice.world().chunks().size()==27;},clients));
        const auto hit=vc::voxel::raycast(host.world(),alice.camera().position,alice.camera().forward());ASSERT_TRUE(hit);
        const auto adjacent=vc::voxel::offsetPosition(hit->block,hit->normal);ASSERT_TRUE(adjacent);placed=*adjacent;
        vc::netgame::Hotbar hotbar;hotbar.names[0]="voxelcraft:lamp";alice.hotbar(hotbar);
        vc::netgame::Input input;input.flags=vc::netgame::Active|vc::netgame::Place;input.yaw=0;input.pitch=-1.55;alice.input(input);
        ASSERT_TRUE(settle(host,[&]{return host.world().block(placed)==host.blocks().require("voxelcraft:lamp");},clients));
        std::filesystem::rename(temp.path()/"snapshots",temp.path()/"backup");
        { std::ofstream file(temp.path()/"snapshots");file<<"block snapshot I/O"; }
        alice.requestSave();ASSERT_TRUE(settle(host,[&]{for(const auto& notice:alice.takeNotices()) if(notice.starts_with("Save failed")) return true;return false;},clients));
        EXPECT_EQ(host.playerCount(),1u);EXPECT_TRUE(alice.open());EXPECT_EQ(host.world().block(placed),host.blocks().require("voxelcraft:lamp"));
        std::filesystem::remove(temp.path()/"snapshots");std::filesystem::rename(temp.path()/"backup",temp.path()/"snapshots");
        for(int i=0;i<310;++i) { alice.pump();host.tick(1.0/60);alice.pump(); }
        alice.requestSave();ASSERT_TRUE(settle(host,[&]{for(const auto& notice:alice.takeNotices()) if(notice=="Saved world 2") return true;return false;},clients));
        alice.disconnect();ASSERT_TRUE(settle(host,[&]{return host.playerCount()==0;}));
    }
    auto blocks=vc::builtinBlocks();vc::voxel::WorldStore store(temp.path());const auto archive=store.load(blocks);ASSERT_TRUE(archive);
    EXPECT_EQ(archive->revision,2u);EXPECT_EQ(archive->world.block(placed),blocks.require("voxelcraft:lamp"));
    const auto saved=vc::decodePlayer(archive->gameData);ASSERT_EQ(saved.profiles.size(),2u);EXPECT_EQ(saved.profiles[0].hotbar[0],"voxelcraft:lamp");
}

TEST(ServerIdentity, CredentialsBindSavedProfilesAcrossRestartAndDoNotEnterWorldData) {
    vc::test::TempDirectory temp;createFlyingWorld(temp.path());auto settings=config();settings.worldDirectory=temp.path();
    const std::string aliceToken(64,'a'),bobToken(64,'b');settings.profileTokens={{"alice",aliceToken},{"bob",bobToken}};
    for(int restart=0;restart<2;++restart) {
        vc::server::Host host(settings);ASSERT_TRUE(settle(host,[&]{return host.ready();}));
        for(const std::string& token:{std::string{},bobToken}) {
            auto registry=vc::builtinBlocks();vc::netgame::RemoteSession impostor("127.0.0.1",host.port(),"ALICE",host.fingerprint(),registry,token);
            const std::array clients{&impostor};
            EXPECT_THROW(settle(host,[]{return false;},clients),std::runtime_error);EXPECT_EQ(host.playerCount(),0u);
        }
        auto registry=vc::builtinBlocks();vc::netgame::RemoteSession alice("127.0.0.1",host.port(),"ALICE",host.fingerprint(),registry,aliceToken);
        const std::array clients{&alice};ASSERT_TRUE(settle(host,[&]{return alice.welcome()!=nullptr;},clients));
        EXPECT_EQ(alice.welcome()->profile.name,"Alice");EXPECT_EQ(alice.welcome()->profile.selectedSlot,2u);
        alice.disconnect();ASSERT_TRUE(settle(host,[&]{return host.playerCount()==0;}));host.save();
    }
    vc::voxel::WorldStore store(temp.path());auto registry=vc::builtinBlocks();const auto archive=store.load(registry);ASSERT_TRUE(archive);
    EXPECT_EQ(archive->gameData.find(aliceToken),std::string::npos);EXPECT_EQ(archive->gameData.find(bobToken),std::string::npos);
}
TEST(ServerIdentity, PublicBindRequiresCredentialsOrExplicitTrustedNetwork) {
    auto settings=config();settings.bindAddress="0.0.0.0";
    EXPECT_THROW(vc::server::Host host(settings),std::invalid_argument);
    settings.profileTokens={{"alice","invalid"}};EXPECT_THROW(vc::server::Host host(settings),std::invalid_argument);
    settings.profileTokens.clear();settings.trustedLan=true;EXPECT_NO_THROW(vc::server::Host host(settings));
}
TEST(ServerHost, IdleSessionExpiresAndShutdownExplainsDisconnect) {
    auto settings=config();vc::server::Host host(settings);ASSERT_TRUE(settle(host,[&]{return host.ready();}));
    auto registry=vc::builtinBlocks();vc::netgame::RemoteSession alice("127.0.0.1",host.port(),"Alice",host.fingerprint(),registry);
    const std::array clients{&alice};ASSERT_TRUE(settle(host,[&]{return alice.welcome()!=nullptr;},clients));
    // Read snapshots, but send no input heartbeat for the full bounded idle period.
    bool timedOut=false;
    for(int n=0;n<1850&&!timedOut;++n) {
        host.tick(1.0/60);
        try { alice.pump(); } catch(const std::runtime_error& error) { timedOut=std::string(error.what()).find("timed out")!=std::string::npos; }
    }
    EXPECT_TRUE(timedOut);
    for(int n=0;n<65;++n) host.tick(1.0/60);
    EXPECT_EQ(host.playerCount(),0u);EXPECT_EQ(host.profiles().size(),1u);
    vc::netgame::RemoteSession rejoin("127.0.0.1",host.port(),"Alice",host.fingerprint(),registry);
    const std::array again{&rejoin};ASSERT_TRUE(settle(host,[&]{return rejoin.welcome()!=nullptr;},again));
    host.shutdown();
    try { rejoin.pump();FAIL()<<"Expected explicit shutdown reason"; }
    catch(const std::runtime_error& error) { EXPECT_NE(std::string(error.what()).find("Server shutting down"),std::string::npos); }
    EXPECT_FALSE(rejoin.open());
}
TEST(ServerIdentity, IdentityFilesRejectAmbiguousOrOversizedCredentials) {
    vc::test::TempDirectory temp;const auto file=temp.path()/"identities";const std::string token(64,'a');
    std::ofstream(file)<<"Alice "<<token<<"\nBob "<<std::string(64,'b')<<"\n";
    const auto entries=vc::netgame::readProfileTokens(file);EXPECT_EQ(entries.at("alice"),token);EXPECT_EQ(entries.size(),2u);
    std::ofstream(file)<<token<<"\n";EXPECT_EQ(vc::netgame::readIdentityToken(file),token);
    EXPECT_THROW(vc::netgame::readProfileTokens(file),std::runtime_error);
    std::ofstream(file)<<"Alice "<<token<<"\nALICE "<<token;EXPECT_THROW(vc::netgame::readProfileTokens(file),std::runtime_error);
    std::ofstream(file)<<token<<" extra";EXPECT_THROW(vc::netgame::readIdentityToken(file),std::runtime_error);
    std::ofstream(file)<<std::string(4097,'a');EXPECT_THROW(vc::netgame::readProfileTokens(file),std::runtime_error);
    EXPECT_FALSE(vc::netgame::equalIdentityToken(token,std::string(64,'b')));EXPECT_TRUE(vc::netgame::equalIdentityToken(token,token));
}

TEST(ServerHost, ProtocolMismatchAndCommandFloodDoNotInterruptAnotherPlayer) {
    vc::server::Host host(config());ASSERT_TRUE(settle(host,[&]{return host.ready();}));
    auto aliceBlocks=vc::builtinBlocks(),spamBlocks=vc::builtinBlocks();
    vc::netgame::RemoteSession alice("127.0.0.1",host.port(),"Alice",host.fingerprint(),aliceBlocks);
    const std::array aliceOnly{&alice};ASSERT_TRUE(settle(host,[&]{return alice.welcome()!=nullptr;},aliceOnly));
    vc::net::FramedConnection old(vc::net::TcpStream::connect("127.0.0.1",host.port()));
    auto hello=vc::netgame::encode(vc::netgame::Hello{host.fingerprint(),"Old"});hello[9]=2;
    ASSERT_TRUE(old.queue(hello));old.pump();
    bool rejected=false;
    for(int i=0;i<10&&!rejected;++i) {
        host.tick(1.0/60);old.pump();alice.pump();
        if(auto message=old.take()) {
            EXPECT_EQ(vc::netgame::messageType(*message),vc::netgame::Message::Reject);
            EXPECT_NE(vc::netgame::decodeText(*message,vc::netgame::Message::Reject).find("protocol version"),std::string::npos);rejected=true;
        }
    }
    EXPECT_TRUE(rejected);EXPECT_EQ(host.profiles().size(),1u);
    vc::netgame::RemoteSession spam("127.0.0.1",host.port(),"Spam",host.fingerprint(),spamBlocks);
    const std::array both{&alice,&spam};ASSERT_TRUE(settle(host,[&]{return spam.welcome()!=nullptr;},both));
    for(std::size_t i=0;i<vc::net::maxQueuedFrames;++i) spam.input({});
    spam.pump();host.tick(1.0/60);alice.pump();
    for(int i=0;i<10;++i) { host.tick(1.0/60);alice.pump(); }
    EXPECT_EQ(host.playerCount(),1u);EXPECT_TRUE(alice.open());
    vc::netgame::Input input;input.flags=vc::netgame::Active;alice.input(input);alice.pump();
    ASSERT_TRUE(settle(host,[&]{return alice.state().players.size()==1;},aliceOnly));
}

TEST(ServerCommands, AuthenticatedRolesRealEffectsPrivateStateAndRestart) {
    using P=vc::commands::Permission;using C=vc::commands::Code;
    vc::test::TempDirectory temp;createSurvivalWorld(temp.path());auto settings=config();settings.worldDirectory=temp.path();
    settings.profileTokens={{"alice",std::string(64,'a')},{"bob",std::string(64,'b')}};settings.administrators={"Alice"};
    {
        vc::server::Host host(settings);auto aBlocks=vc::builtinBlocks(),bBlocks=vc::builtinBlocks();
        vc::netgame::RemoteSession alice("127.0.0.1",host.port(),"Alice",host.fingerprint(),aBlocks,std::string(64,'a'));
        vc::netgame::RemoteSession bob("127.0.0.1",host.port(),"Bob",host.fingerprint(),bBlocks,std::string(64,'b'));
        const std::array clients{&alice,&bob};
        ASSERT_TRUE(settle(host,[&]{return alice.state().survival&&bob.state().survival;},clients));
        EXPECT_EQ(alice.state().permission,P::Administrator);EXPECT_EQ(bob.state().permission,P::Player);
        const auto execute=[&](vc::netgame::RemoteSession& client,std::string text) {
            client.command(text);std::vector<vc::netgame::CommandReply> replies;
            EXPECT_TRUE(settle(host,[&]{replies=client.takeCommandReplies();return !replies.empty();},clients));
            return replies.empty()?vc::commands::Result::error(C::InternalFailure,"No reply"):replies.front().result;
        };
        EXPECT_EQ(execute(bob,"/give @s voxelcraft:stone 64").code,C::PermissionDenied);EXPECT_EQ(count(bob,"voxelcraft:stone"),0u);
        EXPECT_TRUE(execute(alice,"/give Bob voxelcraft:stone 64").success());
        ASSERT_TRUE(settle(host,[&]{return count(bob,"voxelcraft:stone")==64;},clients));EXPECT_EQ(count(alice,"voxelcraft:stone"),0u);
        EXPECT_TRUE(execute(alice,"/give @s voxelcraft:wooden_pickaxe").success());
        EXPECT_TRUE(execute(alice,"/gamemode creative").success());
        ASSERT_TRUE(settle(host,[&]{return !alice.state().survival&&bob.state().survival.has_value();},clients));EXPECT_FALSE(alice.welcome()->survival);EXPECT_TRUE(alice.flying());
        EXPECT_TRUE(execute(alice,"/tp Bob 8.5 1 8.5").success());
        ASSERT_TRUE(settle(host,[&]{return glm::length(bob.camera().position-glm::dvec3(8.5,2.62,8.5))<.01;},clients));EXPECT_GT(bob.state().poseRevision,0u);
        EXPECT_TRUE(execute(alice,"/spawnpoint Bob 8.5 1 8.5").success());
        EXPECT_TRUE(execute(alice,"/summon voxelcraft:hostile 12.5 1 12.5").success());
        ASSERT_TRUE(settle(host,[&]{return !bob.state().actors.empty()&&!alice.state().actors.empty();},clients));
        EXPECT_TRUE(host.console("time set night").success());
        EXPECT_TRUE(host.console("op Bob").success());
        ASSERT_TRUE(settle(host,[&]{return bob.state().permission==P::Administrator;},clients));
        // Let command credits refill using simulation time, not wall time.
        for(int i=0;i<120;++i) { host.tick(1.0/60);alice.pump();bob.pump(); }
        EXPECT_TRUE(execute(bob,"/seed").success());EXPECT_EQ(execute(bob,"/op Bob").code,C::PermissionDenied);
        EXPECT_TRUE(host.console("save").success());
        alice.disconnect();bob.disconnect();ASSERT_TRUE(settle(host,[&]{return host.playerCount()==0;}));host.save();
    }
    // Remove bootstrap grants: roles must come from the persisted role file.
    settings.administrators.clear();
    {
        vc::server::Host host(settings);auto aBlocks=vc::builtinBlocks(),bBlocks=vc::builtinBlocks();
        vc::netgame::RemoteSession alice("127.0.0.1",host.port(),"ALICE",host.fingerprint(),aBlocks,std::string(64,'a'));
        vc::netgame::RemoteSession bob("127.0.0.1",host.port(),"Bob",host.fingerprint(),bBlocks,std::string(64,'b'));
        const std::array clients{&alice,&bob};ASSERT_TRUE(settle(host,[&]{return alice.state().tick&&bob.state().survival.has_value();},clients));
        EXPECT_FALSE(alice.welcome()->survival);EXPECT_EQ(bob.state().permission,P::Administrator);EXPECT_EQ(count(bob,"voxelcraft:stone"),64u);
        EXPECT_EQ(alice.state().permission,P::Administrator);
        EXPECT_EQ(alice.welcome()->profile.items[0].identifier,"voxelcraft:wooden_pickaxe");
        ASSERT_TRUE(bob.welcome()->profile.spawnPoint);EXPECT_EQ(*bob.welcome()->profile.spawnPoint,glm::dvec3(8.5,1,8.5));
        EXPECT_TRUE(host.console("deop Bob").success());
        ASSERT_TRUE(settle(host,[&]{return bob.state().permission==P::Player;},clients));
        bob.command("/seed");std::vector<vc::netgame::CommandReply> replies;
        ASSERT_TRUE(settle(host,[&]{replies=bob.takeCommandReplies();return !replies.empty();},clients));EXPECT_EQ(replies.front().result.code,C::PermissionDenied);
        EXPECT_TRUE(host.console("kick Bob \"Integration complete\"").success());
        EXPECT_THROW(settle(host,[]{return false;},clients),std::runtime_error);EXPECT_EQ(host.playerCount(),1u);
    }
}
TEST(ServerCommands, AnonymousNamesCannotInheritRolesAndFloodIsBounded) {
    vc::test::TempDirectory temp;auto settings=config();settings.permissionsFile=temp.path()/"roles.vcp";
    { vc::commands::PermissionStore roles(settings.permissionsFile);roles.set("Alice",vc::commands::Permission::Administrator); }
    vc::server::Host host(settings);auto blocks=vc::builtinBlocks();vc::netgame::RemoteSession alice("127.0.0.1",host.port(),"Alice",host.fingerprint(),blocks);
    const std::array clients{&alice};ASSERT_TRUE(settle(host,[&]{return alice.state().tick!=0;},clients));
    EXPECT_EQ(alice.state().permission,vc::commands::Permission::Player);
    alice.command("/seed");std::vector<vc::netgame::CommandReply> replies;
    ASSERT_TRUE(settle(host,[&]{replies=alice.takeCommandReplies();return !replies.empty();},clients));EXPECT_EQ(replies.front().result.code,vc::commands::Code::PermissionDenied);
    EXPECT_THROW(alice.command(std::string(1025,'a')),std::exception);
    for(int i=0;i<40;++i) alice.command("/list");
    std::size_t rejected=0;ASSERT_TRUE(settle(host,[&]{for(const auto& reply:alice.takeCommandReplies()) if(!reply.result.success()) ++rejected;return rejected>0;},clients));
    EXPECT_TRUE(alice.open());EXPECT_TRUE(host.console("list").success());
    EXPECT_EQ(host.console("op Alice").code,vc::commands::Code::PermissionDenied);
}
TEST(ServerCommands, DuplicateIdsAndOversizeWireTextDisconnectOnlyTheSender) {
    auto settings=config();vc::server::Host host(settings);auto blocks=vc::builtinBlocks();
    vc::netgame::RemoteSession observer("127.0.0.1",host.port(),"Observer",host.fingerprint(),blocks);const std::array clients{&observer};
    ASSERT_TRUE(settle(host,[&]{return observer.welcome()!=nullptr;},clients));
    for(const bool oversized:{false,true}) {
        vc::net::FramedConnection raw(vc::net::TcpStream::connect("127.0.0.1",host.port()));
        ASSERT_TRUE(raw.queue(vc::netgame::encode(vc::netgame::Hello{host.fingerprint(),"Raw"})));raw.pump();
        ASSERT_TRUE(settle(host,[&]{raw.pump();return host.playerCount()==2;},clients));
        while(raw.take()) {}
        if(oversized) {
            vc::binary::Writer out;out.u32(static_cast<std::uint32_t>(vc::netgame::Message::Command));out.u64(1);out.text(std::string(1025,'a'));ASSERT_TRUE(raw.queue(out.bytes));
        } else {
            const auto command=vc::netgame::encode(vc::netgame::CommandRequest{1,"/position"});ASSERT_TRUE(raw.queue(command));ASSERT_TRUE(raw.queue(command));
        }
        raw.pump();ASSERT_TRUE(settle(host,[&]{raw.pump();return host.playerCount()==1;},clients));EXPECT_TRUE(observer.open());
    }
    observer.command("/list");std::vector<vc::netgame::CommandReply> replies;
    ASSERT_TRUE(settle(host,[&]{replies=observer.takeCommandReplies();return !replies.empty();},clients));EXPECT_TRUE(replies.front().result.success());
}
