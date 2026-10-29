#include "engine/core/log.hpp"
#include "engine/core/paths.hpp"
#include "game/content/blocks.hpp"
#include "game/network/client.hpp"
#include "game/player/save_state.hpp"
#include "server/host.hpp"
#include "tests/temp_directory.hpp"
#include <array>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <numbers>
#include <thread>

namespace {
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
struct Bot {
    vc::voxel::BlockRegistry blocks=vc::builtinBlocks();
    std::unique_ptr<vc::netgame::RemoteSession> client;
    std::uint64_t lastTick{};
    unsigned missingTerrain{};
};
void prepare(const std::filesystem::path& path) {
    const auto blocks=vc::builtinBlocks();vc::voxel::World world({0,31});const auto stone=blocks.require("voxelcraft:stone");
    for(int x=-4;x<=4;++x) for(int z=-4;z<=4;++z) {
        vc::voxel::Chunk chunk({x,0,z});
        for(int a=0;a<32;++a) for(int b=0;b<32;++b) chunk.set({a,0,b},stone);
        world.insert(std::move(chunk));
    }
    vc::PlayerSaveState state;state.flying=true;state.camera.position={0.5,4.62,0.5};
    for(unsigned i=0;i<8;++i) {
        const auto phase=2*std::numbers::pi*i/8;vc::PlayerProfile profile;
        profile.name="Soak_"+std::to_string(i);profile.flying=true;profile.camera.position={48*std::cos(phase),4.62,48*std::sin(phase)};
        profile.hotbar[0]="voxelcraft:lamp";state.profiles.push_back(std::move(profile));
    }
    vc::voxel::WorldStore store(path);store.save(world,blocks,{42,{0,31},1},vc::encodePlayer(state));
}
}
int main(int argc,char** argv) {
    try {
        unsigned seconds=60;
        if(argc>2) throw std::invalid_argument("Usage: vc_network_soak [seconds, 10..14400]");
        if(argc==2) {
            const std::string_view value=argv[1];const auto parsed=std::from_chars(value.data(),value.data()+value.size(),seconds);
            if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size()||seconds<10||seconds>14400)
                throw std::invalid_argument("Soak duration must be 10..14400 seconds");
        }
        vc::test::TempDirectory temp;prepare(temp.path());vc::server::HostConfig config;
        config.worldDirectory=temp.path();config.port=0;config.radius=1;config.populate=false;
        for(std::size_t i=0;i<8;++i) config.profileTokens.emplace("soak_"+std::to_string(i),std::string(64,static_cast<char>('0'+i)));
        std::uint64_t commands=0,placed=0,removed=0,churn=0,steps=0;std::size_t peakChunks=0;
        std::vector<vc::PlayerProfile> expected;
        {
            vc::server::Host host(config);std::array<Bot,8> bots;
            const auto connect=[&](std::size_t index) {
                auto& bot=bots[index];bot.client=std::make_unique<vc::netgame::RemoteSession>("127.0.0.1",host.port(),"Soak_"+std::to_string(index),host.fingerprint(),bot.blocks,config.profileTokens.at("soak_"+std::to_string(index)));
                bot.lastTick=0;bot.missingTerrain=0;
            };
            for(std::size_t i=0;i<bots.size();++i) connect(i);
            using Clock=std::chrono::steady_clock;const auto start=Clock::now();auto next=start;
            std::vector<vc::voxel::Position> adding,breaking;
            while(Clock::now()-start<std::chrono::seconds(seconds)) {
                adding.clear();breaking.clear();
                const auto peers=host.players();
                for(std::size_t i=0;i<bots.size();++i) {
                    auto& bot=bots[i];bot.client->pump();require(bot.client->open(),"Soak client unexpectedly disconnected");
                    if(!bot.client->welcome()) continue;
                    const auto& state=bot.client->state();require(state.tick>=bot.lastTick,"Soak state tick moved backward");bot.lastTick=state.tick;
                    vc::validatePlayerPose(bot.client->camera());peakChunks=std::max(peakChunks,bot.client->world().chunks().size());
                    const auto position=bot.client->camera().position;
                    const vc::voxel::Position cell{static_cast<std::int64_t>(std::floor(position.x)),4,static_cast<std::int64_t>(std::floor(position.z))};
                    if(!bot.client->world().loaded(cell)) ++bot.missingTerrain;else bot.missingTerrain=0;
                    require(bot.missingTerrain<1800,"Soak terrain stalled for 30 seconds");
                    const auto phase=2*std::numbers::pi*static_cast<double>(i)/8+static_cast<double>(steps)*0.002;
                    const glm::dvec3 target{48*std::cos(phase),4.62,48*std::sin(phase)};const auto difference=target-position;
                    vc::netgame::Input intent;intent.flags=vc::netgame::Active;intent.yaw=std::atan2(difference.x,-difference.z);
                    intent.forward=std::min(1.0,glm::length(difference)/0.8);
                    const auto edit=(steps+static_cast<std::uint64_t>(i)*15)%120;
                    if(edit<12) {
                        intent.forward=0;intent.yaw=0;intent.pitch=-1.5;
                        if(edit==3) intent.flags|=vc::netgame::Place;
                        if(edit==8) intent.flags|=vc::netgame::Destroy;
                        for(const auto& peer:peers) if(peer.id==bot.client->welcome()->id&&(edit==3||edit==8)) {
                            vc::Camera camera=peer.camera;camera.yaw=0;camera.pitch=-1.5;
                            if(const auto hit=vc::voxel::raycast(host.world(),camera.position,camera.forward())) {
                                if(edit==8&&host.world().block(hit->block)==host.blocks().require("voxelcraft:lamp")) breaking.push_back(hit->block);
                                if(edit==3) if(const auto p=vc::voxel::offsetPosition(hit->block,hit->normal))
                                    if(host.world().block(*p)==vc::voxel::air) adding.push_back(*p);
                            }
                        }
                    }
                    bot.client->input(intent);++commands;bot.client->pump();(void)bot.client->takeNotices();(void)bot.client->takeDirtyChunks();
                }
                host.tick(1.0/60);++steps;
                for(auto& bot:bots) bot.client->pump();
                const auto lamp=host.blocks().require("voxelcraft:lamp");
                for(const auto p:adding) if(host.world().block(p)==lamp) ++placed;
                for(const auto p:breaking) if(host.world().block(p)==vc::voxel::air) ++removed;
                if(steps%900==0) {
                    const auto index=static_cast<std::size_t>((steps/900-1)%bots.size());bots[index].client->disconnect();
                    host.tick(1.0/60);connect(index);++churn;
                }
                if(steps%1800==0) host.save();
                if(steps%600==0) vc::log("Soak ticks "+std::to_string(steps)+" | commands "+std::to_string(commands)+" | edits "+std::to_string(placed+removed)+
                    " | joins "+std::to_string(churn)+" | resident "+std::to_string(host.world().chunks().size()));
                next+=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0/60));
                if(Clock::now()-next>std::chrono::milliseconds(250)) next=Clock::now();
                std::this_thread::sleep_until(next);
            }
            for(auto& bot:bots) bot.client->disconnect();
            host.tick(1.0/60);host.save();expected=host.profiles();
            require(expected.size()==8&&host.playerCount()==0,"Soak lost/duplicated a player profile");
        }
        {
            vc::server::Host reopened(config);const auto profiles=reopened.profiles();require(profiles.size()==expected.size(),"Soak reopen lost saved profiles");
            for(std::size_t i=0;i<profiles.size();++i) require(profiles[i].camera.position==expected[i].camera.position&&profiles[i].hotbar==expected[i].hotbar,
                "Soak reopen changed player pose or hotbar");
        }
        require(placed>0&&removed>0&&commands>steps*6&&peakChunks<=9,"Soak did not exercise bounded eight-player editing");
        vc::log("Network soak passed: "+std::to_string(seconds)+" seconds, "+std::to_string(steps)+" ticks, "+std::to_string(commands)+" commands, "+
            std::to_string(placed)+" placements, "+std::to_string(removed)+" removals, "+std::to_string(churn)+" rejoins; eight profiles saved/reopened");return 0;
    } catch(const std::exception& error) { vc::log(error.what());return 1; }
}
