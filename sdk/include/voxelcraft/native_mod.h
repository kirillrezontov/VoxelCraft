#ifndef VOXELCRAFT_NATIVE_MOD_H
#define VOXELCRAFT_NATIVE_MOD_H
#include <stdint.h>
#include <stddef.h>
#if defined(_WIN32)
#define VC_MOD_EXPORT __declspec(dllexport)
#define VC_MOD_CALL __cdecl
#else
#define VC_MOD_EXPORT __attribute__((visibility("default")))
#define VC_MOD_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define VC_MOD_ABI_MAJOR 1u
#define VC_MOD_ABI_MINOR 0u
typedef int32_t vc_mod_status;
#define VC_MOD_OK 0
#define VC_MOD_INVALID 1
#define VC_MOD_UNAVAILABLE 2
#define VC_MOD_DUPLICATE 3
#define VC_MOD_VERSION 4
#define VC_MOD_FAILED 5
#define VC_MOD_WRONG_THREAD 6
#define VC_MOD_LIMIT 7
typedef uint64_t vc_mod_token;
typedef struct vc_mod_string { const char* data; uint32_t size; } vc_mod_string;
typedef struct vc_mod_position { int64_t x,y,z; } vc_mod_position;
typedef struct vc_mod_block {
    uint32_t struct_size;
    vc_mod_string id;
    float color[3];
    uint32_t solid,opaque,emission;
    vc_mod_string name;
} vc_mod_block;
typedef struct vc_mod_item {
    uint32_t struct_size;
    vc_mod_string id,block,name;
    uint32_t stack_size;
} vc_mod_item;
typedef struct vc_mod_ingredient { vc_mod_string item; uint32_t count; } vc_mod_ingredient;
typedef struct vc_mod_recipe {
    uint32_t struct_size;
    vc_mod_string id,output;
    uint32_t count,ingredient_count;
    const vc_mod_ingredient* ingredients;
} vc_mod_recipe;
#define VC_MOD_EVENT_LOAD 1u
#define VC_MOD_EVENT_TICK 2u
#define VC_MOD_EVENT_PLACE 4u
#define VC_MOD_EVENT_BREAK 8u
#define VC_MOD_EVENT_CRAFT 16u
typedef struct vc_mod_event {
    uint32_t struct_size,kind;
    vc_mod_position position;
    vc_mod_string identifier;
    double seconds;
} vc_mod_event;
typedef struct vc_mod_command_call {
    uint32_t struct_size,argument_count;
    const vc_mod_string* arguments;
    vc_mod_string issuer;
    uint32_t permission;
} vc_mod_command_call;
typedef vc_mod_status (VC_MOD_CALL *vc_mod_event_fn)(void*,const vc_mod_event*);
typedef vc_mod_status (VC_MOD_CALL *vc_mod_command_fn)(void*,const vc_mod_command_call*,vc_mod_string*);
typedef struct vc_mod_command {
    uint32_t struct_size;
    vc_mod_string id,description;
    /* 0 player, 1 moderator, 2 administrator, 3 server console. */
    uint32_t permission,minimum_arguments,maximum_arguments;
    vc_mod_command_fn invoke;
    void* user;
} vc_mod_command;
typedef struct vc_mod_host {
    uint32_t struct_size,abi_major,abi_minor,reserved;
    void* context;
    vc_mod_status (VC_MOD_CALL *log)(void*,vc_mod_string);
    vc_mod_status (VC_MOD_CALL *register_block)(void*,const vc_mod_block*);
    vc_mod_status (VC_MOD_CALL *register_item)(void*,const vc_mod_item*);
    vc_mod_status (VC_MOD_CALL *register_recipe)(void*,const vc_mod_recipe*);
    vc_mod_status (VC_MOD_CALL *register_command)(void*,const vc_mod_command*,vc_mod_token*);
    vc_mod_status (VC_MOD_CALL *register_event)(void*,uint32_t,vc_mod_event_fn,void*,vc_mod_token*);
    vc_mod_status (VC_MOD_CALL *unregister_callback)(void*,vc_mod_token);
    vc_mod_status (VC_MOD_CALL *get_block)(void*,vc_mod_position,vc_mod_string*);
    vc_mod_status (VC_MOD_CALL *set_block)(void*,vc_mod_position,vc_mod_string);
    vc_mod_status (VC_MOD_CALL *notify)(void*,vc_mod_string);
    vc_mod_string (VC_MOD_CALL *last_error)(void*);
} vc_mod_host;
typedef struct vc_mod_api {
    uint32_t struct_size,abi_major,abi_minor,reserved;
    void* user;
    vc_mod_status (VC_MOD_CALL *start)(void*);
    void (VC_MOD_CALL *stop)(void*);
    void (VC_MOD_CALL *destroy)(void*);
} vc_mod_api;
typedef vc_mod_status (VC_MOD_CALL *vc_mod_query_fn)(uint32_t,const vc_mod_host*,vc_mod_api*);
/* The only required export. Check major/table sizes before reading or writing.
 * Strings/arrays are borrowed for one call; copy retained data. All callbacks and
 * host operations run on the owning simulation thread. No exceptions cross ABI.
 * No hot unload is provided. stop must join plugin-owned threads before return.
 * The host copies registrations and owns their tokens until session shutdown.
 */
/* Define VC_MOD_EXPORT vc_mod_status VC_MOD_CALL vc_mod_query_v1(...) in the
 * plugin, with the signature above. The host resolves this symbol dynamically. */
#ifdef __cplusplus
}
#endif
#endif
