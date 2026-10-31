#include "server/host.hpp"
#include "game/commands/permissions.hpp"
#include "engine/core/log.hpp"
#include "engine/core/paths.hpp"
#include "engine/voxel/streaming.hpp"
#include "engine/voxel/persistence.hpp"
#include "game/content/blocks.hpp"
#include "game/gameplay/interaction.hpp"
#include "game/player/controller.hpp"
#include "game/player/save_state.hpp"
#include "game/player/spawn.hpp"
#include "game/simulation/creatures.hpp"
#include "game/simulation/day_cycle.hpp"
#include <algorithm>
#include <cmath>
#include <list>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace vc::server {
namespace {
voxel::Position cell(glm::dvec3 position) {
    return {static_cast<std::int64_t>(std::floor(position.x)),static_cast<std::int64_t>(std::floor(position.y)),static_cast<std::int64_t>(std::floor(position.z))};
}
struct Connection {
    net::FramedConnection channel;
    std::optional<netgame::Hello> hello;
    std::uint64_t id{};
    PlayerProfile* profile{};
    PlayerController player;
    SurvivalState survival;
    netgame::Input input;
    std::uint32_t actions{};
    double idle{},age{},credit{240},closingTime{};
    bool closing{},survivalMode{},authenticated{};
    std::uint64_t poseRevision{},lastCommand{};
    double commandCredit{8};
    std::map<voxel::Position,std::uint64_t> known;
    explicit Connection(net::TcpStream stream):channel(std::move(stream)) {}
};
}
struct Host::Impl {
    HostConfig config;
    voxel::BlockRegistry blocks=builtinBlocks();
    std::unique_ptr<ModRuntime> mods;
    commands::Registry commandRegistry=commands::builtinCommands();
    std::unique_ptr<commands::PermissionStore> permissions;
    ItemRegistry items;
    std::vector<Recipe> recipes;
    std::optional<MiningRules> mining;
    std::uint64_t fingerprint{},tickCount{},nextPlayer{1};
    voxel::WorldMetadata metadata;
    voxel::World world;
    std::unique_ptr<voxel::WorldPersistence> store;
    PlayerSaveState base;
    std::optional<voxel::TerrainGenerator> generator;
    std::unique_ptr<voxel::ChunkStreamer> streamer;
    std::unique_ptr<LivingWorld> living;
    DayCycle day;
    std::unique_ptr<net::TcpListener> listener;
    std::map<std::string,PlayerProfile> savedProfiles;
    std::list<Connection> connections;
    PlayerController anchor;
    bool warmed{},saveRequested{};
    double elapsed{},lastSave{},lastRequestedSave{-100};

