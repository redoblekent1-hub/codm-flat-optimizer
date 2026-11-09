#include <android/log.h>
#include <string>
#include <unistd.h>
#include <dlfcn.h>
#include <GLES2/gl2.h>
#include <pthread.h>
#include "zygisk.hpp"
#include "dobby.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "CODM_Garena_Flat", __VA_ARGS__)

void (*orig_glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels) = nullptr;
void (*orig_glTexParameteri)(GLenum target, GLenum pname, GLint param) = nullptr;

void hook_glTexParameteri(GLenum target, GLenum pname, GLint param) {
    if (pname == GL_TEXTURE_MIN_FILTER || pname == GL_TEXTURE_MAG_FILTER) {
        param = GL_NEAREST; // Clamp linear filtering to rigid pixelated blocks
    }
    orig_glTexParameteri(target, pname, param);
}

void hook_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels) {
    // 1. Flatten smoke/particle alphas down to their smallest mip (1x1)
    if (width <= 64 || height <= 64 || level > 0) {
        width = 1;
        height = 1;
    } 
    // 2. Strip standard terrain, textures, and grass into flat 2x2 solid blocks
    else if (width > 64 && height > 64) {
        width = 2;
        height = 2;
    }
    orig_glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

class CodmGarenaModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        const char* process_name = env->GetStringUTFChars(args->nice_name, nullptr);
        if (process_name) {
            std::string proc(process_name);
            env->ReleaseStringUTFChars(args->nice_name, process_name);

            // Target lock matching Garena variant
            if (proc == "com.garena.game.codm") {
                api->setOption(zygisk::Option::FORCE_DENYLIST_UNMOUNT);
                should_hook = true;
            }
        }
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs* args) override {
        if (!should_hook) return;

        // Monitor background threading context initialization safely
        pthread_t thread;
        pthread_create(&thread, nullptr, [](void*) -> void* {
            int retries = 0;
            void* libgles = nullptr;
            
            while (retries < 100 && !libgles) {
                libgles = dlopen("libGLESv3.so", RTLD_NOW);
                if (!libgles) libgles = dlopen("libGLESv2.so", RTLD_NOW);
                if (!libgles) {
                    usleep(100000); // 100ms interval delay
                    retries++;
                }
            }

            if (libgles) {
                LOGD("Garena CODM GLES driver isolated. Attaching graphic flattening hooks...");
                void* gles_glTexImage2D = dlsym(libgles, "glTexImage2D");
                void* gles_glTexParameteri = dlsym(libgles, "glTexParameteri");

                DobbyHook(gles_glTexImage2D, (dobby_dummy_func_t)hook_glTexImage2D, (dobby_dummy_func_t*)&orig_glTexImage2D);
                DobbyHook(gles_glTexParameteri, (dobby_dummy_func_t)hook_glTexParameteri, (dobby_dummy_func_t*)&orig_glTexParameteri);
            } else {
                LOGD("Failed to resolve GLES mapping (ensure engine fallback to GLES is active).");
            }
            return nullptr;
        }, nullptr);
    }

private:
    bool should_hook = false;
};

REGISTER_ZYGISK_MODULE(CodmGarenaModule);

