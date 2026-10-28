#include "game/modding/runtime.hpp"
#include "engine/core/paths.hpp"
#include "game/content/atlas.hpp"
#include <nlohmann/json.hpp>
#include <lua.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <set>
#include <stdexcept>
#include <utility>

namespace vc {
namespace {
using Json=nlohmann::json;
constexpr std::array<const char*,5> hooks{"on_load","on_tick","on_place","on_break","on_craft"};
std::string read(const std::filesystem::path& path,std::size_t maximum=131072) {
    const auto size=std::filesystem::file_size(path);if(size>maximum) throw std::runtime_error("Mod file exceeds size limit: "+pathText(path));
    std::ifstream file(path,std::ios::binary);std::string data(static_cast<std::size_t>(size),'\0');
    if(!file||(!data.empty()&&!file.read(data.data(),static_cast<std::streamsize>(data.size())))) throw std::runtime_error("Cannot read mod file: "+pathText(path));
    return data;
}
Json json(const std::filesystem::path& path) {
    const auto data=read(path);
    return Json::parse(data,[](int depth,Json::parse_event_t,Json&) {
        if(depth>24) throw std::runtime_error("Mod JSON nesting exceeds 24 levels");return true;
    });
}
void keys(const Json& object,std::initializer_list<std::string_view> allowed) {
    if(!object.is_object()) throw std::runtime_error("Expected mod JSON object");
    for(const auto& [key,value]:object.items()) {
        (void)value;if(std::find(allowed.begin(),allowed.end(),key)==allowed.end()) throw std::runtime_error("Unknown mod field: "+key);
    }
}
unsigned integer(const Json& value,unsigned minimum,unsigned maximum) {
    if(!value.is_number_integer()) throw std::runtime_error("Expected mod integer");
    const auto result=value.get<std::int64_t>();
    if(result<minimum||result>maximum) throw std::runtime_error("Mod integer outside supported range");return static_cast<unsigned>(result);
}
void own(const std::string& mod,const std::string& name) {
    if(name.size()>256||!voxel::BlockRegistry::validIdentifier(name)||!name.starts_with(mod+':')) throw std::runtime_error("Content must use its mod namespace: "+name);
}
TextureTile tile(const std::string& name) {
    static const std::map<std::string,TextureTile,std::less<>> tilesByName{{"white",tiles::white},{"stone",tiles::stone},{"dirt",tiles::dirt},
        {"grass_top",tiles::grassTop},{"grass_side",tiles::grassSide},{"log",tiles::logSide},{"log_top",tiles::logTop},{"planks",tiles::planks},
        {"leaves",tiles::leaves},{"cobblestone",tiles::cobblestone},{"coal_ore",tiles::coalOre},{"iron_ore",tiles::ironOre},
        {"sand",tiles::sand},{"sandstone",tiles::sandstone},{"snow",tiles::snow},{"water",tiles::water},{"lamp",tiles::lamp}};
    const auto found=tilesByName.find(name);if(found==tilesByName.end()) throw std::runtime_error("Unknown built-in texture: "+name);return found->second;
}
struct Pack {
    SavedMod saved;
    std::map<std::string,std::string,std::less<>> dependencies;
    Json content;
    std::filesystem::path directory;
};
void blocks(const Pack& pack,voxel::BlockRegistry& registry) {
    if(!pack.content.contains("blocks")) return;
    const auto& definitions=pack.content.at("blocks");
    if(!definitions.is_array()||definitions.size()>128) throw std::runtime_error("Mod blocks must be an array of at most 128 definitions");
    for(const auto& data:definitions) {
        keys(data,{"id","color","solid","opaque","fluid","emission","opacity","texture","face_textures"});
        voxel::BlockDefinition block;block.identifier=data.at("id").get<std::string>();own(pack.saved.id,block.identifier);
        block.opaque=data.value("opaque",true);block.solid=data.value("solid",true);block.fluid=data.value("fluid",false);
        if(data.contains("emission")) block.emission=static_cast<std::uint8_t>(integer(data.at("emission"),0,15));
        block.opacity=data.value("opacity",1.0f);
        if(data.contains("color")) {
            const auto& color=data.at("color");if(!color.is_array()||color.size()!=3) throw std::runtime_error("Block color requires three components");
            for(int axis=0;axis<3;++axis) { block.color[axis]=color.at(static_cast<std::size_t>(axis)).get<float>();if(!std::isfinite(block.color[axis])||block.color[axis]<0||block.color[axis]>1) throw std::runtime_error("Block colors must be within 0..1"); }
        }
        block.faceTextures.fill(tile(data.value("texture",std::string("white"))));
        if(data.contains("face_textures")) {
            if(data.contains("texture")) throw std::runtime_error("Use texture or face_textures, not both");
            const auto& faces=data.at("face_textures");if(!faces.is_array()||faces.size()!=6) throw std::runtime_error("face_textures requires six texture names");
            for(std::size_t i=0;i<6;++i) block.faceTextures[i]=tile(faces.at(i).get<std::string>());
        }
        registry.add(std::move(block));
    }
}
struct Edit { voxel::Position position;voxel::BlockId block; };
struct Execution {
    ModContext context;
    SurvivalInventory inventory;
    SavedMod state;
    std::vector<Edit> edits;
    std::vector<std::string> messages;
    bool inventoryChanged{};
};
struct Script {
    lua_State* lua{};
    std::size_t bytes{};
    int remaining{},handlers{LUA_NOREF};
    bool active{true};
    Execution* execution{};
    const ModEvent* event{};
    static constexpr std::size_t memoryLimit=4*1024*1024;
    static void* allocate(void* user,void* pointer,std::size_t oldSize,std::size_t newSize) {
        auto& script=*static_cast<Script*>(user);const auto old=pointer?oldSize:0;
        if(!newSize) { std::free(pointer);script.bytes-=old;return nullptr; }
        if(newSize>old&&newSize-old>memoryLimit-script.bytes) return nullptr;
        void* next=std::realloc(pointer,newSize);if(next) script.bytes=script.bytes-old+newSize;return next;
    }
    static Script& self(lua_State* state) { void* user{};(void)lua_getallocf(state,&user);return *static_cast<Script*>(user); }
    static void budget(lua_State* state,lua_Debug*) {
        auto& script=self(state);script.remaining-=1000;if(script.remaining<=0) luaL_error(state,"Mod instruction budget exhausted");
    }
    static std::string string(lua_State* state,int argument,std::size_t maximum=256) {
        if(lua_type(state,argument)!=LUA_TSTRING) throw std::runtime_error("API argument must be a string");
        std::size_t size{};const char* value=lua_tolstring(state,argument,&size);
        if(size>maximum) throw std::runtime_error("API string exceeds limit");return {value,size};
    }
    static std::int64_t coordinate(lua_State* state,int argument) {
        if(!lua_isinteger(state,argument)) throw std::runtime_error("Block coordinates must be integers");
        const auto value=lua_tointeger(state,argument);if(value< -1000000000000000LL||value>1000000000000000LL) throw std::runtime_error("Block coordinate exceeds physics range");return value;
    }
    static int api(lua_State* state) {
        try {
            auto& script=self(state);if(!script.execution) throw std::runtime_error("World APIs are available only inside event hooks");
            auto& exec=*script.execution;const auto operation=lua_tointeger(state,lua_upvalueindex(1));
            if(operation==0||operation==1) { // get/set block
                const voxel::Position p{coordinate(state,1),coordinate(state,2),coordinate(state,3)};
                const bool loaded=exec.context.world.bounds().contains(p.y)&&exec.context.world.loaded(p);
                if(operation==0) {
                    if(!loaded) { lua_pushnil(state);return 1; }
                    auto block=exec.context.world.block(p);
                    for(auto it=exec.edits.rbegin();it!=exec.edits.rend();++it) if(it->position==p) { block=it->block;break; }
                    const auto& name=exec.context.blocks.definition(block).identifier;lua_pushlstring(state,name.data(),name.size());return 1;
                }
                const auto name=string(state,4);const auto block=exec.context.blocks.find(name);
                if(!block) throw std::runtime_error("Unknown block: "+name);
                const glm::dvec3 low{static_cast<double>(p.x),static_cast<double>(p.y),static_cast<double>(p.z)};
                const auto center=(exec.context.player.min+exec.context.player.max)*0.5;
                const bool valid=loaded&&glm::length(low+0.5-center)<=8&&exec.edits.size()<64&&
                    (!exec.context.blocks.definition(*block).solid||!exec.context.player.intersects({low,low+1.0}));
                if(valid) exec.edits.push_back({p,*block});lua_pushboolean(state,valid);return 1;
            }
            if(operation==2) { // give item
                const auto name=string(state,1);if(!lua_isinteger(state,2)) throw std::runtime_error("Item count must be an integer");
                const auto count=lua_tointeger(state,2);if(count<1||count>64) throw std::runtime_error("Item count must be 1..64");
                const auto item=exec.context.items.find(name);if(!item) throw std::runtime_error("Unknown item: "+name);
                const bool added=exec.context.survival&&exec.context.survival->alive()&&exec.inventory.add(*item,static_cast<std::uint16_t>(count),exec.context.items);
                exec.inventoryChanged=exec.inventoryChanged||added;lua_pushboolean(state,added);return 1;
            }
            if(operation==3) { // message
                auto message=string(state,1,120);if(exec.messages.size()>=4) throw std::runtime_error("Too many mod messages in one event");
                exec.messages.push_back(std::move(message));return 0;
            }
            if(operation==4||operation==5) { // get/set state
                const auto key=string(state,1,64);if(key.empty()) throw std::runtime_error("State key must not be empty");
                if(operation==4) {
                    const auto found=exec.state.state.find(key);if(found==exec.state.state.end()) lua_pushnil(state);
                    else lua_pushlstring(state,found->second.data(),found->second.size());return 1;
                }
                auto next=exec.state.state;if(lua_isnil(state,2)) next.erase(key);else next[key]=string(state,2,256);
                std::size_t size=0;for(const auto& [name,value]:next) size+=name.size()+value.size()+4;
                const bool fits=next.size()<=64&&size<=1024;if(fits) exec.state.state=std::move(next);lua_pushboolean(state,fits);return 1;
            }
            if(operation==6) { // player feet position
                const auto center=(exec.context.player.min+exec.context.player.max)*0.5;
                lua_pushnumber(state,center.x);lua_pushnumber(state,exec.context.player.min.y);lua_pushnumber(state,center.z);return 3;
            }
            lua_pushboolean(state,exec.context.survival!=nullptr);return 1;
        } catch(const std::exception& error) { return luaL_error(state,"%s",error.what()); }
    }
    static int invoke(lua_State* state) {
        auto& script=self(state);lua_rawgeti(state,LUA_REGISTRYINDEX,script.handlers);
        lua_getfield(state,-1,hooks.at(static_cast<std::size_t>(script.event->kind)));
        if(lua_isnil(state,-1)) return 0;
        lua_newtable(state);
        lua_pushinteger(state,script.event->position.x);lua_setfield(state,-2,"x");
        lua_pushinteger(state,script.event->position.y);lua_setfield(state,-2,"y");
        lua_pushinteger(state,script.event->position.z);lua_setfield(state,-2,"z");
        lua_pushlstring(state,script.event->identifier.data(),script.event->identifier.size());lua_setfield(state,-2,"id");
        lua_pushnumber(state,script.event->seconds);lua_setfield(state,-2,"dt");
        lua_call(state,1,0);return 0;
    }
    explicit Script(const std::filesystem::path& path) {
        lua=lua_newstate(allocate,this);if(!lua) throw std::runtime_error("Cannot create Lua state");
        try {
            luaL_requiref(lua,"_G",luaopen_base,1);lua_pop(lua,1);
            for(const auto* name:{"dofile","loadfile","load","collectgarbage","print","warn","pcall","xpcall","setmetatable","getmetatable"}) { lua_pushnil(lua);lua_setglobal(lua,name); }
            luaL_requiref(lua,LUA_TABLIBNAME,luaopen_table,1);lua_pop(lua,1);
            luaL_requiref(lua,LUA_STRLIBNAME,luaopen_string,1);lua_pop(lua,1);
            luaL_requiref(lua,LUA_MATHLIBNAME,luaopen_math,1);lua_pop(lua,1);
            luaL_requiref(lua,LUA_UTF8LIBNAME,luaopen_utf8,1);lua_pop(lua,1);
            lua_newtable(lua);constexpr std::array names{"get_block","set_block","give_item","message","get_state","set_state","player_position","is_survival"};
            for(std::size_t i=0;i<names.size();++i) { lua_pushinteger(lua,static_cast<lua_Integer>(i));lua_pushcclosure(lua,api,1);lua_setfield(lua,-2,names[i]); }
            lua_pushinteger(lua,1);lua_setfield(lua,-2,"api_version");lua_setglobal(lua,"voxelcraft");
            const auto source=read(path);const auto name=pathText(path);
            if(luaL_loadbufferx(lua,source.data(),source.size(),name.c_str(),"t")!=LUA_OK) throw std::runtime_error(lua_tostring(lua,-1));
            remaining=100000;lua_sethook(lua,budget,LUA_MASKCOUNT,1000);
            if(lua_pcall(lua,0,1,0)!=LUA_OK) throw std::runtime_error(lua_tostring(lua,-1));
            if(!lua_istable(lua,-1)) throw std::runtime_error("init.lua must return a table of event functions");
            lua_pushnil(lua);
            while(lua_next(lua,-2)) {
                if(lua_type(lua,-2)!=LUA_TSTRING||!lua_isfunction(lua,-1)) throw std::runtime_error("Event table requires named functions");
                const auto hook=string(lua,-2,32);
                if(std::find(hooks.begin(),hooks.end(),hook)==hooks.end()) throw std::runtime_error("Unknown Lua hook: "+hook);
                lua_pop(lua,1);
            }
            handlers=luaL_ref(lua,LUA_REGISTRYINDEX);lua_sethook(lua,nullptr,0,0);
        } catch(...) { lua_close(lua);lua=nullptr;throw; }
    }
    ~Script() { if(lua) lua_close(lua); }
    std::optional<std::string> run(const ModEvent& message,Execution& exec) {
        if(!active) return {};
        event=&message;execution=&exec;remaining=100000;lua_settop(lua,0);lua_sethook(lua,budget,LUA_MASKCOUNT,1000);
        lua_pushcfunction(lua,invoke);const auto result=lua_pcall(lua,0,0,0);lua_sethook(lua,nullptr,0,0);execution=nullptr;event=nullptr;
        if(result!=LUA_OK) {
            active=false;const char* error=lua_tostring(lua,-1);std::string reason=error?error:"Unknown Lua error";lua_settop(lua,0);return reason;
        }
        return {};
    }
};
}
struct ModRuntime::Impl {
    std::vector<Pack> packs;
    std::vector<std::unique_ptr<Script>> scripts;
    std::vector<SavedMod> missing;
    std::vector<std::string> diagnostics;
};
ModRuntime::ModRuntime(const std::filesystem::path& root,voxel::BlockRegistry& registry):impl_(std::make_unique<Impl>()) {
    if(root.empty()||!std::filesystem::exists(root)) return;
    if(!std::filesystem::is_directory(root)) throw std::runtime_error("Mods root is not a directory: "+pathText(root));
    std::map<std::string,Pack,std::less<>> found;
    for(const auto& entry:std::filesystem::directory_iterator(root)) {
        if(!entry.is_directory()) continue;
        const auto manifest=entry.path()/"mod.json";if(!std::filesystem::exists(manifest)) continue;
        try {
            const auto data=json(manifest);keys(data,{"id","version","api","dependencies"});
            Pack pack;pack.directory=entry.path();pack.saved.id=data.at("id").get<std::string>();pack.saved.version=data.at("version").get<std::string>();
            validateMods({pack.saved});if(integer(data.at("api"),1,1)!=1) throw std::runtime_error("Unsupported mod API");
            if(data.contains("dependencies")) {
                if(!data.at("dependencies").is_object()) throw std::runtime_error("Dependencies must map IDs to exact versions");
                for(const auto& [id,version]:data.at("dependencies").items()) {
                    const auto text=version.get<std::string>();if(!validModId(id)||!validModVersion(text)||id==pack.saved.id) throw std::runtime_error("Invalid mod dependency");
                    pack.dependencies.emplace(id,text);
                }
            }
            const auto content=entry.path()/"content.json";pack.content=std::filesystem::exists(content)?json(content):Json::object();keys(pack.content,{"blocks","items","recipes"});
            if(!found.emplace(pack.saved.id,std::move(pack)).second) throw std::runtime_error("Duplicate mod identifier");
            if(found.size()>32) throw std::runtime_error("At most 32 mods are supported");
        } catch(const std::exception& error) { throw std::runtime_error(pathText(manifest)+": "+error.what()); }
    }
    std::map<std::string,int,std::less<>> visited;
    std::function<void(const std::string&)> visit=[&](const std::string& id) {
        if(visited[id]==2) return;if(visited[id]==1) throw std::runtime_error("Mod dependency cycle at "+id);
        visited[id]=1;const auto& pack=found.at(id);
        for(const auto& [dependency,version]:pack.dependencies) {
            const auto candidate=found.find(dependency);if(candidate==found.end()) throw std::runtime_error(id+" requires missing mod "+dependency);
            if(candidate->second.saved.version!=version) throw std::runtime_error(id+" requires "+dependency+" version "+version);
            visit(dependency);
        }
        visited[id]=2;impl_->packs.push_back(pack);
    };
    for(const auto& [id,pack]:found) { (void)pack;visit(id); }
    auto next=registry;
    for(const auto& pack:impl_->packs) {
        try { blocks(pack,next);const auto script=pack.directory/"init.lua";impl_->scripts.push_back(std::filesystem::exists(script)?std::make_unique<Script>(script):nullptr); }
        catch(const std::exception& error) { throw std::runtime_error("Mod "+pack.saved.id+": "+error.what()); }
    }
    auto trial=builtinItems(next);auto recipes=builtinRecipes(trial);installItems(trial,next,recipes);registry=std::move(next);
}
ModRuntime::~ModRuntime()=default;
void ModRuntime::installItems(ItemRegistry& items,const voxel::BlockRegistry& registry,std::vector<Recipe>& recipes) const {
    auto next=items;auto output=recipes;
    for(const auto& pack:impl_->packs) if(pack.content.contains("items")) {
        const auto& definitions=pack.content.at("items");if(!definitions.is_array()||definitions.size()>128) throw std::runtime_error("Mod items must be an array of at most 128 definitions");
        for(const auto& data:definitions) {
            keys(data,{"id","block","stack_size","durability","tool","tier","mining_speed"});
            ItemDefinition item;item.identifier=data.at("id").get<std::string>();own(pack.saved.id,item.identifier);
            if(data.contains("block")) item.block=registry.require(data.at("block").get<std::string>());
            if(data.contains("stack_size")) item.stackSize=static_cast<std::uint16_t>(integer(data.at("stack_size"),1,64));
            if(data.contains("durability")) item.durability=static_cast<std::uint16_t>(integer(data.at("durability"),0,65535));
            const auto tool=data.value("tool",std::string("none"));if(tool!="none"&&tool!="pickaxe") throw std::runtime_error("Unknown mod tool kind");
            item.tool=tool=="pickaxe"?ToolKind::Pickaxe:ToolKind::None;
            if(data.contains("tier")) item.tier=integer(data.at("tier"),0,16);
            item.miningSpeed=data.value("mining_speed",1.0);next.add(std::move(item));
        }
    }
    for(const auto& pack:impl_->packs) if(pack.content.contains("recipes")) {
        const auto& definitions=pack.content.at("recipes");if(!definitions.is_array()||definitions.size()>128) throw std::runtime_error("Mod recipes must be an array of at most 128 definitions");
        for(const auto& data:definitions) {
            keys(data,{"name","ingredients","output","count"});Recipe recipe;recipe.name=data.at("name").get<std::string>();
            if(recipe.name.empty()||recipe.name.size()>24||!std::all_of(recipe.name.begin(),recipe.name.end(),[](char c){return c>=32&&c<=126;})) throw std::runtime_error("Recipe name must be 1..24 printable ASCII characters");
            recipe.output=next.require(data.at("output").get<std::string>());recipe.count=static_cast<std::uint16_t>(integer(data.at("count"),1,next.definition(recipe.output).stackSize));
            const auto& ingredients=data.at("ingredients");if(!ingredients.is_object()||ingredients.empty()||ingredients.size()>4) throw std::runtime_error("Recipes require 1..4 ingredients");
            for(const auto& [id,count]:ingredients.items()) recipe.ingredients.push_back({next.require(id),static_cast<std::uint16_t>(integer(count,1,64))});
            output.push_back(std::move(recipe));
        }
    }
    if(output.size()>128) throw std::runtime_error("At most 128 total recipes are supported");items=std::move(next);recipes=std::move(output);
}
std::vector<SavedMod> ModRuntime::save() const {
    auto saved=impl_->missing;for(const auto& pack:impl_->packs) saved.push_back(pack.saved);
    std::sort(saved.begin(),saved.end(),[](const auto& a,const auto& b){return a.id<b.id;});validateMods(saved);return saved;
}
void ModRuntime::restore(const std::vector<SavedMod>& saved) {
    validateMods(saved);auto packs=impl_->packs;std::vector<SavedMod> missing;
    for(const auto& mod:saved) {
        const auto found=std::find_if(packs.begin(),packs.end(),[&](const auto& pack){return pack.saved.id==mod.id;});
        if(found==packs.end()) { missing.push_back(mod);impl_->diagnostics.push_back("Missing saved mod "+mod.id+" "+mod.version+"; its state/content keys are retained"); }
        else {
            if(found->saved.version!=mod.version) impl_->diagnostics.push_back("Saved mod "+mod.id+" changed from "+mod.version+" to "+found->saved.version);
            found->saved.state=mod.state;
        }
    }
    auto combined=missing;for(const auto& pack:packs) combined.push_back(pack.saved);validateMods(combined);
    impl_->packs=std::move(packs);impl_->missing=std::move(missing);
}
std::vector<std::string> ModRuntime::dispatch(const ModEvent& event,ModContext context) {
    if(static_cast<std::size_t>(event.kind)>=hooks.size()||!std::isfinite(event.seconds)||event.seconds<0||event.seconds>1||event.identifier.size()>256)
        throw std::invalid_argument("Invalid mod event");
    std::vector<std::string> messages;
    for(std::size_t i=0;i<impl_->packs.size();++i) {
        auto* script=impl_->scripts[i].get();if(!script||!script->active) continue;
        auto& pack=impl_->packs[i];Execution exec{context,context.survival?context.survival->inventory:SurvivalInventory{},pack.saved,{},{},false};
        auto error=script->run(event,exec);
        if(!error) {
            auto candidate=save();for(auto& saved:candidate) if(saved.id==pack.saved.id) saved=exec.state;
            try { validateMods(candidate); } catch(const std::exception& failure) { error=failure.what();script->active=false; }
        }
        if(error) { impl_->diagnostics.push_back("Disabled script "+pack.saved.id+": "+*error);continue; }
        for(const auto& edit:exec.edits) context.world.setBlock(edit.position,edit.block);
        if(exec.inventoryChanged) context.survival->inventory=std::move(exec.inventory);
        pack.saved=std::move(exec.state);for(auto& message:exec.messages) messages.push_back(pack.saved.id+": "+message);
    }
    return messages;
}
std::vector<std::string> ModRuntime::takeDiagnostics() { return std::exchange(impl_->diagnostics,{}); }
std::size_t ModRuntime::size() const { return impl_->packs.size(); }
std::vector<std::string> ModRuntime::order() const { std::vector<std::string> result;for(const auto& pack:impl_->packs) result.push_back(pack.saved.id);return result; }
}
