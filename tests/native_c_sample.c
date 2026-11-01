#include <voxelcraft/native_mod.h>
#include <string.h>
static vc_mod_status VC_MOD_CALL loaded(void* user,const vc_mod_event* event) {
    const vc_mod_host* host=(const vc_mod_host*)user;
    const char message[]="Pure C ABI callback executed";
    (void)event;
    return host->notify(host->context,(vc_mod_string){message,(uint32_t)strlen(message)});
}
VC_MOD_EXPORT vc_mod_status VC_MOD_CALL vc_mod_query_v1(uint32_t major,const vc_mod_host* host,vc_mod_api* output) {
    vc_mod_token token=0;
    if(major!=1||!host||host->struct_size<sizeof(vc_mod_host)||!output||output->struct_size<sizeof(vc_mod_api)) return VC_MOD_VERSION;
    *output=(vc_mod_api){sizeof(vc_mod_api),1,0,0,0,0,0,0};
    return host->register_event(host->context,VC_MOD_EVENT_LOAD,loaded,(void*)host,&token);
}
