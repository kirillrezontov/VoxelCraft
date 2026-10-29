#include "server/host.hpp"
#include "engine/core/log.hpp"
#include "engine/core/paths.hpp"
#include "engine/platform/stop_signal.hpp"
#include <charconv>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
struct Options { vc::server::HostConfig host;std::uint64_t ticks{};bool help{}; };
template<class T> T number(std::string_view text) {
    T value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()) throw std::invalid_argument("Invalid server number: "+std::string(text));
    return value;
}
Options parse(int argc,char** argv) {
    Options options;std::filesystem::path directory=vc::utf8Path("worlds/server");options.host.modsDirectory=vc::utf8Path("mods");
    bool noSave=false,worldSpecified=false,noMods=false,modsSpecified=false;
    for(int i=1;i<argc;++i) {
        const std::string_view flag=argv[i];
        const auto value=[&]()->std::string_view { if(++i>=argc) throw std::invalid_argument("Missing value for "+std::string(flag));return argv[i]; };
        if(flag=="--world-dir") { directory=vc::utf8Path(value());worldSpecified=true;if(directory.empty()) throw std::invalid_argument("--world-dir must not be empty"); }
        else if(flag=="--auth-file") options.host.profileTokens=vc::netgame::readProfileTokens(vc::utf8Path(value()));
        else if(flag=="--trusted-lan") options.host.trustedLan=true;
        else if(flag=="--no-save") noSave=true;
        else if(flag=="--mods-dir") { options.host.modsDirectory=vc::utf8Path(value());modsSpecified=true;if(options.host.modsDirectory.empty()) throw std::invalid_argument("--mods-dir must not be empty"); }
        else if(flag=="--no-mods") noMods=true;
        else if(flag=="--seed") options.host.seed=number<std::uint64_t>(value());
        else if(flag=="--min-y") options.host.minY=number<std::int64_t>(value());
        else if(flag=="--max-y") options.host.maxY=number<std::int64_t>(value());
        else if(flag=="--generator-version") options.host.generatorVersion=number<std::uint32_t>(value());
        else if(flag=="--radius") options.host.radius=number<int>(value());
        else if(flag=="--bind") options.host.bindAddress=value();
        else if(flag=="--port") { const auto port=number<unsigned>(value());if(port>65535) throw std::invalid_argument("Server port must be 0..65535");options.host.port=static_cast<std::uint16_t>(port); }
        else if(flag=="--ticks") { options.ticks=number<std::uint64_t>(value());if(!options.ticks) throw std::invalid_argument("--ticks must be positive"); }
        else if(flag=="--no-creatures") options.host.populate=false;
        else if(flag=="--survival") options.host.survival=true;
        else if(flag=="--creative") options.host.survival=false;
        else if(flag=="--help") options.help=true;
        else throw std::invalid_argument("Unknown server option: "+std::string(flag));
    }
    if(noSave&&worldSpecified) throw std::invalid_argument("--world-dir conflicts with --no-save");
    if(noMods&&modsSpecified) throw std::invalid_argument("--mods-dir conflicts with --no-mods");
    if(noMods) options.host.modsDirectory.clear();
    if(!noSave&&(!options.ticks||worldSpecified)) options.host.worldDirectory=directory;
    if(options.host.radius<1||options.host.radius>6) throw std::invalid_argument("--radius must be 1..6");
    if(options.host.generatorVersion&&(*options.host.generatorVersion<1||*options.host.generatorVersion>vc::voxel::terrainGenerationVersion)) throw std::invalid_argument("Unsupported server generator version");
    if(options.host.minY&&options.host.maxY) vc::voxel::WorldBounds{*options.host.minY,*options.host.maxY}.validate();
    return options;
}
int run(const Options& options) {
    vc::StopSignal stop;vc::server::Host host(options.host);std::clog.flush();
    constexpr double dt=1.0/60;using Clock=std::chrono::steady_clock;
    const auto step=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(dt));
    auto next=Clock::now(),diagnostic=next;std::uint64_t completed=0;bool saved=false;
    for(;;) {
        if(stop.requested()) {
            try { (void)host.save();saved=true;break; }
            catch(const std::exception& error) {
                if(options.ticks) throw;
                vc::log(std::string("Shutdown cancelled: save failed; server retains changes in memory. Interrupt again to retry: ")+error.what());stop.reset();
            }
        }
        host.tick(dt);++completed;
        if(options.ticks&&completed>=options.ticks) break;
        const auto now=Clock::now();
        if(now-diagnostic>=std::chrono::seconds(5)) {
            vc::log("Server tick "+std::to_string(host.ticks())+" | players "+std::to_string(host.playerCount())+" | resident chunks "+std::to_string(host.world().chunks().size()));
            diagnostic=now;
        }
        next+=step;if(now-next>std::chrono::milliseconds(250)) next=now;
        std::this_thread::sleep_until(next);
    }
    if(!saved) (void)host.save();
    host.shutdown();
    vc::log("Server clean shutdown after "+std::to_string(completed)+" ticks");return 0;
}
}
int main(int argc,char** argv) {
    try {
        const auto options=parse(argc,argv);
        if(options.help) {
            std::cout<<"VoxelCraft headless server " VC_VERSION_STRING "\n"
                "--world-dir PATH --no-save --seed N --min-y N --max-y N --generator-version 1..3\n"
                "--bind ADDRESS (default 127.0.0.1; use 0.0.0.0 for LAN) --port 0..65535 (default 25565)\n"
                "--auth-file PATH (per-profile credentials) / --trusted-lan (unauthenticated private network)\n"
                "--radius 1..6 --mods-dir PATH --no-mods --no-creatures --ticks N\n"
                "--survival / --creative (saved mode resumes automatically)\n"
                "60 Hz authoritative simulation. Ctrl-C/SIGTERM saves and stops.\n"
                "Bounded runs are temporary unless --world-dir is explicit. No SDL/Vulkan initialization.\n";
            return 0;
        }
        return run(options);
    } catch(const std::exception& error) { vc::log(error.what());return 1; }
}
