#pragma once
#include "voxelcraft/native_mod.h"
#include <stdexcept>
#include <string>
#include <string_view>
#include <limits>

namespace voxelcraft::sdk {
inline vc_mod_string text(std::string_view value) {
    if(value.size()>std::numeric_limits<uint32_t>::max()) throw std::length_error("SDK string too long");
    return {value.data(),static_cast<uint32_t>(value.size())};
}
class Host {
public:
    explicit Host(const vc_mod_host* api):api_(api) {
        if(!api||api->struct_size<sizeof(vc_mod_host)||api->abi_major!=VC_MOD_ABI_MAJOR||
            !api->last_error||!api->notify||!api->log||!api->register_block||!api->register_item||
            !api->register_recipe||!api->register_command||!api->register_event||
            !api->unregister_callback||!api->get_block||!api->set_block)
            throw std::invalid_argument("Unsupported VoxelCraft host API");
    }
    void check(vc_mod_status status) const {
        if(status==VC_MOD_OK) return;
        const auto error=api_->last_error(api_->context);
        throw std::runtime_error(error.data?std::string(error.data,error.size):"Native host operation failed");
    }
    const vc_mod_host& api() const { return *api_; }
    void notify(std::string_view value) const { check(api_->notify(api_->context,text(value))); }
private:
    const vc_mod_host* api_;
};
// Optional C++ conveniences stay inside the plugin. Its object never crosses ABI.
template<class Mod> struct Entry {
    static vc_mod_status VC_MOD_CALL start(void* p) noexcept {
        try { if constexpr(requires(Mod& m){m.start();}) static_cast<Mod*>(p)->start();return VC_MOD_OK; }
        catch(...) { return VC_MOD_FAILED; }
    }
    static void VC_MOD_CALL stop(void* p) noexcept {
        try { if constexpr(requires(Mod& m){m.stop();}) static_cast<Mod*>(p)->stop(); } catch(...) {}
    }
    static void VC_MOD_CALL destroy(void* p) noexcept { delete static_cast<Mod*>(p); }
    static vc_mod_status query(uint32_t major,const vc_mod_host* host,vc_mod_api* output) noexcept {
        if(major!=VC_MOD_ABI_MAJOR||!output||output->struct_size<sizeof(vc_mod_api)) return VC_MOD_VERSION;
        try {
            auto* instance=new Mod(Host(host));
            *output={sizeof(vc_mod_api),VC_MOD_ABI_MAJOR,VC_MOD_ABI_MINOR,0,instance,start,stop,destroy};
            return VC_MOD_OK;
        } catch(...) { return VC_MOD_FAILED; }
    }
};
}