    explicit Impl(HostConfig settings):config(std::move(settings)) {
        if(config.radius<1||config.radius>6) throw std::invalid_argument("Server radius must be 1..6 chunks");
        if(config.profileTokens.empty()&&!config.trustedLan&&config.bindAddress!="127.0.0.1"&&config.bindAddress!="::1")
            throw std::invalid_argument("Non-loopback hosting requires --auth-file or explicit --trusted-lan (trusted private networks only)");
        for(const auto& [name,token]:config.profileTokens)
            if(!validPlayerName(name)||name!=playerNameKey(name)||!netgame::validIdentityToken(token))
                throw std::invalid_argument("Invalid configured profile identity");
        if(config.profileTokens.size()>maxSavedProfiles) throw std::invalid_argument("Too many configured profile identities");
        mods=std::make_unique<ModRuntime>(config.modsDirectory,blocks);installContent();
        fingerprint=netgame::contentFingerprint(blocks,items,recipes,*mining,*mods);
        metadata.seed=config.seed.value_or(12345);metadata.bounds={config.minY.value_or(-512),config.maxY.value_or(511)};
        metadata.generatorVersion=config.generatorVersion.value_or(voxel::terrainGenerationVersion);
        bool loaded=false,hasPlayer=false;
        if(config.worldDirectory) {
            store=std::make_unique<voxel::WorldPersistence>(*config.worldDirectory);
            if(auto archive=store->load(blocks)) {
                if(config.seed&&*config.seed!=archive->metadata.seed) throw std::invalid_argument("Server seed differs from the saved world");
                if((config.minY&&*config.minY!=archive->metadata.bounds.minY)||(config.maxY&&*config.maxY!=archive->metadata.bounds.maxY))
                    throw std::invalid_argument("Server height differs from the saved world");
                if(config.generatorVersion&&*config.generatorVersion!=archive->metadata.generatorVersion) throw std::invalid_argument("Server generator differs from the saved world");
                metadata=archive->metadata;world=std::move(archive->world);loaded=true;
                if(!archive->gameData.empty()) { base=decodePlayer(archive->gameData);hasPlayer=true; }
                log("Server loaded world revision "+std::to_string(archive->revision));
            }
        }
        metadata.bounds.validate();
        if(metadata.bounds.minY< -1048576||metadata.bounds.maxY>1048575||metadata.generatorVersion<1||metadata.generatorVersion>voxel::terrainGenerationVersion)
            throw std::invalid_argument("Unsupported server world bounds/generator");
        base.survival=config.survival.value_or(base.survival);
        if(!loaded) world=voxel::World(metadata.bounds);
        mods->restore(base.mods);
        for(const auto& profile:base.profiles) savedProfiles.emplace(playerNameKey(profile.name),profile);
        if(blocks.size()>netgame::maxNetworkBlocks) throw std::runtime_error("Server block palette exceeds 8192 entries");
        installContent();retainHotbarItems(items,base.hotbar);
        for(const auto& [key,profile]:savedProfiles) { (void)key;retainHotbarItems(items,profile.hotbar); }
        auto permissionFile=config.permissionsFile;
        if(permissionFile.empty()&&config.worldDirectory) permissionFile=*config.worldDirectory/"operators.vcp";
        permissions=std::make_unique<commands::PermissionStore>(permissionFile);
        for(const auto& name:config.administrators) {
            if(!validPlayerName(name)||!config.profileTokens.contains(playerNameKey(name))) throw std::invalid_argument("Administrators require configured authenticated profile identities");
            (void)permissions->set(name,commands::Permission::Administrator);
        }
        SurvivalInventory checked;checked.restore(base.items,base.selectedSlot,items);
        for(const auto& [key,profile]:savedProfiles) { (void)key;checked.restore(profile.items,profile.selectedSlot,items); }
        generator.emplace(metadata.seed,metadata.bounds,blocks,metadata.generatorVersion);
        if(!hasPlayer) { base.camera.position=findSpawn(*generator,blocks,&world);base.camera.yaw=0;base.camera.pitch=-0.55; }
        anchor.camera=base.camera;anchor.flying=base.flying;day=DayCycle(base.worldTime);
        living=std::make_unique<LivingWorld>(metadata.seed);living->restore(base.actors,items);
        std::vector<voxel::Position> retained;for(const auto& [p,chunk]:world.chunks()) { (void)chunk;retained.push_back(p); }
        streamer=std::make_unique<voxel::ChunkStreamer>(world,*generator,config.radius);streamer->retain(retained);
        streamer->tick(cell(anchor.camera.position),4);listener=std::make_unique<net::TcpListener>(config.bindAddress,config.port);
        log("Server listening on "+config.bindAddress+':'+std::to_string(listener->port())+"; content "+std::to_string(fingerprint));
    }
    void installContent() {
        items=builtinItems(blocks);recipes=builtinRecipes(items);mods->installItems(items,blocks,recipes);
        mining.emplace(blocks,items);mods->installMining(*mining,blocks,items);
    }
    std::size_t count() const {
        return static_cast<std::size_t>(std::count_if(connections.begin(),connections.end(),[](const auto& c){return c.id&&!c.closing&&c.channel.open();}));
    }
    std::vector<voxel::Aabb> bodies(const Connection* except=nullptr) const {
        std::vector<voxel::Aabb> result;
        for(const auto& connection:connections) if(&connection!=except&&connection.id&&!connection.closing&&connection.channel.open()) result.push_back(connection.player.bounds());
        return result;
    }
    void remember(Connection& connection) {
        if(!connection.profile) return;
        connection.profile->camera=connection.player.camera;connection.profile->flying=connection.player.flying;
        connection.profile->survivalMode=connection.survivalMode;
        if(connection.survivalMode) connection.profile->selectedSlot=static_cast<std::uint32_t>(connection.survival.inventory.selectedSlot());
        connection.profile->health=connection.survival.health();connection.profile->items=connection.survival.inventory.save(items);
    }
    void text(Connection& connection,netgame::Message kind,std::string_view message) {
        if(connection.channel.open()&&!connection.channel.queue(netgame::encodeText(kind,message.substr(0,240)))) connection.channel.close();
    }
    void broadcast(std::string_view message) {
        for(auto& connection:connections) if(connection.id&&!connection.closing) text(connection,netgame::Message::Notice,message);
    }
    void reject(Connection& connection,std::string_view reason) {
        text(connection,netgame::Message::Reject,reason);remember(connection);connection.closing=true;connection.closingTime=0;
    }
    bool clear(const PlayerController& player) const {
        const auto body=player.bounds();
        if(body.min.y<static_cast<double>(metadata.bounds.minY)) return false;
        for(auto y=static_cast<std::int64_t>(std::floor(body.min.y));y<static_cast<std::int64_t>(std::ceil(body.max.y));++y)
            for(auto z=static_cast<std::int64_t>(std::floor(body.min.z));z<static_cast<std::int64_t>(std::ceil(body.max.z));++z)
                for(auto x=static_cast<std::int64_t>(std::floor(body.min.x));x<static_cast<std::int64_t>(std::ceil(body.max.x));++x)
                    if(blocks.definition(streamer->sample({x,y,z})).solid) return false;
        return true;
    }
    void join(Connection& connection) {
        const auto& hello=*connection.hello;
        if(hello.fingerprint!=fingerprint) { reject(connection,"Content mismatch: use the same game/mod content as this server");return; }
        if(count()>=netgame::maxPlayers) { reject(connection,"Server is full (8 players)");return; }
        const auto key=playerNameKey(hello.name);
        for(const auto& other:connections) if(&other!=&connection&&other.id&&other.channel.open()&&other.profile&&playerNameKey(other.profile->name)==key) {
            reject(connection,"That player name is already connected");return;
        }
        if(!config.profileTokens.empty()) {
            const auto credential=config.profileTokens.find(key);
            if(credential==config.profileTokens.end()||!netgame::equalIdentityToken(credential->second,hello.identityToken)) {
                reject(connection,"Player identity rejected");return;
            }
        }
        connection.authenticated=!config.profileTokens.empty();connection.hello->identityToken.clear();
        const auto occupied=bodies(&connection);auto found=savedProfiles.find(key);
        if(found==savedProfiles.end()) {
            if(savedProfiles.size()>=maxSavedProfiles) { reject(connection,"Server saved-profile limit reached");return; }
            PlayerProfile profile;profile.name=hello.name;
            if(savedProfiles.empty()) {
                profile.camera=base.camera;profile.flying=base.flying;profile.hotbar=base.hotbar;profile.selectedSlot=base.selectedSlot;
                profile.health=base.health;profile.items=base.items;
            }
            else { profile.camera.position=findSpawn(*generator,blocks,*streamer,occupied);profile.camera.yaw=0;profile.camera.pitch=-0.55; }
            found=savedProfiles.emplace(key,std::move(profile)).first;
        }
        connection.profile=&found->second;connection.player.camera=connection.profile->camera;connection.player.flying=connection.profile->flying;
        connection.survival.inventory.restore(connection.profile->items,connection.profile->selectedSlot,items);
        connection.survival.restoreHealth(connection.profile->health);
        connection.survivalMode=connection.profile->survivalMode.value_or(base.survival);
        if(connection.survivalMode) connection.player.flying=false;
        const auto body=connection.player.bounds();
        if((!connection.player.flying&&!clear(connection.player))||std::any_of(occupied.begin(),occupied.end(),[&](const auto& other){return body.intersects(other);}))
            connection.player.camera.position=findSpawn(*generator,blocks,*streamer,occupied);
        remember(connection);connection.id=nextPlayer++;connection.idle=0;
        netgame::Welcome welcome;welcome.id=connection.id;welcome.tick=tickCount;welcome.metadata=metadata;welcome.radius=static_cast<std::uint32_t>(config.radius);
        welcome.worldTime=day.seconds();welcome.profile=*connection.profile;welcome.survival=connection.survivalMode;
        for(std::size_t i=0;i<blocks.size();++i) welcome.blocks.push_back(blocks.definition(static_cast<voxel::BlockId>(i)).identifier);
        if(!connection.channel.queue(netgame::encode(welcome))) throw std::runtime_error("Cannot queue server welcome");
        log("Player joined: "+connection.profile->name);broadcast(connection.profile->name+" joined");
    }
    commands::Permission permission(const Connection& connection) const {
        return connection.authenticated&&connection.profile?permissions->get(connection.profile->name):commands::Permission::Player;
    }
    commands::Context commandContext(Connection* issuer=nullptr) {
        commands::Context context;context.permission=issuer?permission(*issuer):commands::Permission::ServerConsole;
        context.issuer=issuer?issuer->profile->name:std::string{};
        context.origin=(issuer?issuer->player.camera.position:anchor.camera.position)-glm::dvec3(0,PlayerController::eyeHeight,0);
        context.world=&world;context.blocks=&blocks;context.items=&items;context.living=living.get();context.day=&day;context.metadata=metadata;
        context.sample=[this](voxel::Position p){return streamer->sample(p);};
        for(auto& connection:connections) if(connection.id&&!connection.closing&&connection.channel.open())
            context.players.push_back({connection.profile->name,&connection.player,&connection.survival,&connection.survivalMode,
                &connection.profile->hotbar,&connection.profile->selectedSlot,&connection.profile->spawnPoint,&connection.poseRevision,permission(connection)});
        context.save=[this] {
            if(!store) return commands::Result::error(commands::Code::Unavailable,"Temporary world: saving disabled");
            if(store->saving()) return commands::Result::error(commands::Code::Unavailable,"Save already in progress");
            if(elapsed-lastRequestedSave<5) return commands::Result::error(commands::Code::Unavailable,"Save requested too recently");
            saveRequested=true;lastRequestedSave=elapsed;return commands::Result::ok("World snapshot queued");
        };
        context.broadcast=[this](std::string_view text){broadcast(text);};
        context.kick=[this](std::string_view name,std::string_view reason) {
            for(auto& connection:connections) if(connection.id&&!connection.closing&&connection.channel.open()&&playerNameKey(connection.profile->name)==playerNameKey(name)) {
                reject(connection,reason);log("Operator kicked profile: "+connection.profile->name);return commands::Result::ok("Disconnected "+connection.profile->name);
            }
            return commands::Result::error(commands::Code::InvalidTarget,"Player is no longer connected");
        };
        context.setPermission=[this](std::string_view name,commands::Permission role) {
            if(!validPlayerName(name)||!config.profileTokens.contains(playerNameKey(name)))
                return commands::Result::error(commands::Code::PermissionDenied,"Operator grants require a configured authenticated profile");
            (void)permissions->set(name,role);log("Operator permission changed: "+playerNameKey(name)+" -> "+std::string(commands::permissionName(role)));
            return commands::Result::ok("Permission for "+std::string(name)+": "+std::string(commands::permissionName(role)));
        };
        return context;
    }
    void audit(std::string_view text,std::string_view issuer,const commands::Result& result) {
        // Log operation/issuer/outcome only, never arbitrary command arguments.
        std::string name="invalid";try { if(const auto* definition=commandRegistry.find(commands::parse(text).name)) name=definition->name; } catch(...) {}
        log("Command "+std::string(issuer.empty()?"server-console":issuer)+" /"+name+" -> "+(result.success()?"success":"rejected"));
    }
    void handle(Connection& connection,const binary::Bytes& message) {
        const auto type=netgame::messageType(message);
        if(!connection.id) {
            if(type!=netgame::Message::Hello||connection.hello) throw std::runtime_error("Expected a single client hello");
            connection.hello=netgame::decodeHello(message);if(warmed) join(connection);return;
        }
        switch(type) {
        case netgame::Message::Input: {
            const auto input=netgame::decodeInput(message);
            if(input.sequence<=connection.input.sequence) throw std::runtime_error("Input sequence must increase");
            connection.input=input;connection.profile->selectedSlot=input.slot;connection.idle=0;
            if(connection.survivalMode) connection.survival.inventory.select(input.slot);
            connection.actions|=input.flags&(netgame::Jump|netgame::ToggleFlight|netgame::Destroy|netgame::Place|netgame::Respawn);break;
        }
        case netgame::Message::Hotbar: {
            if(connection.survivalMode) throw std::runtime_error("Creative hotbar assignment is unavailable in survival");
            const auto hotbar=netgame::decodeHotbar(message);
            for(const auto& name:hotbar.names) if(!name.empty()&&!items.find(name)) throw std::runtime_error("Hotbar contains an unknown block");
            connection.profile->hotbar=hotbar.names;connection.profile->selectedSlot=hotbar.slot;break;
        }
        case netgame::Message::Inventory: {
            const auto action=netgame::decodeInventory(message);
            if(!connection.survivalMode) throw std::runtime_error("Survival inventory command in a creative world");
            if(!connection.survival.alive()) break;
            const auto result=applyInventoryAction(connection.survival.inventory,action,items,recipes);connection.survival.cancelMining();
            if(action.kind==InventoryActionKind::Craft) {
                if(result==InventoryActionResult::Changed) {
                    dispatch({ModEventKind::Craft,cell(connection.player.camera.position),items.definition(recipes.at(action.recipe).output).identifier},connection.player,&connection.survival);
                    text(connection,netgame::Message::Notice,"Crafted "+recipes.at(action.recipe).name);
                } else text(connection,netgame::Message::Notice,result==InventoryActionResult::NoSpace?"Inventory full":"Missing materials");
            }
            break;
        }
        case netgame::Message::Command: {
            const auto request=netgame::decodeCommand(message);
            if(request.id<=connection.lastCommand) throw std::runtime_error("Command request IDs must increase");
            connection.lastCommand=request.id;
            commands::Result result;
            if(connection.commandCredit<1) result=commands::Result::error(commands::Code::Unavailable,"Command rate limit exceeded; wait before retrying");
            else {
                --connection.commandCredit;auto context=commandContext(&connection);result=commandRegistry.execute(request.text,context);
                audit(request.text,connection.profile->name,result);
            }
            if(!connection.channel.queue(netgame::encode(netgame::CommandReply{request.id,std::move(result)}))) connection.channel.close();
            break;
        }
        case netgame::Message::Save:
            netgame::decodeControl(message,type);
            if(elapsed-lastRequestedSave>=5) { saveRequested=true;lastRequestedSave=elapsed; }
            else text(connection,netgame::Message::Notice,"Save requested too recently");
            break;
        case netgame::Message::Goodbye: netgame::decodeControl(message,type);remember(connection);connection.channel.close();break;
        default: throw std::runtime_error("Packet is not a client command");
        }
    }
    void receive(double seconds) {
        for(int n=0;n<4;++n) {
            auto socket=listener->accept();if(!socket) break;
            if(connections.size()>=16) socket->close();else connections.emplace_back(std::move(*socket));
        }
        for(auto it=connections.begin();it!=connections.end();) {
            auto& connection=*it;connection.age+=seconds;connection.idle+=seconds;connection.credit=std::min(240.0,connection.credit+120*seconds);connection.commandCredit=std::min(8.0,connection.commandCredit+4*seconds);
            try {
                connection.channel.pump();
                if(!connection.closing) while(auto message=connection.channel.take()) {
                    if(connection.credit<1) throw std::runtime_error("Client command rate exceeded");
                    --connection.credit;handle(connection,*message);if(connection.closing||!connection.channel.open()) break;
                }
                if(warmed&&!connection.id&&!connection.closing&&connection.hello) join(connection);
                if(connection.id&&connection.idle>30&&!connection.closing) reject(connection,"Player input timed out");
                if(warmed&&!connection.id&&connection.age>10&&!connection.closing) reject(connection,"Join handshake timed out");
                if(connection.closing) { connection.closingTime+=seconds;if(!connection.channel.queuedBytes()||connection.closingTime>1) connection.channel.close(); }
            } catch(const std::exception& error) {
                log(std::string("Rejected peer: ")+error.what());
                if(connection.id||connection.closing||!connection.channel.open()) connection.channel.close();
                else try { reject(connection,error.what()); } catch(...) { connection.channel.close(); }
            }
            if(!connection.channel.open()) {
                remember(connection);if(connection.id&&connection.profile) { log("Player left: "+connection.profile->name);broadcast(connection.profile->name+" left"); }
                it=connections.erase(it);
            } else ++it;
        }
    }
    void dispatch(const ModEvent& event,const PlayerController& player,SurvivalState* survival=nullptr) {
        const auto occupied=bodies();
        for(const auto& message:mods->dispatch(event,{world,blocks,items,survival,player.bounds(),occupied})) { log(message);broadcast(message); }
        for(const auto& message:mods->takeDiagnostics()) { log(message);broadcast(message); }
    }
    void simulatePlayer(Connection& connection,double seconds) {
        auto& player=connection.player;auto& survival=connection.survival;
        const auto actions=std::exchange(connection.actions,0u);
        if(connection.survivalMode&&!survival.alive()) {
            survival.cancelMining();
            if(actions&netgame::Respawn) {
                const auto occupied=bodies(&connection);
                try {
                    auto spawn=findSpawn(*generator,blocks,*streamer,occupied);
                    if(connection.profile->spawnPoint) {
                        auto context=commandContext(&connection);
                        if(commands::safeFeet(context,*connection.profile->spawnPoint)) spawn=*connection.profile->spawnPoint+glm::dvec3(0,PlayerController::eyeHeight,0);
                    }
                    player=PlayerController{};player.camera.position=spawn;player.camera.yaw=0;player.camera.pitch=-0.55;
                    ++connection.poseRevision;survival.respawn();connection.input.flags=0;connection.input.forward=connection.input.right=connection.input.up=0;
                    text(connection,netgame::Message::Notice,"Respawned - inventory kept");
                } catch(const std::exception& error) { log(error.what());text(connection,netgame::Message::Notice,"No safe spawn available"); }
            }
            return;
        }
        const bool active=(connection.input.flags&netgame::Active)&&connection.idle<2;
        if(active) { player.camera.yaw=connection.input.yaw;player.camera.pitch=connection.input.pitch; }
        const CameraInput movement{active?static_cast<float>(connection.input.forward):0,active?static_cast<float>(connection.input.right):0,
            active?static_cast<float>(connection.input.up):0,0,0};
        player.update({movement,active&&(actions&netgame::Jump),active&&!connection.survivalMode&&(actions&netgame::ToggleFlight),active},seconds,world,blocks);
        validatePlayerPose(player.camera);
        if(connection.survivalMode&&survival.land(player.landingSpeed)) text(connection,netgame::Message::Notice,survival.alive()?"Fall damage":"You died - R to respawn");
        const bool playing=active&&(!connection.survivalMode||survival.alive());
        auto* subject=connection.survivalMode?&survival:nullptr;
        const auto actor=living->target(world,blocks,player.camera.position,player.camera.forward());
        if(playing&&actor&&(connection.input.flags&netgame::AttackHeld)) {
            const auto result=living->attack(*actor,player.camera.position,subject,items,connection.id);
            if(result.hit) text(connection,netgame::Message::Notice,result.killed?"Creature defeated - collect loot":"Hit creature");
            if(result.noDropSpace) text(connection,netgame::Message::Notice,"Loot limit - collect existing drops");
        }
        const auto hit=voxel::raycast(world,player.camera.position,player.camera.forward(),6,connection.survivalMode?&blocks:nullptr);
        bool changed=false;
        const auto broken=hit?blocks.definition(world.block(hit->block)).identifier:std::string{};
        if(connection.survivalMode) {
            const auto result=survival.mine(playing&&!actor&&(connection.input.flags&netgame::AttackHeld),hit,seconds,world,items,*mining);
            changed=result.changed;
            if(result.inventoryFull) text(connection,netgame::Message::Notice,"Inventory full - mining stopped");
            if(result.toolBroke) text(connection,netgame::Message::Notice,"Tool broke");
        } else if(playing&&hit&&!actor&&(actions&netgame::Destroy)) changed=destroyBlock(world,*hit);
        if(changed&&hit) dispatch({ModEventKind::Break,hit->block,broken},player,subject);
        const bool ate=connection.survivalMode&&playing&&(actions&netgame::Place)&&survival.eat(items);
        if(ate) text(connection,netgame::Message::Notice,"Ate food - healed 3");
        if(!playing||changed||ate||!hit||actor||!(actions&netgame::Place)||(connection.survivalMode&&(connection.input.flags&netgame::AttackHeld))) return;
        std::optional<voxel::BlockId> block;
        if(connection.survivalMode) {
            const auto& stack=survival.inventory.selected();if(!stack.empty()) block=items.definition(stack.item).block;
        } else block=blocks.find(connection.profile->hotbar[connection.profile->selectedSlot]);
        const auto p=voxel::offsetPosition(hit->block,hit->normal);if(!block||!p) return;
        const glm::dvec3 low{static_cast<double>(p->x),static_cast<double>(p->y),static_cast<double>(p->z)};const voxel::Aabb cellBox{low,low+1.0};
        const auto occupied=bodies(&connection);
        if(std::any_of(occupied.begin(),occupied.end(),[&](const auto& body){return body.intersects(cellBox);})) return;
        if(placeBlock(world,blocks,*hit,*block,player.bounds())) {
            if(connection.survivalMode) survival.inventory.consumeSelected();
            dispatch({ModEventKind::Place,*p,blocks.definition(*block).identifier},player,subject);
        }
    }
    void simulate(double seconds) {
        std::vector<voxel::Position> centers;
        for(const auto& connection:connections) if(connection.id&&!connection.closing&&connection.channel.open()) centers.push_back(cell(connection.player.camera.position));
        if(centers.empty()) centers.push_back(cell(anchor.camera.position));
        streamer->tick(centers,4);
        if(!warmed) {
            if(!streamer->settled()) { (void)world.takeDirtyChunks();return; }
            dispatch({ModEventKind::Load},anchor);warmed=true;log("Server world ready");
        }
        day.advance(seconds);const PlayerController* activeAnchor=&anchor;SurvivalState* activeSurvival=nullptr;
        std::vector<LivingPlayer> livingPlayers;
        std::map<std::uint64_t,unsigned> healthBefore;
        for(auto& connection:connections) {
            if(!connection.id||connection.closing||!connection.channel.open()) continue;
            if(activeAnchor==&anchor) { activeAnchor=&connection.player;activeSurvival=connection.survivalMode?&connection.survival:nullptr; }
            simulatePlayer(connection,seconds);
            livingPlayers.push_back({connection.player.camera.position,connection.survivalMode?&connection.survival:nullptr});
            healthBefore[connection.id]=connection.survival.health();
        }
        dispatch({ModEventKind::Tick,{}, {},seconds},*activeAnchor,activeSurvival);
        (void)living->update(seconds,world,blocks,livingPlayers,items,day.daylight().sunlight<0.12f,config.populate);
        for(auto& connection:connections) if(connection.survivalMode&&connection.id&&!connection.closing&&connection.channel.open()) {
            if(connection.survival.health()<healthBefore[connection.id]) text(connection,netgame::Message::Notice,connection.survival.alive()?"Creature attack":"You died - R to respawn");
            if(!connection.survival.alive()) connection.survival.cancelMining();
        }
        (void)world.takeDirtyChunks();
    }
    std::vector<netgame::Peer> players() const {
        std::vector<netgame::Peer> peers;
        for(const auto& connection:connections) if(connection.id&&!connection.closing&&connection.channel.open())
            peers.push_back({connection.id,connection.profile->name,connection.player.camera,connection.player.flying,connection.survivalMode?connection.survival.health():20u});
        return peers;
    }
    void send() {
        netgame::State state;state.tick=tickCount;state.worldTime=day.seconds();state.players=players();
        if(tickCount%3==0) state.actors=living->save(items);
        binary::Bytes snapshot;
        for(auto& connection:connections) {
            if(!connection.id||connection.closing||!connection.channel.open()) continue;
            try {
                if(connection.channel.queuedBytes()>4*1024*1024) { reject(connection,"Connection is too slow for world streaming");continue; }
                const auto region=voxel::streamingRegion(cell(connection.player.camera.position),metadata.bounds,config.radius,metadata.generatorVersion);
                const std::set<voxel::Position> wanted(region.begin(),region.end());
                for(auto it=connection.known.begin();it!=connection.known.end();) {
                    if(!wanted.contains(it->first)||!world.findChunk(it->first)) {
                        if(!connection.channel.queue(netgame::encodeUnload(it->first))) break;
                        it=connection.known.erase(it);
                    } else ++it;
                }
                std::size_t sent=0;
                for(const auto p:region) {
                    const auto* chunk=world.findChunk(p);if(!chunk) continue;
                    const auto known=connection.known.find(p);
                    if(known!=connection.known.end()&&known->second==chunk->revision()) continue;
                    if(!connection.channel.queue(netgame::encodeChunk(*chunk,chunk->revision(),blocks.size()))) break;
                    connection.known[p]=chunk->revision();if(++sent>=2) break;
                }
                state.survival.reset();
                if(tickCount%3==0&&connection.survivalMode) {
                    auto& s=state.survival.emplace();s.health=connection.survival.health();
                    s.selectedSlot=static_cast<std::uint32_t>(connection.survival.inventory.selectedSlot());s.items=connection.survival.inventory.save(items);
                    s.miningProgress=connection.survival.miningProgress();s.mining=connection.survival.mining();
                }
                if(tickCount%3==0) {
                    state.hotbar=connection.profile->hotbar;state.selectedSlot=connection.profile->selectedSlot;
                    state.poseRevision=connection.poseRevision;state.permission=permission(connection);snapshot=netgame::encode(state);
                } else snapshot.clear();
                if(!snapshot.empty()&&!connection.channel.queue(snapshot)) reject(connection,"Connection state queue exceeded");
                connection.channel.pump();
            } catch(const std::exception& error) { log(std::string("Peer transport failed: ")+error.what());connection.channel.close(); }
        }
    }
    std::string snapshotData() {
        for(auto& connection:connections) remember(connection);
        auto state=base;state.worldTime=day.seconds();state.actors=living->save(items);state.mods=mods->save();state.profiles.clear();
        for(const auto& [key,profile]:savedProfiles) { (void)key;state.profiles.push_back(profile); }
        return encodePlayer(state);
    }
    void completed(const voxel::SnapshotResult& result) {
        result.rethrowIfFailed();lastSave=elapsed;
        log("Server saved world revision "+std::to_string(result.revision)+" in "+std::to_string(result.milliseconds)+" ms to "+pathText(store->directory()));
    }
    void pollSave() {
        if(!store) return;
        if(const auto result=store->takeSaveResult()) {
            try { completed(*result);broadcast("Saved world "+std::to_string(result->revision)); }
            catch(const std::exception& error) {
                log(std::string("Server save failed; changes remain in memory: ")+error.what());
                broadcast("Save failed - server retains changes in memory");lastSave=elapsed;
            }
        }
    }
    void requestSave() {
        if(!store) { broadcast("Temporary server - saving disabled");return; }
        if(store->saving()) { broadcast("Save already in progress");return; }
        const auto data=snapshotData();
        if(!store->requestSave(streamer->snapshot(),blocks,metadata,data)) throw std::logic_error("Server snapshot unexpectedly busy");
        lastSave=elapsed;broadcast("Saving world");
    }
    std::uint64_t save() {
        if(!store) return 0;
        if(store->saving()) {
            const auto previous=store->waitForSave();
            try { completed(previous); }
            catch(const std::exception& error) { log(std::string("Previous asynchronous save failed; retrying current snapshot: ")+error.what()); }
        }
        const auto data=snapshotData();
        if(!store->requestSave(streamer->snapshot(),blocks,metadata,data)) throw std::logic_error("Server final snapshot unexpectedly busy");
        const auto result=store->waitForSave();completed(result);return result.revision;
    }
    void tick(double seconds) {
        if(!std::isfinite(seconds)||seconds<=0||seconds>0.1) throw std::invalid_argument("Server tick must be finite and within (0,0.1]");
        elapsed+=seconds;++tickCount;pollSave();receive(seconds);simulate(seconds);send();
        if(saveRequested||(store&&elapsed-lastSave>=60)) {
            saveRequested=false;
            try { requestSave(); }
            catch(const std::exception& error) { log(std::string("Server save failed; changes remain in memory: ")+error.what());broadcast("Save failed - server retains changes in memory");lastSave=elapsed; }
        }
    }
};
Host::Host(HostConfig config):impl_(std::make_unique<Impl>(std::move(config))) {}
Host::~Host()=default;
commands::Result Host::console(std::string_view command) {
    auto context=impl_->commandContext();auto result=impl_->commandRegistry.execute(command,context);impl_->audit(command,{},result);return result;
}
void Host::shutdown() {
    // One bounded nonblocking flush. A slow peer cannot hold up a saved shutdown.
    for(auto& connection:impl_->connections) if(connection.id&&connection.channel.open()) {
        try {
            impl_->text(connection,netgame::Message::Goodbye,"Server shutting down");
            connection.channel.pump();
        } catch(...) {}
        connection.channel.close();
    }
}
void Host::tick(double seconds) { impl_->tick(seconds); }
std::uint64_t Host::save() { return impl_->save(); }
bool Host::ready() const { return impl_->warmed; }
std::uint16_t Host::port() const { return impl_->listener->port(); }
std::size_t Host::playerCount() const { return impl_->count(); }
std::uint64_t Host::ticks() const { return impl_->tickCount; }
const voxel::World& Host::world() const { return impl_->world; }
const voxel::BlockRegistry& Host::blocks() const { return impl_->blocks; }
std::vector<PlayerProfile> Host::profiles() const {
    std::vector<PlayerProfile> result;for(const auto& [name,profile]:impl_->savedProfiles) { (void)name;result.push_back(profile); }return result;
}
std::vector<netgame::Peer> Host::players() const { return impl_->players(); }
std::uint64_t Host::fingerprint() const { return impl_->fingerprint; }
}
