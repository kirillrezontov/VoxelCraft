#include <voxelcraft/native_mod.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <string>
#include <fstream>
#include <filesystem>
#ifndef VC_FIXTURE_ID
#define VC_FIXTURE_ID "fixture"
#endif
#ifndef VC_FIXTURE_MODE
#define VC_FIXTURE_MODE 0
#endif
namespace {
void record(const char* phase) {
    if(const char* file=std::getenv("VC_NATIVE_LIFECYCLE_FILE")) {
        const std::filesystem::path path(std::u8string(reinterpret_cast<const char8_t*>(file),std::strlen(file)));
        std::ofstream out(path,std::ios::binary|std::ios::app);out<<VC_FIXTURE_ID<<':'<<phase<<'\n';
    }
}
struct Unload { ~Unload() { record("unload"); } } unload;
vc_mod_status VC_MOD_CALL start(void*) { record("start");return VC_FIXTURE_MODE==3?VC_MOD_FAILED:VC_MOD_OK; }
void VC_MOD_CALL stop(void*) { record("stop"); }
void VC_MOD_CALL destroy(void*) { record("destroy"); }
vc_mod_status VC_MOD_CALL event(void* user,const vc_mod_event*) {
    const auto* host=static_cast<const vc_mod_host*>(user);record("event");
    const auto text=[](const char* p){return vc_mod_string{p,static_cast<uint32_t>(std::strlen(p))};};
    if(VC_FIXTURE_MODE==6) {
        vc_mod_status status=VC_MOD_OK;std::thread other([&]{status=host->log(host->context,text("foreign thread"));});other.join();
        if(status!=VC_MOD_WRONG_THREAD) return VC_MOD_FAILED;
        return host->notify(host->context,text("Owner thread enforced"));
    }
    (void)host->set_block(host->context,{0,1,0},text("voxelcraft:stone"));
    (void)host->notify(host->context,text("Must be rolled back"));
    (void)host->set_block(host->context,{100000,1,0},text("voxelcraft:stone"));
    return VC_MOD_OK; // Intentionally ignore the failed host call.
}
}
#if VC_FIXTURE_MODE != 9
extern "C" VC_MOD_EXPORT vc_mod_status VC_MOD_CALL vc_mod_query_v1(uint32_t,const vc_mod_host* host,vc_mod_api* output) {
    record("query");
    *output={sizeof(vc_mod_api),1,VC_FIXTURE_MODE==5?9u:0u,0,nullptr,start,stop,destroy};
    if(VC_FIXTURE_MODE==1) output->abi_major=99;
    if(VC_FIXTURE_MODE==2) output->struct_size=16;
    if(VC_FIXTURE_MODE==3||VC_FIXTURE_MODE==4) {
        const char id[]=VC_FIXTURE_ID ":block";
        const vc_mod_block block{sizeof(vc_mod_block),{id,sizeof(id)-1},{1,0,0},1,1,0,{}};
        (void)host->register_block(host->context,&block);
        if(VC_FIXTURE_MODE==4) (void)host->register_block(host->context,&block);
    }
    if(VC_FIXTURE_MODE==6||VC_FIXTURE_MODE==7) {
        vc_mod_token token{};(void)host->register_event(host->context,VC_MOD_EVENT_LOAD,event,const_cast<vc_mod_host*>(host),&token);
    }
    if(VC_FIXTURE_MODE==8) for(int i=0;i<129;++i) {
        const auto id=std::string(VC_FIXTURE_ID)+":b"+std::to_string(i);
        const vc_mod_block block{sizeof(vc_mod_block),{id.data(),static_cast<uint32_t>(id.size())},{1,0,0},1,1,0,{}};
        (void)host->register_block(host->context,&block);
    }
    return VC_MOD_OK;
}
#endif
