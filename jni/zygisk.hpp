#pragma once

#include <jni.h>

namespace zygisk {

enum Option : uint32_t {
    FORCE_DENYLIST_UNMOUNT = 1 << 0,
    DLCLOSE_MODULE_LIBRARY = 1 << 1,
};

struct AppSpecializeArgs {
    jint &uid;
    jint &gid;
    jintArray &gids;
    jint &runtime_flags;
    jobjectArray &rlimits;
    jint &mount_external;
    jstring &se_info;
    jstring &nice_name;
    jintArray &fds_to_close;
    jintArray &fds_to_ignore;
    jboolean &is_child_zygote;
    jstring &instruction_set;
    jstring &app_data_dir;
    jboolean &is_top_app;
    jobjectArray &pkg_data_info_list;
    jobjectArray &whitelisted_data_info_list;
    jboolean &bind_mount_data_dirs;
    jboolean &bind_mount_app_storage_dirs;
};

struct ServerSpecializeArgs {
    jint &uid;
    jint &gid;
    jintArray &gids;
    jint &runtime_flags;
    jobjectArray &rlimits;
    jlong &permitted_capabilities;
    jlong &effective_capabilities;
};

class Api {
public:
    virtual int getApiVersion() = 0;
    virtual void setOption(Option opt) = 0;
};

class ModuleBase {
public:
    virtual void onLoad(Api *api, JNIEnv *env) {}
    virtual void preAppSpecialize(AppSpecializeArgs *args) {}
    virtual void postAppSpecialize(const AppSpecializeArgs *args) {}
    virtual void preServerSpecialize(ServerSpecializeArgs *args) {}
    virtual void postServerSpecialize(const ServerSpecializeArgs *args) {}
};

}

#define REGISTER_ZYGISK_MODULE(clazz) \
extern "C" [[gnu::visibility("default")]] void zygisk_module_entry(zygisk::Api *api, JNIEnv *env) { \
    static clazz module; \
    api->onLoad(api, env); \
}
