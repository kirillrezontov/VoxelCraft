#include <voxelcraft/native_mod.hpp>
#include <charconv>
#include <array>

using voxelcraft::sdk::text;
class Sample {
public:
    explicit Sample(voxelcraft::sdk::Host host):host_(host) {
        const vc_mod_block beacon{sizeof(vc_mod_block),text("native_sample:beacon"),{0.2f,0.8f,0.95f},1,1,12,text("Native beacon")};
        host_.check(host_.api().register_block(host_.api().context,&beacon));
        const vc_mod_item dust{sizeof(vc_mod_item),text("native_sample:dust"),{},text("Native dust"),64};
        host_.check(host_.api().register_item(host_.api().context,&dust));
        const vc_mod_ingredient ingredient{text("voxelcraft:stone"),1};
        const vc_mod_recipe recipe{sizeof(vc_mod_recipe),text("native_sample:beacon"),text("native_sample:beacon"),1,1,&ingredient};
        host_.check(host_.api().register_recipe(host_.api().context,&recipe));
        host_.check(host_.api().register_event(host_.api().context,VC_MOD_EVENT_LOAD,onLoad,this,&event_));
        vc_mod_command ping{sizeof(vc_mod_command),text("native_sample:ping"),text("Verify the native SDK is active"),0,0,0,onPing,this};
        host_.check(host_.api().register_command(host_.api().context,&ping,&ping_));
        vc_mod_command stamp{sizeof(vc_mod_command),text("native_sample:stamp"),text("Place a beacon at integer x y z"),2,3,3,onStamp,this};
        host_.check(host_.api().register_command(host_.api().context,&stamp,&stamp_));
        vc_mod_command retire{sizeof(vc_mod_command),text("native_sample:retire"),text("Retire this sample's ping and load callbacks"),2,0,0,onRetire,this};
        host_.check(host_.api().register_command(host_.api().context,&retire,&retire_));
    }
    void stop() {
        // Host shutdown already disables dispatch; explicit tokens also support
        // voluntary callback retirement while the module remains loaded.
        (void)host_.api().unregister_callback(host_.api().context,event_);
        (void)host_.api().unregister_callback(host_.api().context,ping_);
        (void)host_.api().unregister_callback(host_.api().context,stamp_);
        (void)host_.api().unregister_callback(host_.api().context,retire_);
    }
private:
    static vc_mod_status VC_MOD_CALL onLoad(void* user,const vc_mod_event*) noexcept {
        try { static_cast<Sample*>(user)->host_.notify("Native SDK sample is running");return VC_MOD_OK; } catch(...) { return VC_MOD_FAILED; }
    }
    static vc_mod_status VC_MOD_CALL onPing(void*,const vc_mod_command_call*,vc_mod_string* reply) noexcept {
        if(!reply) return VC_MOD_INVALID;*reply=text("Native SDK is running");return VC_MOD_OK;
    }
    static vc_mod_status VC_MOD_CALL onRetire(void* user,const vc_mod_command_call*,vc_mod_string* reply) noexcept {
        auto& sample=*static_cast<Sample*>(user);if(!reply) return VC_MOD_INVALID;
        const auto& api=sample.host_.api();
        if(api.unregister_callback(api.context,sample.ping_)!=VC_MOD_OK||api.unregister_callback(api.context,sample.event_)!=VC_MOD_OK) return VC_MOD_FAILED;
        *reply=text("Callbacks retired");return VC_MOD_OK;
    }
    static vc_mod_status VC_MOD_CALL onStamp(void* user,const vc_mod_command_call* call,vc_mod_string* reply) noexcept {
        try {
            if(!call||call->argument_count!=3||!reply) return VC_MOD_INVALID;
            std::array<int64_t,3> p{};
            for(std::size_t i=0;i<3;++i) {
                const auto value=call->arguments[i];if(!value.data||!value.size) return VC_MOD_INVALID;
                const auto parsed=std::from_chars(value.data,value.data+value.size,p[i]);
                if(parsed.ec!=std::errc{}||parsed.ptr!=value.data+value.size) return VC_MOD_INVALID;
            }
            auto& host=static_cast<Sample*>(user)->host_;
            const auto status=host.api().set_block(host.api().context,{p[0],p[1],p[2]},text("native_sample:beacon"));
            if(status!=VC_MOD_OK) return status;
            *reply=text("Placed a native beacon");return VC_MOD_OK;
        } catch(...) { return VC_MOD_FAILED; }
    }
    voxelcraft::sdk::Host host_;
    vc_mod_token event_{},ping_{},stamp_{},retire_{};
};
extern "C" VC_MOD_EXPORT vc_mod_status VC_MOD_CALL vc_mod_query_v1(uint32_t major,const vc_mod_host* host,vc_mod_api* output) {
    return voxelcraft::sdk::Entry<Sample>::query(major,host,output);
}
