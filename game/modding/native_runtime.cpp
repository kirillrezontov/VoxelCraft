#include "game/modding/native_runtime.hpp"
#include "game/modding/runtime.hpp"
#include "game/commands/commands.hpp"
#include "engine/platform/dynamic_library.hpp"
#include "engine/core/paths.hpp"
#include "engine/core/utf8.hpp"
#include "voxelcraft/native_mod.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <functional>
#include <set>
#include <thread>
#include <utility>

namespace vc {
namespace {
struct ApiError : std::runtime_error {
    vc_mod_status status;
    ApiError(vc_mod_status value,std::string message):std::runtime_error(std::move(message)),status(value) {}
};
void require(bool value,std::string message,vc_mod_status status=VC_MOD_INVALID) {
    if(!value) throw ApiError(status,std::move(message));
}
std::string string(vc_mod_string value,std::size_t limit=256) {
    require(value.size<=limit&&(!value.size||value.data),"Invalid native string length");
    std::string result(value.data?value.data:"",value.size);
    require(result.find('\0')==std::string::npos&&utf8::valid(result),"Native strings must be bounded UTF-8");return result;
}
vc_mod_string borrowed(const std::string& value) { return {value.data(),static_cast<uint32_t>(value.size())}; }
std::filesystem::path contained(const std::filesystem::path& root,const std::filesystem::path& file) {
    auto resolved=std::filesystem::canonical(file);auto cursor=resolved.begin();
    for(const auto& part:root) { require(cursor!=resolved.end()&&*cursor==part,"Native library escapes its pack");++cursor; }
    return resolved;
}
}

std::vector<NativeModInfo> NativeModRuntime::discover(const std::filesystem::path& root) {
    if(root.empty()) return {};
    require(std::filesystem::is_directory(root),"Native mod root does not exist");
    const auto canonical=std::filesystem::canonical(root);std::vector<NativeModInfo> result;std::set<std::string> ids;std::size_t scanned{};
    for(const auto& entry:std::filesystem::directory_iterator(canonical)) {
        require(++scanned<=1024,"Native discovery entry budget",VC_MOD_LIMIT);
        if(!entry.is_directory()) continue;
        const auto directory=contained(canonical,entry.path());
        const auto manifest=directory/"native.json";if(!std::filesystem::exists(manifest)) continue;
        require(result.size()<32,"At most 32 native mods are supported",VC_MOD_LIMIT);
        require(std::filesystem::file_size(manifest)<=16384,"Native manifest exceeds 16 KiB",VC_MOD_LIMIT);
        std::ifstream input(contained(canonical,manifest),std::ios::binary);require(static_cast<bool>(input),"Native manifest read failed");
        std::string bytes(16385,'\0');input.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
        require(!input.bad()&&input.gcount()<=16384,"Native manifest grew beyond limit",VC_MOD_LIMIT);
        bytes.resize(static_cast<std::size_t>(input.gcount()));
        const auto data=nlohmann::json::parse(bytes,[](int depth,auto,auto&){require(depth<=16,"Native manifest nesting limit");return true;});
        require(data.is_object(),"Native manifest must be an object");
        for(const auto& [key,value]:data.items()) { (void)value;require(key=="id"||key=="version"||key=="abi"||key=="library"||key=="dependencies","Unknown native manifest field"); }
        NativeModInfo info;info.id=data.at("id").get<std::string>();info.version=data.at("version").get<std::string>();
        require(validModId(info.id)&&validModVersion(info.version),"Invalid native module identity");
        require(ids.insert(info.id).second,"Duplicate native module ID",VC_MOD_DUPLICATE);
        require(data.at("abi").is_number_unsigned()||data.at("abi").is_number_integer(),"Native ABI must be an integer");
        require(data.at("abi").get<int64_t>()==VC_MOD_ABI_MAJOR,"Unsupported native ABI major",VC_MOD_VERSION);
#ifdef _WIN32
        const auto library=data.at("library").at("windows").get<std::string>();
        constexpr std::string_view suffix=".dll";
#else
        const auto library=data.at("library").at("linux").get<std::string>();
        constexpr std::string_view suffix=".so";
#endif
        require(!library.empty()&&library.size()<=128&&library.ends_with(suffix)&&
            std::all_of(library.begin(),library.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.';}),"Native library must be a portable local basename");
        info.directory=contained(canonical,entry.path());info.library=contained(info.directory,info.directory/utf8Path(library));
        require(std::filesystem::is_regular_file(info.library),"Native library is not a regular file");
        if(data.contains("dependencies")) {
            require(data["dependencies"].is_object()&&data["dependencies"].size()<=32,"Invalid native dependencies");
            for(const auto& [id,version]:data["dependencies"].items()) {
                const auto v=version.get<std::string>();require(validModId(id)&&validModVersion(v)&&id!=info.id,"Invalid native dependency");info.dependencies.emplace(id,v);
            }
        }
        result.push_back(std::move(info));
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.id<b.id;});return result;
}

struct NativeModRuntime::State {
    struct Execution {
        voxel::World& world;const voxel::BlockRegistry& blocks;
        std::span<const voxel::Aabb> occupied;
        const voxel::Aabb* primary{};
        std::vector<std::pair<voxel::Position,voxel::BlockId>> edits;
        std::vector<std::string> messages;
        bool failed{};
        std::size_t maxEdits{64},maxMessages{16};
    };
    struct Item { std::string id,block,name;uint16_t stack; };
    struct RecipeData { std::string id,output;uint16_t count;std::vector<std::pair<std::string,uint16_t>> ingredients; };
    struct Callback {
        vc_mod_token token{};bool enabled{true};uint32_t events{};
        vc_mod_event_fn event{};vc_mod_command_fn command{};void* user{};
        std::string id,description;uint32_t permission{},minimum{},maximum{};
    };
    struct Module {
        std::unique_ptr<DynamicLibrary> library; // Retired after callbacks and plugin state.
        State& owner;NativeModInfo info;SavedMod saved;
        vc_mod_host host{};vc_mod_api api{};bool valid{},started{},failed{};unsigned blockCount{};
        std::string error;std::vector<Callback> callbacks;std::vector<Item> items;std::vector<RecipeData> recipes;
        Execution* execution{};
        Module(State& state,NativeModInfo metadata):owner(state),info(std::move(metadata)),saved{info.id,info.version,{}} {}
    };
    voxel::BlockRegistry blocks;
    std::vector<std::unique_ptr<Module>> modules;
    std::vector<std::string> messages;
    std::thread::id thread=std::this_thread::get_id();
    vc_mod_token next{1};bool registering{true},stopped{};unsigned dispatchDepth{};
    explicit State(const voxel::BlockRegistry& registry):blocks(registry) {}
    ~State() { stop(); }
    void stop() noexcept {
        if(stopped) return;
        stopped=true;
        for(auto& m:modules) for(auto& c:m->callbacks) c.enabled=false;
        for(auto it=modules.rbegin();it!=modules.rend();++it) {
            auto& m=**it;
            try { if(m.started&&m.api.stop) m.api.stop(m.api.user); } catch(...) {}
            try { if(m.valid&&m.api.destroy) m.api.destroy(m.api.user); } catch(...) {}
            m.started=m.valid=false;m.api={};m.callbacks.clear();
        }
        for(auto it=modules.rbegin();it!=modules.rend();++it) (*it)->library.reset();
    }
    void ownerThread() const { require(thread==std::this_thread::get_id(),"Native operation requires owner thread",VC_MOD_WRONG_THREAD); }
    static void own(Module& m,const std::string& id) {
        require(voxel::BlockRegistry::validIdentifier(id)&&id.starts_with(m.info.id+':'),"Native registration must use its module namespace");
    }
    static void registration(Module& m) { require(m.owner.registering&&!m.owner.stopped,"Content registration is closed",VC_MOD_UNAVAILABLE); }
    template<class Function> static vc_mod_status guard(void* context,Function body) noexcept {
        if(!context) return VC_MOD_INVALID;
        auto& m=*static_cast<Module*>(context);
        if(m.owner.thread!=std::this_thread::get_id()) return VC_MOD_WRONG_THREAD;
        try { require(!m.owner.stopped,"Native runtime is stopped",VC_MOD_UNAVAILABLE);body(m);m.error.clear();return VC_MOD_OK; }
        catch(const ApiError& e) { fail(m,e.what());return e.status; }
        catch(const std::exception& e) { fail(m,e.what());return VC_MOD_FAILED; }
        catch(...) { fail(m,"Unknown native host error");return VC_MOD_FAILED; }
    }
    static void fail(Module& m,const char* text) noexcept {
        if(m.execution) m.execution->failed=true;else if(m.owner.registering) m.failed=true;
        try { m.error=utf8::singleLine(text,512); } catch(...) {}
    }
    static vc_mod_status VC_MOD_CALL log(void* context,vc_mod_string text) noexcept {
        return guard(context,[&](Module& m){const auto message=string(text,4096);require(m.owner.messages.size()<1024,"Native diagnostic limit",VC_MOD_LIMIT);m.owner.messages.push_back(m.info.id+": "+message);});
    }
    static vc_mod_status VC_MOD_CALL block(void* context,const vc_mod_block* value) noexcept {
        return guard(context,[&](Module& m){registration(m);require(value&&value->struct_size>=sizeof(*value),"Native block table is too small");
            voxel::BlockDefinition b;b.identifier=string(value->id);own(m,b.identifier);
            require(!m.owner.blocks.find(b.identifier),"Duplicate native block",VC_MOD_DUPLICATE);
            require(m.blockCount<128,"Native block registration limit",VC_MOD_LIMIT);
            require(value->solid<=1&&value->opaque<=1&&value->emission<=15,"Invalid native block flags");
            b.solid=value->solid!=0;b.opaque=value->opaque!=0;b.emission=static_cast<uint8_t>(value->emission);
            for(int i=0;i<3;++i) { require(std::isfinite(value->color[i])&&value->color[i]>=0&&value->color[i]<=1,"Invalid native block color");b.color[i]=value->color[i]; }
            b.presentation.name=string(value->name,128);m.owner.blocks.add(std::move(b));++m.blockCount;
        });
    }
    static vc_mod_status VC_MOD_CALL item(void* context,const vc_mod_item* value) noexcept {
        return guard(context,[&](Module& m){registration(m);require(value&&value->struct_size>=sizeof(*value),"Native item table is too small");
            Item i{string(value->id),string(value->block),string(value->name,128),0};own(m,i.id);
            require(value->stack_size>=1&&value->stack_size<=64,"Native item stack limit");i.stack=static_cast<uint16_t>(value->stack_size);
            require(!m.owner.blocks.find(i.id),"Block items are registered automatically",VC_MOD_DUPLICATE);
            if(!i.block.empty()) require(m.owner.blocks.find(i.block).has_value(),"Native item references an unknown block");
            for(const auto& module:m.owner.modules) for(const auto& old:module->items) require(old.id!=i.id,"Duplicate native item",VC_MOD_DUPLICATE);
            require(m.items.size()<128,"Native item limit",VC_MOD_LIMIT);m.items.push_back(std::move(i));
        });
    }
    static vc_mod_status VC_MOD_CALL recipe(void* context,const vc_mod_recipe* value) noexcept {
        return guard(context,[&](Module& m){registration(m);require(value&&value->struct_size>=sizeof(*value),"Native recipe table is too small");
            RecipeData r{string(value->id,24),string(value->output),0,{}};own(m,r.id);
            require(value->count>=1&&value->count<=64&&value->ingredient_count>=1&&value->ingredient_count<=16&&value->ingredients,"Native recipe limits");r.count=static_cast<uint16_t>(value->count);
            for(uint32_t n=0;n<value->ingredient_count;++n) { const auto& v=value->ingredients[n];require(v.count>=1&&v.count<=64,"Invalid native ingredient count");r.ingredients.emplace_back(string(v.item),static_cast<uint16_t>(v.count)); }
            for(const auto& module:m.owner.modules) for(const auto& old:module->recipes) require(old.id!=r.id,"Duplicate native recipe",VC_MOD_DUPLICATE);
            require(m.recipes.size()<128,"Native recipe limit",VC_MOD_LIMIT);m.recipes.push_back(std::move(r));
        });
    }
    static vc_mod_status VC_MOD_CALL command(void* context,const vc_mod_command* value,vc_mod_token* token) noexcept {
        return guard(context,[&](Module& m){registration(m);require(value&&value->struct_size>=sizeof(*value)&&token&&value->invoke,"Invalid native command table");
            Callback c;c.id=string(value->id,64);own(m,c.id);
            require(std::all_of(c.id.begin(),c.id.end(),[](char ch){return (ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')||ch==':'||ch=='_'||ch=='-';}),"Invalid native command name");
            c.description=string(value->description,240);require(value->permission<=3&&value->minimum_arguments<=value->maximum_arguments&&value->maximum_arguments<=16,"Native command limits");
            for(const auto& module:m.owner.modules) for(const auto& old:module->callbacks) require(old.id.empty()||old.id!=c.id,"Duplicate native command",VC_MOD_DUPLICATE);
            c.command=value->invoke;c.user=value->user;c.permission=value->permission;c.minimum=value->minimum_arguments;c.maximum=value->maximum_arguments;add(m,std::move(c),token);
        });
    }
    static void add(Module& m,Callback callback,vc_mod_token* output) {
        require(m.callbacks.size()<64&&m.owner.next<4096,"Native callback limit",VC_MOD_LIMIT);
        callback.token=m.owner.next++;*output=callback.token;m.callbacks.push_back(std::move(callback));
    }
    static vc_mod_status VC_MOD_CALL event(void* context,uint32_t mask,vc_mod_event_fn callback,void* user,vc_mod_token* token) noexcept {
        return guard(context,[&](Module& m){registration(m);require(mask&&!(mask&~31u)&&callback&&token,"Invalid native event registration");Callback c;c.events=mask;c.event=callback;c.user=user;add(m,std::move(c),token);});
    }
    static vc_mod_status VC_MOD_CALL unregister(void* context,vc_mod_token token) noexcept {
        return guard(context,[&](Module& m){const auto found=std::find_if(m.callbacks.begin(),m.callbacks.end(),[&](const auto& c){return c.token==token;});require(found!=m.callbacks.end(),"Unknown or foreign native callback handle");found->enabled=false;});
    }
    static voxel::Position position(vc_mod_position p) {
        constexpr int64_t limit=1000000000000000LL;
        require(p.x>=-limit&&p.x<=limit&&p.y>=-limit&&p.y<=limit&&p.z>=-limit&&p.z<=limit,"Native coordinate limit");return {p.x,p.y,p.z};
    }
    static vc_mod_status VC_MOD_CALL get(void* context,vc_mod_position p,vc_mod_string* output) noexcept {
        return guard(context,[&](Module& m){require(m.execution&&output,"World access requires an active callback",VC_MOD_UNAVAILABLE);
            auto& e=*m.execution;const auto at=position(p);require(e.world.loaded(at),"Native world data is not resident",VC_MOD_UNAVAILABLE);
            auto id=e.world.block(at);for(const auto& edit:e.edits) if(edit.first==at) id=edit.second;
            *output=borrowed(e.blocks.definition(id).identifier);
        });
    }
    static vc_mod_status VC_MOD_CALL set(void* context,vc_mod_position p,vc_mod_string id) noexcept {
        return guard(context,[&](Module& m){require(m.execution,"World access requires an active callback",VC_MOD_UNAVAILABLE);
            auto& e=*m.execution;const auto at=position(p);const auto block=e.blocks.require(string(id));require(e.world.loaded(at),"Native edit requires resident world data",VC_MOD_UNAVAILABLE);
            if(e.blocks.definition(block).solid) { const glm::dvec3 low{static_cast<double>(at.x),static_cast<double>(at.y),static_cast<double>(at.z)};const voxel::Aabb cell{low,low+glm::dvec3(1)};
                if(e.primary) require(!cell.intersects(*e.primary),"Native edit would intersect a player");
                for(const auto& body:e.occupied) require(!cell.intersects(body),"Native edit would intersect a player"); }
            for(auto& edit:e.edits) if(edit.first==at) { edit.second=block;return; }
            require(e.edits.size()<e.maxEdits,"Native edit budget",VC_MOD_LIMIT);e.edits.emplace_back(at,block);
        });
    }
    static vc_mod_status VC_MOD_CALL notify(void* context,vc_mod_string text) noexcept {
        return guard(context,[&](Module& m){require(m.execution,"Notifications require an active callback",VC_MOD_UNAVAILABLE);require(m.execution->messages.size()<m.execution->maxMessages,"Native notification budget",VC_MOD_LIMIT);m.execution->messages.push_back(m.info.id+": "+utf8::singleLine(string(text,240),240));});
    }
    static vc_mod_string VC_MOD_CALL error(void* context) noexcept {
        static const std::string invalid="Invalid native context/thread";
        if(!context) return borrowed(invalid);
        auto& m=*static_cast<Module*>(context);return borrowed(m.owner.thread==std::this_thread::get_id()?m.error:invalid);
    }
    void load(Module& m) {
        m.host={sizeof(vc_mod_host),VC_MOD_ABI_MAJOR,VC_MOD_ABI_MINOR,0,&m,log,block,item,recipe,command,event,unregister,get,set,notify,error};
        m.library=std::make_unique<DynamicLibrary>(m.info.library);
        const auto query=reinterpret_cast<vc_mod_query_fn>(m.library->symbol("vc_mod_query_v1"));
        m.api.struct_size=sizeof(m.api);vc_mod_status status=VC_MOD_FAILED;bool threw=false;
        try { status=query(VC_MOD_ABI_MAJOR,&m.host,&m.api); } catch(...) { threw=true; }
        require(!threw,"Native query threw across the C ABI",VC_MOD_FAILED);
        require(status==VC_MOD_OK,"Native query failed: "+m.info.id,VC_MOD_VERSION);
        require(m.api.struct_size>=sizeof(m.api)&&m.api.abi_major==VC_MOD_ABI_MAJOR&&m.api.reserved==0,"Incompatible native function table",VC_MOD_VERSION);
        require(!m.api.user||m.api.destroy,"Native state requires an explicit destroy function");m.valid=true;
        require(!m.failed,"Native registration failed: "+m.error);
        // Mark before invoking start so failed/partially started modules receive stop.
        m.started=true;
        if(m.api.start) {
            try { status=m.api.start(m.api.user); } catch(...) { status=VC_MOD_FAILED; }
            require(status==VC_MOD_OK,"Native start failed: "+m.info.id);
        }
        require(!m.failed,"Native registration failed: "+m.error);
    }
    static void commit(Execution& execution) {
        require(!execution.failed,"Native callback transaction failed",VC_MOD_FAILED);
        for(const auto& [position,block]:execution.edits) execution.world.setBlock(position,block);
    }
    struct Invocation {
        State& state;Module& module;
        Invocation(State& s,Module& m,Execution& execution):state(s),module(m) { require(!m.execution,"Native callback reentry is unsupported");++state.dispatchDepth;m.execution=&execution; }
        ~Invocation() { module.execution=nullptr;--state.dispatchDepth; }
    };
};

NativeModRuntime::NativeModRuntime(const std::filesystem::path& root,voxel::BlockRegistry& registry,std::span<const SavedMod> dataMods,const ItemRegistry* dataItems)
    :state_(std::make_shared<State>(registry)) {
    auto metadata=discover(root);require(metadata.size()+dataMods.size()<=32,"Combined mod count exceeds 32",VC_MOD_LIMIT);
    std::map<std::string,NativeModInfo,std::less<>> found;std::map<std::string,std::string,std::less<>> external;
    for(const auto& mod:dataMods) external.emplace(mod.id,mod.version);
    for(auto& info:metadata) { require(!external.contains(info.id),"Native/data mod namespace collision",VC_MOD_DUPLICATE);found.emplace(info.id,std::move(info)); }
    std::map<std::string,int,std::less<>> visited;std::vector<NativeModInfo> ordered;
    std::function<void(const std::string&)> visit=[&](const std::string& id){
        require(visited[id]!=1,"Native dependency cycle");if(visited[id]==2) return;visited[id]=1;
        for(const auto& [dependency,version]:found.at(id).dependencies) {
            if(external.contains(dependency)) { require(external.at(dependency)==version,"Native dependency version mismatch");continue; }
            require(found.contains(dependency)&&found.at(dependency).version==version,"Missing/incompatible native dependency");visit(dependency);
        }
        visited[id]=2;ordered.push_back(found.at(id));
    };
    for(const auto& [id,info]:found) { (void)info;visit(id); }
    try {
        for(auto& info:ordered) { state_->modules.push_back(std::make_unique<State::Module>(*state_,std::move(info)));state_->load(*state_->modules.back()); }
        state_->registering=false;
        // Validate item/recipe references before publishing any registrations.
        auto items=builtinItems(state_->blocks);
        if(dataItems) for(std::size_t i=0;i<dataItems->size();++i) {
            const auto& item=dataItems->definition(static_cast<ItemId>(i));if(!items.find(item.identifier)) items.add(item);
        }
        auto recipes=builtinRecipes(items);installItems(items,state_->blocks,recipes);
        auto ready=state_->blocks;registry=std::move(ready);
    } catch(...) { state_->stop();throw; }
}
NativeModRuntime::~NativeModRuntime() { if(state_) state_->stop(); }
void NativeModRuntime::shutdown() { state_->ownerThread();require(!state_->dispatchDepth,"Cannot unload inside native dispatch",VC_MOD_UNAVAILABLE);state_->stop(); }
void NativeModRuntime::installItems(ItemRegistry& destination,const voxel::BlockRegistry& blocks,std::vector<Recipe>& destinationRecipes) const {
    state_->ownerThread();
    auto items=destination;auto recipes=destinationRecipes;
    for(const auto& m:state_->modules) for(const auto& item:m->items) {
        require(!items.find(item.id),"Duplicate native item at installation",VC_MOD_DUPLICATE);
        ItemDefinition definition;definition.identifier=item.id;definition.stackSize=item.stack;definition.presentation.name=item.name;
        if(!item.block.empty()) definition.block=blocks.require(item.block);
        items.add(std::move(definition));
    }
    for(const auto& m:state_->modules) for(const auto& recipe:m->recipes) {
        require(std::none_of(recipes.begin(),recipes.end(),[&](const auto& old){return old.name==recipe.id;}),"Duplicate native recipe at installation",VC_MOD_DUPLICATE);
        Recipe value{recipe.id,{},items.require(recipe.output),recipe.count};
        require(value.count<=items.definition(value.output).stackSize,"Native recipe exceeds output stack");
        std::set<ItemId> unique;for(const auto& [id,count]:recipe.ingredients) { const auto ingredient=items.require(id);require(unique.insert(ingredient).second,"Duplicate native ingredient");value.ingredients.push_back({ingredient,count}); }
        recipes.push_back(std::move(value));
    }
    require(recipes.size()<=128,"At most 128 total recipes are supported",VC_MOD_LIMIT);
    destination=std::move(items);destinationRecipes=std::move(recipes);
}
void NativeModRuntime::installCommands(commands::Registry& registry) const {
    state_->ownerThread();const std::weak_ptr<State> weak=state_;
    for(const auto& m:state_->modules) for(const auto& c:m->callbacks) {
        if(!c.command||!c.enabled) continue;
        commands::Definition d;d.name=c.id;d.description=c.description;d.usage=c.id;d.permission=static_cast<commands::Permission>(c.permission);d.minimum=c.minimum;d.maximum=c.maximum;
        const auto token=c.token;
        d.execute=[weak,token](commands::Context& context,std::span<const std::string> arguments) {
            auto state=weak.lock();if(!state||state->stopped) return commands::Result::error(commands::Code::Unavailable,"Native command is no longer active");
            state->ownerThread();
            for(auto& module:state->modules) for(auto& callback:module->callbacks) if(callback.token==token&&callback.enabled) {
                require(context.world&&context.blocks,"Native command needs a world",VC_MOD_UNAVAILABLE);
                std::array<voxel::Aabb,8> occupied{};std::size_t count{};
                for(const auto& player:context.players) if(player.controller) {
                    require(count<occupied.size(),"Native player context limit");occupied[count++]=player.controller->bounds();
                }
                State::Execution execution{*context.world,*context.blocks,std::span(occupied.data(),count),nullptr,{},{},false};
                State::Invocation active(*state,*module,execution);
                require(arguments.size()<=16,"Native argument limit");std::array<vc_mod_string,16> args{};
                for(std::size_t i=0;i<arguments.size();++i) args[i]=borrowed(arguments[i]);
                const vc_mod_command_call call{sizeof(vc_mod_command_call),static_cast<uint32_t>(arguments.size()),args.data(),borrowed(context.issuer),static_cast<uint32_t>(context.permission)};
                vc_mod_string reply{};vc_mod_status status=VC_MOD_FAILED;
                try { status=callback.command(callback.user,&call,&reply); } catch(...) { execution.failed=true; }
                if(status!=VC_MOD_OK||execution.failed) return commands::Result::error(commands::Code::InternalFailure,"Native command failed; staged edits discarded");
                auto message=utf8::singleLine(string(reply,240),240);State::commit(execution);
                if(context.broadcast) for(const auto& text:execution.messages) context.broadcast(text);
                return commands::Result::ok(std::move(message));
            }
            return commands::Result::error(commands::Code::Unavailable,"Native callback was unregistered");
        };
        registry.add(std::move(d));
    }
}
std::vector<std::string> NativeModRuntime::dispatch(const ModEvent& event,ModContext context) {
    state_->ownerThread();std::vector<std::string> result;if(state_->stopped) return result;
    const auto ordinal=static_cast<unsigned>(event.kind);require(ordinal<=4&&std::isfinite(event.seconds)&&event.seconds>=0,"Invalid native event");
    const uint32_t kind=1u<<ordinal;
    const vc_mod_event data{sizeof(vc_mod_event),kind,{event.position.x,event.position.y,event.position.z},borrowed(event.identifier),event.seconds};
    std::size_t committedEdits{};
    for(auto& module:state_->modules) for(auto& callback:module->callbacks) if(callback.enabled&&callback.event&&(callback.events&kind)) {
        State::Execution execution{context.world,context.blocks,context.players,&context.player,{},{},false};
        execution.maxEdits=std::min<std::size_t>(64,4096-committedEdits);execution.maxMessages=std::min<std::size_t>(16,256-result.size());
        State::Invocation active(*state_,*module,execution);
        vc_mod_status status=VC_MOD_FAILED;
        try { status=callback.event(callback.user,&data); } catch(...) { execution.failed=true; }
        if(status!=VC_MOD_OK||execution.failed) { callback.enabled=false;state_->messages.push_back("Disabled native callback "+module->info.id+": staged effects discarded");continue; }
        State::commit(execution);committedEdits+=execution.edits.size();for(auto& message:execution.messages) result.push_back(std::move(message));
    }
    return result;
}
std::vector<SavedMod> NativeModRuntime::save() const { state_->ownerThread();std::vector<SavedMod> result;for(const auto& m:state_->modules) result.push_back(m->saved);return result; }
void NativeModRuntime::restore(std::span<const SavedMod> saved) {
    auto next=save();
    for(auto& record:next) for(const auto& old:saved) if(old.id==record.id) {
        require(old.stateVersion==1,"Native state version is unsupported");record.state=old.state;
    }
    validateMods(next);for(std::size_t i=0;i<next.size();++i) state_->modules[i]->saved=std::move(next[i]);
}
bool NativeModRuntime::contains(std::string_view id) const { return std::any_of(state_->modules.begin(),state_->modules.end(),[&](const auto& m){return m->info.id==id;}); }
std::vector<std::string> NativeModRuntime::diagnostics() { state_->ownerThread();return std::exchange(state_->messages,{}); }
}
