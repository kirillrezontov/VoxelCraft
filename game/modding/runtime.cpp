#include "game/modding/runtime.hpp"
#include "game/modding/native_runtime.hpp"
#include "engine/core/paths.hpp"
#include "game/content/atlas.hpp"
#include "game/content/png.hpp"
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
        if(depth>24) throw std::runtime_error("Mod JSON nesting exceeds 24 levels");
        return true;
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
    if(result<minimum||result>maximum) throw std::runtime_error("Mod integer outside supported range");
    return static_cast<unsigned>(result);
}
void own(const std::string& mod,const std::string& name) {
    if(name.size()>256||!voxel::BlockRegistry::validIdentifier(name)||!name.starts_with(mod+':')) throw std::runtime_error("Content must use its mod namespace: "+name);
}
struct Pack {
    SavedMod saved;
    std::map<std::string,std::string,std::less<>> dependencies;
    Json content;
    std::filesystem::path directory;
};
std::filesystem::path assetPath(const Pack& pack,const std::string& name) {
    if(name.empty()||name.size()>256||name.find_first_of("\\:*?\"")!=std::string::npos||
        std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32||c==127;}))
        throw std::runtime_error("Invalid portable mod asset path");
    const auto relative=utf8Path(name);
    if(relative.has_root_path()) throw std::runtime_error("Mod assets require a relative path");
    for(const auto& part:relative) if(part=="..") throw std::runtime_error("Mod asset path must stay inside its pack");
    const auto root=std::filesystem::canonical(pack.directory),resolved=std::filesystem::weakly_canonical(root/relative);
    auto cursor=resolved.begin();
    for(const auto& part:root) {
        if(cursor==resolved.end()||*cursor!=part) throw std::runtime_error("Mod asset symlink escapes its pack");
        ++cursor;
    }
    return resolved;
}
void textures(const Pack& pack,ContentAtlas& atlas) {
    if(!pack.content.contains("textures")) return;
    const auto& definitions=pack.content.at("textures");
    if(!definitions.is_array()||definitions.size()>128) throw std::runtime_error("Mod textures must be an array of at most 128 definitions");
    for(const auto& data:definitions) {
        keys(data,{"id","file"});const auto id=data.at("id").get<std::string>();own(pack.saved.id,id);
        atlas.add(id,loadPngTile(assetPath(pack,data.at("file").get<std::string>())));
    }
}
ContentPresentation presentation(const Json& data,const ContentAtlas& atlas,std::vector<std::string>& diagnostics) {
    ContentPresentation result;result.name=data.value("name",std::string{});result.description=data.value("description",std::string{});
    result.category=data.value("category",std::string{});result.icon=data.value("icon",std::string{});
    validatePresentation(result);
    if(!result.icon.empty()) {
        result.iconTile=atlas.find(result.icon);
        if(!result.iconTile) {
            result.iconTile=tiles::missingItem;const auto message="Missing item icon "+result.icon+"; using fallback";
            if(std::ranges::find(diagnostics,message)==diagnostics.end()) diagnostics.push_back(message);
        }
    }
    return result;
}
void blocks(const Pack& pack,voxel::BlockRegistry& registry,const ContentAtlas& atlas,std::vector<std::string>& diagnostics) {
    if(!pack.content.contains("blocks")) return;
    const auto& definitions=pack.content.at("blocks");
    if(!definitions.is_array()||definitions.size()>128) throw std::runtime_error("Mod blocks must be an array of at most 128 definitions");
    for(const auto& data:definitions) {
        keys(data,{"id","color","solid","opaque","fluid","emission","opacity","texture","face_textures","mining","name","description","category","icon"});
        voxel::BlockDefinition block;block.identifier=data.at("id").get<std::string>();own(pack.saved.id,block.identifier);
        block.opaque=data.value("opaque",true);block.solid=data.value("solid",true);block.fluid=data.value("fluid",false);
        if(data.contains("emission")) block.emission=static_cast<std::uint8_t>(integer(data.at("emission"),0,15));
        block.opacity=data.value("opacity",1.0f);
        if(data.contains("color")) {
            const auto& color=data.at("color");if(!color.is_array()||color.size()!=3) throw std::runtime_error("Block color requires three components");
            for(int axis=0;axis<3;++axis) { block.color[axis]=color.at(static_cast<std::size_t>(axis)).get<float>();if(!std::isfinite(block.color[axis])||block.color[axis]<0||block.color[axis]>1) throw std::runtime_error("Block colors must be within 0..1"); }
        }
        block.faceTextures.fill(atlas.require(data.value("texture",std::string("white"))));
        if(data.contains("face_textures")) {
            if(data.contains("texture")) throw std::runtime_error("Use texture or face_textures, not both");
            const auto& faces=data.at("face_textures");if(!faces.is_array()||faces.size()!=6) throw std::runtime_error("face_textures requires six texture names");
            for(std::size_t i=0;i<6;++i) block.faceTextures[i]=atlas.require(faces.at(i).get<std::string>());
        }
        block.presentation=presentation(data,atlas,diagnostics);registry.add(std::move(block));
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
    SavedMod* migration{};
    const SavedMod* migrationSource{};
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
        if(size>maximum) throw std::runtime_error("API string exceeds limit");
        return {value,size};
    }
    static std::string errorText(lua_State* state) {
        // Lua permits error({}) and other non-string values.
        const char* message=lua_type(state,-1)==LUA_TSTRING?lua_tostring(state,-1):nullptr;
        return message?message:"Lua raised a non-string error";
    }
    static std::int64_t coordinate(lua_State* state,int argument) {
        if(!lua_isinteger(state,argument)) throw std::runtime_error("Block coordinates must be integers");
        const auto value=lua_tointeger(state,argument);if(value< -1000000000000000LL||value>1000000000000000LL) throw std::runtime_error("Block coordinate exceeds physics range");return value;
    }
    static int api(lua_State* state) {
        try {
            auto& script=self(state);const auto operation=lua_tointeger(state,lua_upvalueindex(1));
            if(operation==4||operation==5) { // State APIs also work in migrations.
                auto* saved=script.execution?&script.execution->state:script.migration;
                if(!saved) throw std::runtime_error("State APIs are available only inside event hooks or migrations");
                const auto key=string(state,1,64);if(key.empty()) throw std::runtime_error("State key must not be empty");
                if(operation==4) {
                    const auto found=saved->state.find(key);if(found==saved->state.end()) lua_pushnil(state);
                    else lua_pushlstring(state,found->second.data(),found->second.size());
                    return 1;
                }
                auto next=saved->state;if(lua_isnil(state,2)) next.erase(key);else next[key]=string(state,2,256);
                std::size_t size=0;for(const auto& [name,value]:next) size+=name.size()+value.size()+4;
                const bool fits=next.size()<=64&&size<=1024;if(fits) saved->state=std::move(next);
                lua_pushboolean(state,fits);return 1;
            }
            if(!script.execution) throw std::runtime_error("World APIs are available only inside gameplay event hooks");
            auto& exec=*script.execution;
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
                const voxel::Aabb cell{low,low+1.0};
                const bool occupied=exec.context.player.intersects(cell)||std::any_of(exec.context.players.begin(),exec.context.players.end(),[&](const auto& body){return body.intersects(cell);});
                const bool valid=loaded&&glm::length(low+0.5-center)<=8&&exec.edits.size()<64&&
                    (!exec.context.blocks.definition(*block).solid||!occupied);
                if(valid) exec.edits.push_back({p,*block});
                lua_pushboolean(state,valid);return 1;
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
    static int invokeMigration(lua_State* state) {
        auto& script=self(state);lua_rawgeti(state,LUA_REGISTRYINDEX,script.handlers);
        lua_getfield(state,-1,"on_migrate");
        if(lua_isnil(state,-1)) return luaL_error(state,"State version upgrade requires on_migrate");
        lua_newtable(state);
        lua_pushlstring(state,script.migrationSource->version.data(),script.migrationSource->version.size());lua_setfield(state,-2,"from_version");
        lua_pushlstring(state,script.migration->version.data(),script.migration->version.size());lua_setfield(state,-2,"to_version");
        lua_pushinteger(state,script.migrationSource->stateVersion);lua_setfield(state,-2,"from_state_version");
        lua_pushinteger(state,script.migration->stateVersion);lua_setfield(state,-2,"to_state_version");
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
            if(luaL_loadbufferx(lua,source.data(),source.size(),name.c_str(),"t")!=LUA_OK) throw std::runtime_error(errorText(lua));
            remaining=100000;lua_sethook(lua,budget,LUA_MASKCOUNT,1000);
            if(lua_pcall(lua,0,1,0)!=LUA_OK) throw std::runtime_error(errorText(lua));
            if(!lua_istable(lua,-1)) throw std::runtime_error("init.lua must return a table of event functions");
            lua_pushnil(lua);
            while(lua_next(lua,-2)) {
                if(lua_type(lua,-2)!=LUA_TSTRING||!lua_isfunction(lua,-1)) throw std::runtime_error("Event table requires named functions");
                const auto hook=string(lua,-2,32);
                if(hook!="on_migrate"&&std::find(hooks.begin(),hooks.end(),hook)==hooks.end()) throw std::runtime_error("Unknown Lua hook: "+hook);
                lua_pop(lua,1);
            }
            handlers=luaL_ref(lua,LUA_REGISTRYINDEX);lua_sethook(lua,nullptr,0,0);
        } catch(...) { lua_close(lua);lua=nullptr;throw; }
    }
    ~Script() { if(lua) lua_close(lua); }
    std::optional<std::string> call(lua_CFunction invokeFunction) {
        remaining=100000;lua_settop(lua,0);lua_sethook(lua,budget,LUA_MASKCOUNT,1000);
        lua_pushcfunction(lua,invokeFunction);const auto result=lua_pcall(lua,0,0,0);lua_sethook(lua,nullptr,0,0);
        if(result!=LUA_OK) { auto reason=errorText(lua);lua_settop(lua,0);return reason; }
        return {};
    }
    std::optional<std::string> run(const ModEvent& message,Execution& exec) {
        if(!active) return {};
        event=&message;execution=&exec;auto error=call(invoke);execution=nullptr;event=nullptr;
        if(error) active=false;
        return error;
    }
    std::optional<std::string> migrate(const SavedMod& source,SavedMod& target) {
        migration=&target;migrationSource=&source;auto error=call(invokeMigration);
        migration=nullptr;migrationSource=nullptr;return error;
    }
};
}
struct ModRuntime::Impl {
    ContentAtlas atlas;
    std::vector<Pack> packs;
    std::vector<std::unique_ptr<Script>> scripts;
    std::vector<SavedMod> missing;
    std::vector<std::string> diagnostics;
    std::unique_ptr<NativeModRuntime> native;
};
ModRuntime::ModRuntime(const std::filesystem::path& root,voxel::BlockRegistry& registry,const std::filesystem::path& nativeRoot):impl_(std::make_unique<Impl>()) {
    std::optional<voxel::BlockRegistry> original;if(!nativeRoot.empty()) original=registry;
    const auto loadNative=[&] {
        if(nativeRoot.empty()) return;
        std::vector<SavedMod> dataMods;for(const auto& pack:impl_->packs) dataMods.push_back(pack.saved);
        auto dataItems=builtinItems(registry);auto dataRecipes=builtinRecipes(dataItems);installItems(dataItems,registry,dataRecipes);
        impl_->native=std::make_unique<NativeModRuntime>(nativeRoot,registry,dataMods,&dataItems);
        auto checked=builtinItems(registry);auto recipes=builtinRecipes(checked);installItems(checked,registry,recipes);
        MiningRules mining(registry,checked);installMining(mining,registry,checked);
    };
    try {
    if(root.empty()||!std::filesystem::exists(root)) { loadNative();return; }
    if(!std::filesystem::is_directory(root)) throw std::runtime_error("Mods root is not a directory: "+pathText(root));
    std::map<std::string,Pack,std::less<>> found;
    for(const auto& entry:std::filesystem::directory_iterator(root)) {
        if(!entry.is_directory()) continue;
        const auto manifest=entry.path()/"mod.json";if(!std::filesystem::exists(manifest)) continue;
        try {
            const auto data=json(manifest);keys(data,{"id","version","api","dependencies","state_version"});
            Pack pack;pack.directory=entry.path();pack.saved.id=data.at("id").get<std::string>();pack.saved.version=data.at("version").get<std::string>();
            validateMods({pack.saved});if(integer(data.at("api"),1,1)!=1) throw std::runtime_error("Unsupported mod API");
            if(data.contains("state_version")) pack.saved.stateVersion=integer(data.at("state_version"),1,65535);
            if(data.contains("dependencies")) {
                if(!data.at("dependencies").is_object()) throw std::runtime_error("Dependencies must map IDs to exact versions");
                for(const auto& [id,version]:data.at("dependencies").items()) {
                    const auto text=version.get<std::string>();if(!validModId(id)||!validModVersion(text)||id==pack.saved.id) throw std::runtime_error("Invalid mod dependency");
                    pack.dependencies.emplace(id,text);
                }
            }
            const auto content=entry.path()/"content.json";pack.content=std::filesystem::exists(content)?json(content):Json::object();keys(pack.content,{"textures","blocks","items","recipes"});
            if(!found.emplace(pack.saved.id,std::move(pack)).second) throw std::runtime_error("Duplicate mod identifier");
            if(found.size()>32) throw std::runtime_error("At most 32 mods are supported");
        } catch(const std::exception& error) { throw std::runtime_error(pathText(manifest)+": "+error.what()); }
    }
    std::map<std::string,int,std::less<>> visited;
    std::function<void(const std::string&)> visit=[&](const std::string& id) {
        if(visited[id]==2) return;
        if(visited[id]==1) throw std::runtime_error("Mod dependency cycle at "+id);
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
        try { textures(pack,impl_->atlas); }
        catch(const std::exception& error) { throw std::runtime_error("Mod "+pack.saved.id+": "+error.what()); }
    }
    for(const auto& pack:impl_->packs) {
        try { blocks(pack,next,impl_->atlas,impl_->diagnostics);const auto script=pack.directory/"init.lua";impl_->scripts.push_back(std::filesystem::exists(script)?std::make_unique<Script>(script):nullptr); }
        catch(const std::exception& error) { throw std::runtime_error("Mod "+pack.saved.id+": "+error.what()); }
    }
    auto trial=builtinItems(next);auto recipes=builtinRecipes(trial);installItems(trial,next,recipes);
    MiningRules rules(next,trial);installMining(rules,next,trial);registry=std::move(next);
    loadNative();
    } catch(...) { if(original) registry=std::move(*original);throw; }
}
ModRuntime::~ModRuntime()=default;
void ModRuntime::installItems(ItemRegistry& items,const voxel::BlockRegistry& registry,std::vector<Recipe>& recipes) const {
    auto next=items;auto output=recipes;
    for(const auto& pack:impl_->packs) if(pack.content.contains("items")) {
        const auto& definitions=pack.content.at("items");if(!definitions.is_array()||definitions.size()>128) throw std::runtime_error("Mod items must be an array of at most 128 definitions");
        for(const auto& data:definitions) {
            keys(data,{"id","block","stack_size","durability","tool","tier","mining_speed","name","description","category","icon"});
            ItemDefinition item;item.identifier=data.at("id").get<std::string>();own(pack.saved.id,item.identifier);
            if(data.contains("block")) item.block=registry.require(data.at("block").get<std::string>());
            if(data.contains("stack_size")) item.stackSize=static_cast<std::uint16_t>(integer(data.at("stack_size"),1,64));
            if(data.contains("durability")) item.durability=static_cast<std::uint16_t>(integer(data.at("durability"),0,65535));
            const auto tool=data.value("tool",std::string("none"));if(tool!="none"&&tool!="pickaxe") throw std::runtime_error("Unknown mod tool kind");
            item.tool=tool=="pickaxe"?ToolKind::Pickaxe:ToolKind::None;
            if(data.contains("tier")) item.tier=integer(data.at("tier"),0,16);
            item.miningSpeed=data.value("mining_speed",1.0);item.presentation=presentation(data,impl_->atlas,impl_->diagnostics);next.add(std::move(item));
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
    if(impl_->native) impl_->native->installItems(next,registry,output);
    if(output.size()>128) throw std::runtime_error("At most 128 total recipes are supported");
    items=std::move(next);recipes=std::move(output);
}
void ModRuntime::installMining(MiningRules& rules,const voxel::BlockRegistry& registry,const ItemRegistry& items) const {
    auto next=rules;
    for(const auto& pack:impl_->packs) if(pack.content.contains("blocks")) {
        for(const auto& data:pack.content.at("blocks")) {
            if(!data.contains("mining")) continue;
            const auto block=registry.require(data.at("id").get<std::string>());
            const auto& mining=data.at("mining");keys(mining,{"seconds","tool","tier","drop","drop_count"});
            auto rule=next.rule(block);rule.seconds=mining.value("seconds",rule.seconds);
            const auto tool=mining.value("tool",std::string("none"));
            if(tool!="none"&&tool!="pickaxe") throw std::runtime_error("Unknown mining tool kind");
            rule.tool=tool=="pickaxe"?ToolKind::Pickaxe:ToolKind::None;
            if(mining.contains("tier")) rule.requiredTier=integer(mining.at("tier"),0,16);
            if(mining.contains("drop")) rule.drop=mining.at("drop").is_null()?std::nullopt:std::optional(items.require(mining.at("drop").get<std::string>()));
            if(mining.contains("drop_count")) {
                if(!rule.drop) throw std::runtime_error("A mining drop_count requires an item drop");
                rule.dropCount=static_cast<std::uint16_t>(integer(mining.at("drop_count"),1,64));
            }
            next.set(block,rule,items);
        }
    }
    rules=std::move(next);
}
std::vector<SavedMod> ModRuntime::save() const {
    auto saved=impl_->missing;for(const auto& pack:impl_->packs) saved.push_back(pack.saved);
    if(impl_->native) { auto native=impl_->native->save();saved.insert(saved.end(),native.begin(),native.end()); }
    std::sort(saved.begin(),saved.end(),[](const auto& a,const auto& b){return a.id<b.id;});validateMods(saved);return saved;
}
void ModRuntime::restore(const std::vector<SavedMod>& saved) {
    validateMods(saved);auto packs=impl_->packs;std::vector<SavedMod> missing;
    auto diagnostics=impl_->diagnostics;
    // Use fresh VMs so a failed migration cannot leak Lua global changes into
    // the running session. Commit the entire restored mod set together.
    std::vector<std::unique_ptr<Script>> scripts;scripts.reserve(packs.size());
    for(auto& pack:packs) {
        pack.saved.state.clear();const auto path=pack.directory/"init.lua";
        scripts.push_back(std::filesystem::exists(path)?std::make_unique<Script>(path):nullptr);
    }
    for(const auto& mod:saved) {
        if(impl_->native&&impl_->native->contains(mod.id)) continue;
        const auto found=std::find_if(packs.begin(),packs.end(),[&](const auto& pack){return pack.saved.id==mod.id;});
        if(found==packs.end()) { missing.push_back(mod);diagnostics.push_back("Missing saved mod "+mod.id+" "+mod.version+"; its state/content keys are retained"); }
        else {
            if(found->saved.stateVersion<mod.stateVersion) throw std::runtime_error("Mod "+mod.id+": saved state version is newer than the installed pack; downgrade refused");
            if(found->saved.version!=mod.version) diagnostics.push_back("Saved mod "+mod.id+" changed from "+mod.version+" to "+found->saved.version);
            found->saved.state=mod.state;
            if(found->saved.stateVersion>mod.stateVersion) {
                auto* script=scripts[static_cast<std::size_t>(found-packs.begin())].get();
                if(!script) throw std::runtime_error("Mod "+mod.id+": state version upgrade requires an init.lua migration");
                if(const auto error=script->migrate(mod,found->saved)) throw std::runtime_error("Mod "+mod.id+" migration failed: "+*error);
                diagnostics.push_back("Migrated mod "+mod.id+" state "+std::to_string(mod.stateVersion)+" -> "+std::to_string(found->saved.stateVersion));
            }
        }
    }
    auto combined=missing;for(const auto& pack:packs) combined.push_back(pack.saved);
    if(impl_->native) { auto native=impl_->native->save();for(auto& record:native) for(const auto& old:saved) if(old.id==record.id) record.state=old.state;combined.insert(combined.end(),native.begin(),native.end()); }
    validateMods(combined);if(impl_->native) impl_->native->restore(saved);
    impl_->packs=std::move(packs);impl_->missing=std::move(missing);impl_->scripts=std::move(scripts);impl_->diagnostics=std::move(diagnostics);
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
    if(impl_->native) { auto native=impl_->native->dispatch(event,context);messages.insert(messages.end(),std::make_move_iterator(native.begin()),std::make_move_iterator(native.end())); }
    return messages;
}
void ModRuntime::installCommands(commands::Registry& registry) const { if(impl_->native) impl_->native->installCommands(registry); }
std::vector<std::string> ModRuntime::takeDiagnostics() { auto result=std::exchange(impl_->diagnostics,{});if(impl_->native) { auto native=impl_->native->diagnostics();result.insert(result.end(),std::make_move_iterator(native.begin()),std::make_move_iterator(native.end())); }return result; }
std::size_t ModRuntime::size() const { return impl_->packs.size()+(impl_->native?impl_->native->save().size():0); }
const ContentAtlas& ModRuntime::atlas() const { return impl_->atlas; }
std::vector<std::string> ModRuntime::order() const { std::vector<std::string> result;for(const auto& pack:impl_->packs) result.push_back(pack.saved.id);if(impl_->native) for(const auto& pack:impl_->native->save()) result.push_back(pack.id);return result; }
}
