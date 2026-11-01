#include <voxelcraft/native_mod.h>
_Static_assert(sizeof(vc_mod_status)==4,"Status width");
_Static_assert(sizeof(vc_mod_token)==8,"Token width");
_Static_assert(offsetof(vc_mod_host,struct_size)==0,"Versioned prefix");
int main(void) { vc_mod_api api={0};api.struct_size=sizeof(api);return VC_MOD_ABI_MAJOR==1&&api.struct_size>16?0:1; }
