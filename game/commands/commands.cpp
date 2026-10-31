#include "game/commands/commands.hpp"
#include "game/player/profile.hpp"
#include "engine/core/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <set>

namespace vc::commands {
namespace {
bool commandName(std::string_view name) {
    if(name=="?") return true;
    return !name.empty()&&name.size()<=64&&std::all_of(name.begin(),name.end(),[](char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c==':'||c=='_'||c=='-';});
}
std::string position(glm::dvec3 value) { std::ostringstream out;out<<std::fixed<<std::setprecision(2)<<value.x<<" "<<value.y<<" "<<value.z;return out.str(); }
glm::dvec3 feet(const Player& player) { return player.controller->camera.position-glm::dvec3(0,PlayerController::eyeHeight,0); }
glm::dvec3 destination(std::span<const std::string> values,glm::dvec3 origin) {
    return {coordinate(values[0],origin.x),coordinate(values[1],origin.y),coordinate(values[2],origin.z)};
}
void requireWorld(const Context& context) {
    if(!context.world||!context.blocks||!context.items||!context.living||!context.day) throw Error(Code::Unavailable,"World is not ready");
}
std::string join(std::span<const std::string> arguments,std::string_view separator=" ") {
    std::string result;
    for(const auto& word:arguments) { if(!result.empty()) result+=separator;result+=word; }
    return result;
}
}
Player& target(Context& context,std::string_view name) {
    if(name=="@s") {
        if(context.issuer.empty()) throw Error(Code::InvalidTarget,"Server console has no @s player; specify a name");
        name=context.issuer;
    }
    if(!validPlayerName(name)) throw Error(Code::InvalidTarget,"Invalid player name or unsupported selector");
    const auto key=playerNameKey(name);
    for(auto& player:context.players) if(playerNameKey(player.name)==key&&player.controller&&player.survival&&player.survivalMode) return player;
    throw Error(Code::InvalidTarget,"Player is not connected");
}
bool safeFeet(const Context& context,glm::dvec3 position) {
    if(!context.world||!context.blocks) return false;
    for(int axis=0;axis<3;++axis) if(!std::isfinite(position[axis])||std::abs(position[axis])>1e15-16) return false;
    if(position.y<static_cast<double>(context.world->bounds().minY)||position.y+PlayerController::height>static_cast<double>(context.world->bounds().maxY)+1) return false;
    const glm::dvec3 low=position-glm::dvec3(PlayerController::halfWidth,0,PlayerController::halfWidth),
        high=position+glm::dvec3(PlayerController::halfWidth,PlayerController::height,PlayerController::halfWidth);
    for(auto y=static_cast<std::int64_t>(std::floor(low.y));y<static_cast<std::int64_t>(std::ceil(high.y));++y)
        for(auto x=static_cast<std::int64_t>(std::floor(low.x));x<static_cast<std::int64_t>(std::ceil(high.x));++x)
            for(auto z=static_cast<std::int64_t>(std::floor(low.z));z<static_cast<std::int64_t>(std::ceil(high.z));++z) {
                const voxel::Position cell{x,y,z};
                if(!context.world->loaded(cell)&&!context.sample) return false;
                const auto block=context.sample?context.sample(cell):context.world->block(cell);
                if(context.blocks->definition(block).solid) return false;
            }
    return true;
}
void teleport(Player& player,glm::dvec3 position) {
    auto camera=player.controller->camera;const bool flying=player.controller->flying&&!*player.survivalMode;
    *player.controller=PlayerController{};player.controller->camera=camera;
    player.controller->camera.position=position+glm::dvec3(0,PlayerController::eyeHeight,0);player.controller->flying=flying;
    player.survival->cancelMining();if(player.poseRevision) ++*player.poseRevision;
}
void Registry::add(Definition definition) {
    if(!commandName(definition.name)||!definition.execute||definition.minimum>definition.maximum||definition.maximum>maxArguments)
        throw std::invalid_argument("Invalid command definition");
    if(find(definition.name)) throw std::invalid_argument("Duplicate command name");
    std::set<std::string> uniqueAliases;
    for(const auto& alias:definition.aliases) if(!commandName(alias)||find(alias)||alias==definition.name||!uniqueAliases.insert(alias).second) throw std::invalid_argument("Invalid or duplicate command alias");
    const auto name=definition.name;
    for(const auto& alias:definition.aliases) aliases_.emplace(alias,name);
    definitions_.emplace(name,std::move(definition));
}
const Definition* Registry::find(std::string_view name) const {
    if(const auto alias=aliases_.find(std::string(name));alias!=aliases_.end()) name=alias->second;
    const auto found=definitions_.find(std::string(name));return found==definitions_.end()?nullptr:&found->second;
}
Result Registry::execute(std::string_view text,Context& context) const noexcept {
    try {
        const auto parsed=parse(text);const auto* command=find(parsed.name);
        if(!command) return Result::error(Code::UnknownCommand,"Unknown command; use /help");
        if(context.permission<command->permission) return Result::error(Code::PermissionDenied,"Permission denied: requires "+std::string(permissionName(command->permission)));
        if(command->available&&!command->available(context)) return Result::error(Code::Unavailable,"Command is unavailable in this context");
        if(parsed.arguments.size()<command->minimum) return Result::error(Code::MissingArgument,"Usage: "+command->usage);
        if(parsed.arguments.size()>command->maximum) return Result::error(Code::InvalidArgument,"Usage: "+command->usage);
        context.registry=this;auto result=command->execute(context,parsed.arguments);
        if(result.lines.size()>maxResultLines) result.lines.resize(maxResultLines);
        for(auto& line:result.lines) line=utf8::singleLine(line,maxResultBytes);
        return result;
    } catch(const Error& error) { return Result::error(error.code,error.what()); }
    catch(const std::invalid_argument& error) { return Result::error(Code::InvalidArgument,error.what()); }
    catch(const std::exception&) { return Result::error(Code::InternalFailure,"Command failed internally; state was retained where possible"); }
    catch(...) { return Result::error(Code::InternalFailure,"Unknown command failure"); }
}
Registry builtinCommands() {
    using K=ArgumentKind;using P=Permission;
    Registry registry;
    registry.add({"help",{"?"},"Show permitted commands and syntax","/help [command]",P::Player,{{"command",K::Text,true}},0,1,
        [](Context& c,std::span<const std::string> a) {
            Result result;
            for(const auto& [name,d]:c.registry->definitions()) {
                if(c.permission<d.permission||(d.available&&!d.available(c))) continue;
                if(!a.empty()&&c.registry->find(a[0])!=&d) continue;
                result.lines.push_back(d.usage+" — "+d.description+" ["+std::string(permissionName(d.permission))+"]");
            }
            if(result.lines.empty()) return Result::error(Code::UnknownCommand,"Command is unknown or not permitted");
            return result;
        }});
    registry.add({"gamemode",{"gm"},"Change a player's mode without discarding items","/gamemode <survival|creative> [player]",P::Administrator,
        {{"mode",K::Mode},{"player",K::Player,true}},1,2,[](Context& c,std::span<const std::string> a) {
            if(a[0]!="survival"&&a[0]!="creative") throw Error(Code::InvalidArgument,"Game mode must be survival or creative");
            auto& player=target(c,a.size()==2?a[1]:"@s");const bool survival=a[0]=="survival";
            if(survival&&!safeFeet(c,feet(player))) throw Error(Code::InvalidArgument,"Move to a clear, valid position before entering survival");
            *player.survivalMode=survival;
            const auto camera=player.controller->camera;*player.controller=PlayerController{};player.controller->camera=camera;player.controller->flying=!survival;
            player.survival->cancelMining();if(!survival&&!player.survival->alive()) player.survival->respawn();
            if(player.poseRevision) ++*player.poseRevision;
            return Result::ok(player.name+" is now in "+a[0]+" mode; counted inventory preserved");
        }});
    registry.add({"give",{},"Add registered items transactionally","/give <player|@s> <item> [count]",P::Administrator,
        {{"player",K::Player},{"item",K::Item},{"count",K::Number,true}},2,3,[](Context& c,std::span<const std::string> a) {
            requireWorld(c);auto& player=target(c,a[0]);const auto item=c.items->find(a[1]);
            if(!item) throw Error(Code::InvalidArgument,"Unknown item identifier");
            const auto count=a.size()==3?unsignedNumber(a[2],4096):1;
            if(!count) throw Error(Code::InvalidArgument,"Item count must be 1..4096");
            if(!player.survival->inventory.add(*item,static_cast<std::uint16_t>(count),*c.items)) return Result::error(Code::Unavailable,"Inventory full; nothing was added or removed");
            if(!*player.survivalMode&&player.hotbar&&player.selectedSlot) (*player.hotbar)[*player.selectedSlot]=c.items->definition(*item).identifier;
            return Result::ok("Gave "+std::to_string(count)+" "+a[1]+" to "+player.name);
        }});
    registry.add({"tp",{"teleport"},"Teleport safely using feet coordinates and ~ offsets","/tp <x y z> | <player> <x y z> | <player> <targetPlayer>",P::Administrator,
        {{"player or x",K::Player},{"coordinate",K::Coordinate},{"coordinate",K::Coordinate,true},{"coordinate",K::Coordinate,true}},2,4,
        [](Context& c,std::span<const std::string> a) {
            requireWorld(c);Player* player=nullptr;glm::dvec3 position;
            if(a.size()==2) { player=&target(c,a[0]);position=feet(target(c,a[1])); }
            else if(a.size()==3) { player=&target(c,"@s");position=destination(a,c.origin); }
            else { player=&target(c,a[0]);position=destination(a.subspan(1),c.origin); }
            if(!safeFeet(c,position)) throw Error(Code::InvalidArgument,"Teleport destination is obstructed, unavailable or outside world bounds");
            teleport(*player,position);return Result::ok("Teleported "+player->name+" to "+commands::position(position));
        }});
    registry.add({"summon",{},"Spawn an existing creature with normal limits","/summon <entity> [x y z]",P::Administrator,
        {{"entity",K::Entity},{"x",K::Coordinate,true},{"y",K::Coordinate,true},{"z",K::Coordinate,true}},1,4,
        [](Context& c,std::span<const std::string> a) {
            requireWorld(c);if(a.size()!=1&&a.size()!=4) throw Error(Code::MissingArgument,"Supply all three spawn coordinates");
            const auto* entity=findCreature(a[0]);if(!entity) throw Error(Code::InvalidArgument,"Unknown entity identifier");
            const auto p=a.size()==4?destination(a.subspan(1),c.origin):c.origin;
            if(!safeFeet(c,p)) throw Error(Code::InvalidArgument,"Entity spawn is obstructed, unavailable or outside world bounds");
            const auto id=c.living->spawn(entity->kind,p);if(!id) return Result::error(Code::Unavailable,"Creature population limit reached");
            return Result::ok("Summoned "+std::string(entity->identifier)+" #"+std::to_string(*id)+" at "+position(p));
        }});
    registry.add({"time",{},"Read or change the real simulation clock","/time query | set <day|night|ticks>",P::Administrator,
        {{"query or set",K::Time},{"value",K::Time,true}},1,2,[](Context& c,std::span<const std::string> a) {
            requireWorld(c);
            if(a[0]=="query"&&a.size()==1) return Result::ok("Time "+std::to_string(static_cast<std::uint64_t>(c.day->seconds()*20))+" ticks ("+c.day->clock()+")");
            if(a[0]!="set"||a.size()!=2) throw Error(Code::InvalidArgument,"Usage: /time query | set <day|night|ticks>");
            const auto ticks=a[1]=="day"?6000:a[1]=="night"?18000:unsignedNumber(a[1],UINT64_MAX)%24000;
            *c.day=DayCycle(static_cast<double>(ticks)/20);return Result::ok("World time set to "+std::to_string(ticks)+" ticks");
        }});
    registry.add({"seed",{},"Display the saved world seed","/seed",P::Administrator,{},0,0,[](Context& c,auto) {
        requireWorld(c);return Result::ok("Seed: "+std::to_string(c.metadata.seed)); }});
    registry.add({"position",{"pos"},"Display current feet coordinates","/position",P::Player,{},0,0,[](Context& c,auto) {
        return Result::ok("Position: "+position(c.issuer.empty()?c.origin:feet(target(c,"@s")))); }});
    registry.add({"list",{},"List connected players","/list",P::Player,{},0,0,[](Context& c,auto) {
        std::string text=std::to_string(c.players.size())+" players:";for(const auto& player:c.players) text+=' '+player.name;return Result::ok(text); }});
    registry.add({"save",{},"Queue a consistent world snapshot","/save",P::Moderator,{},0,0,[](Context& c,auto) {
        if(!c.save) return Result::error(Code::Unavailable,"Saving is disabled in this world");
        return c.save(); }});
    registry.add({"spawnpoint",{},"Set a player's persistent respawn location","/spawnpoint [player] [x y z]",P::Administrator,
        {{"player",K::Player,true},{"x",K::Coordinate,true},{"y",K::Coordinate,true},{"z",K::Coordinate,true}},0,4,
        [](Context& c,std::span<const std::string> a) {
            requireWorld(c);Player* player=nullptr;glm::dvec3 p;
            if(a.empty()) { player=&target(c,"@s");p=feet(*player); }
            else if(a.size()==1) { player=&target(c,a[0]);p=feet(*player); }
            else if(a.size()==3) { player=&target(c,"@s");p=destination(a,c.origin); }
            else if(a.size()==4) { player=&target(c,a[0]);p=destination(a.subspan(1),c.origin); }
            else throw Error(Code::InvalidArgument,"Usage: /spawnpoint [player] [x y z]");
            if(!player->spawnPoint||!safeFeet(c,p)) throw Error(Code::InvalidArgument,"Respawn point is unavailable, obstructed or outside world bounds");
            *player->spawnPoint=p;return Result::ok("Spawnpoint for "+player->name+" set to "+position(p));
        }});
    registry.add({"kick",{},"Disconnect a player","/kick <player> [reason]",P::Moderator,
        {{"player",K::Player},{"reason",K::Text,true}},1,8,[](Context& c,std::span<const std::string> a) {
            auto& player=target(c,a[0]);if(player.permission>=c.permission) throw Error(Code::PermissionDenied,"Cannot kick an equal or higher permission level");
            const auto reason=a.size()>1?join(a.subspan(1)):"Kicked by an operator";
            if(reason.size()>120) throw Error(Code::InvalidArgument,"Kick reason exceeds 120 bytes");
            return c.kick(player.name,reason);
        },[](const Context& c){return bool(c.kick);}});
    registry.add({"op",{},"Grant authenticated profile administrator access","/op <player>",P::ServerConsole,{{"player",K::Player}},1,1,
        [](Context& c,std::span<const std::string> a){return c.setPermission(a[0],P::Administrator);},[](const Context& c){return bool(c.setPermission);}});
    registry.add({"deop",{},"Revoke a profile's operator access","/deop <player>",P::ServerConsole,{{"player",K::Player}},1,1,
        [](Context& c,std::span<const std::string> a){return c.setPermission(a[0],P::Player);},[](const Context& c){return bool(c.setPermission);}});
    registry.add({"say",{},"Send a bounded message","/say <message>",P::Player,{{"message",K::Text}},1,16,
        [](Context& c,std::span<const std::string> a){const auto message="<"+(c.issuer.empty()?std::string("server"):c.issuer)+"> "+join(a);if(c.broadcast) c.broadcast(message);return Result::ok("Message sent");}});
    return registry;
}
std::vector<Completion> Registry::complete(std::string_view prefix,const CompletionContext& c) const {
    using K=ArgumentKind;
    if(prefix.size()>maxCommandBytes||!utf8::valid(prefix)) return {};
    const auto last=prefix.find_last_of(" \t");const auto token=std::string(prefix.substr(last==std::string_view::npos?0:last+1));
    const auto before=last==std::string_view::npos?std::string{}:std::string(prefix.substr(0,last+1));
    std::vector<std::string> choices;
    if(last==std::string_view::npos) {
        const bool slash=token.starts_with('/');const auto match=slash?token.substr(1):token;
        for(const auto& [name,d]:definitions_) if(c.permission>=d.permission&&(!d.available||c.administration||(name!="kick"&&name!="op"&&name!="deop"))) {
            if(name.starts_with(match)) choices.push_back((slash?"/":"")+name);
            for(const auto& alias:d.aliases) if(alias.starts_with(match)) choices.push_back((slash?"/":"")+alias);
        }
    } else {
        Parsed parsed;try { parsed=parse(before); } catch(...) { return {}; }
        const auto* d=find(parsed.name);if(!d||c.permission<d->permission) return {};
        const auto index=parsed.arguments.size();
        auto kind=index<d->arguments.size()?d->arguments[index].kind:ArgumentKind::Text;
        if(d->name=="give"&&index==1) kind=ArgumentKind::Item;
        if(kind==K::Player) { choices=c.players;if(c.self) choices.push_back("@s"); }
        else if(kind==K::Item&&c.items) for(std::size_t i=0;i<c.items->size();++i) choices.push_back(c.items->definition(static_cast<ItemId>(i)).identifier);
        else if(kind==K::Mode) choices={"creative","survival"};
        else if(kind==K::Entity) for(const auto& entity:creatureDefinitions()) { choices.emplace_back(entity.identifier);for(const auto alias:entity.aliases) choices.emplace_back(alias); }
        else if(kind==K::Time) choices=index==0?std::vector<std::string>{"query","set"}:std::vector<std::string>{"day","night","0","6000","18000"};
        std::erase_if(choices,[&](const auto& choice){return !choice.starts_with(token);});
    }
    std::sort(choices.begin(),choices.end());choices.erase(std::unique(choices.begin(),choices.end()),choices.end());
    std::vector<Completion> result;for(const auto& choice:choices) { result.push_back({choice,before+choice});if(result.size()==32) break; }return result;
}
}
