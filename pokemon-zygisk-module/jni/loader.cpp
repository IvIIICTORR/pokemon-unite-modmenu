/**
 * Zygisk Module Loader for Honor of Kings Mod Menu
 * ================================================
 * Detects com.levelinfinite.sgameGlobal process, then:
 *   1. Copies mod files to app-writable location (during preAppSpecialize with root)
 *   2. After app starts, loads classes.dex via DexClassLoader
 *   3. Calls com.android.support.Main.Start(context) to launch the mod menu
 *
 * Works with Kitsune Mask + Zygisk on MuMu Player emulator.
 */

#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <dirent.h>
#include <android/log.h>
#include <sys/stat.h>

#include "zygisk.hpp"

#define LOG_TAG "PUMod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)

static const char *TARGET_PACKAGE = "jp.pokemon.pokemonunite";

// Detect ABI at compile time
#if defined(__x86_64__)
    #define CURRENT_ABI "x86_64"
#elif defined(__i386__)
    #define CURRENT_ABI "x86"
#elif defined(__aarch64__)
    #define CURRENT_ABI "arm64-v8a"
#elif defined(__arm__)
    #define CURRENT_ABI "armeabi-v7a"
#else
    #define CURRENT_ABI "unknown"
#endif

// Cached paths (set in preAppSpecialize with root access)
static std::string g_localDex;       // /data/data/<pkg>/mod_cache/classes.dex
static std::string g_localSoDir;     // /data/data/<pkg>/mod_cache/lib/
static std::string g_modCacheDir;    // /data/data/<pkg>/mod_cache/
static bool g_filesCopied = false;

// ======================== File Utilities ========================

static bool copyFile(const char *src, const char *dst) {
    int fdin = open(src, O_RDONLY);
    if (fdin < 0) {
        LOGE("copyFile: cannot open src: %s (errno=%d)", src, errno);
        return false;
    }

    int fdout = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fdout < 0) {
        close(fdin);
        LOGE("copyFile: cannot open dst: %s (errno=%d)", dst, errno);
        return false;
    }

    char buf[8192];
    ssize_t n;
    ssize_t total = 0;
    while ((n = read(fdin, buf, sizeof(buf))) > 0) {
        ssize_t written = write(fdout, buf, n);
        if (written < 0) {
            LOGE("copyFile: write error (errno=%d)", errno);
            close(fdin);
            close(fdout);
            return false;
        }
        total += written;
    }
    close(fdin);
    close(fdout);
    LOGI("copyFile: %s -> %s (%zd bytes)", src, dst, total);
    return total > 0;
}

static void mkdirRecursive(const char *path) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
}

// ======================== Copy files with root access ========================
// Called from preAppSpecialize where we still have zygote/root privileges

static bool copyModFiles(int moduleDirFd) {
    std::string appDataDir = "/data/data/" + std::string(TARGET_PACKAGE);
    g_modCacheDir = appDataDir + "/mod_cache";
    g_localDex = g_modCacheDir + "/classes.dex";
    g_localSoDir = g_modCacheDir + "/lib";

    // Create directories with root access
    mkdirRecursive(g_modCacheDir.c_str());
    mkdirRecursive(g_localSoDir.c_str());

    // Set permissions so the app can read them after sandboxing
    chmod(g_modCacheDir.c_str(), 0777);
    chmod(g_localSoDir.c_str(), 0777);

    // Source paths in module directory
    std::string modulePath = "/data/adb/modules/pu_mod";
    std::string srcDex = modulePath + "/mod/classes.dex";
    std::string dstSo = g_localSoDir + "/libgiyuutmk.so";

    LOGI("ABI: %s", CURRENT_ABI);
    LOGI("Module path: %s", modulePath.c_str());

    // IMPORTANT: The game's native libs (libil2cpp.so) are ARM64, running via Houdini
    // translation on x86_64 emulators. Our mod lib MUST be ARM64 to hook the correct
    // offsets. Houdini will translate it automatically when loaded via System.loadLibrary.
    // Priority order: arm64 > arm > current ABI > any available
    const char *abiPriority[] = {"arm64-v8a", "armeabi-v7a", CURRENT_ABI, "x86_64", "x86", nullptr};
    std::string srcSo;
    for (int i = 0; abiPriority[i]; i++) {
        std::string tryPath = modulePath + "/mod/lib/" + abiPriority[i] + "/libgiyuutmk.so";
        if (access(tryPath.c_str(), R_OK) == 0) {
            srcSo = tryPath;
            LOGI("Selected mod lib ABI: %s", abiPriority[i]);
            break;
        }
    }

    LOGI("Src DEX: %s (exists=%d)", srcDex.c_str(), access(srcDex.c_str(), R_OK) == 0);
    LOGI("Src SO: %s (exists=%d)", srcSo.c_str(), !srcSo.empty() && access(srcSo.c_str(), R_OK) == 0);

    // Copy DEX
    bool dexOk = false;
    if (access(srcDex.c_str(), R_OK) == 0) {
        dexOk = copyFile(srcDex.c_str(), g_localDex.c_str());
        chmod(g_localDex.c_str(), 0666);
    } else {
        LOGE("DEX not found: %s", srcDex.c_str());
    }

    // Copy SO
    bool soOk = false;
    if (!srcSo.empty() && access(srcSo.c_str(), R_OK) == 0) {
        soOk = copyFile(srcSo.c_str(), dstSo.c_str());
        chmod(dstSo.c_str(), 0777);
    } else {
        LOGE("SO not found for any ABI");
    }

    // Make sure app owns the files
    // Get UID from package (app UIDs start at 10000)
    struct stat st;
    if (stat(appDataDir.c_str(), &st) == 0) {
        chown(g_modCacheDir.c_str(), st.st_uid, st.st_gid);
        chown(g_localSoDir.c_str(), st.st_uid, st.st_gid);
        chown(g_localDex.c_str(), st.st_uid, st.st_gid);
        chown(dstSo.c_str(), st.st_uid, st.st_gid);
        LOGI("Set ownership to uid=%d gid=%d", st.st_uid, st.st_gid);
    }

    LOGI("File copy result: dex=%d so=%d", dexOk, soOk);
    return dexOk && soOk;
}

// ======================== Inject mod menu (runs in app process) ========================

// Get the current running Activity (not just Application) via ActivityThread reflection
static jobject getCurrentActivity(JNIEnv *env) {
    // ActivityThread.currentActivityThread().mActivities -> get first Activity
    jclass actThreadClass = env->FindClass("android/app/ActivityThread");
    if (!actThreadClass) return nullptr;

    jmethodID currentAT = env->GetStaticMethodID(actThreadClass, "currentActivityThread",
                                                   "()Landroid/app/ActivityThread;");
    if (!currentAT) return nullptr;

    jobject actThread = env->CallStaticObjectMethod(actThreadClass, currentAT);
    if (!actThread) return nullptr;

    // Get mActivities field (ArrayMap or HashMap of Activity records)
    jfieldID mActivitiesField = env->GetFieldID(actThreadClass, "mActivities",
                                                 "Landroid/util/ArrayMap;");
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
    if (!mActivitiesField) return nullptr;

    jobject mActivities = env->GetObjectField(actThread, mActivitiesField);
    if (!mActivities) return nullptr;

    // Get values collection
    jclass mapClass = env->GetObjectClass(mActivities);
    jmethodID valuesMethod = env->GetMethodID(mapClass, "values", "()Ljava/util/Collection;");
    if (!valuesMethod) return nullptr;

    jobject values = env->CallObjectMethod(mActivities, valuesMethod);
    if (!values) return nullptr;

    // Convert to array
    jclass collClass = env->GetObjectClass(values);
    jmethodID toArray = env->GetMethodID(collClass, "toArray", "()[Ljava/lang/Object;");
    if (!toArray) return nullptr;

    jobjectArray arr = (jobjectArray) env->CallObjectMethod(values, toArray);
    if (!arr || env->GetArrayLength(arr) == 0) return nullptr;

    // Get first ActivityClientRecord and extract its activity field
    jobject record = env->GetObjectArrayElement(arr, 0);
    if (!record) return nullptr;

    jclass recordClass = env->GetObjectClass(record);
    jfieldID activityField = env->GetFieldID(recordClass, "activity", "Landroid/app/Activity;");
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
    if (!activityField) return nullptr;

    jobject activity = env->GetObjectField(record, activityField);
    return activity;
}

static void injectModMenu(JNIEnv *env) {
    LOGI("=== Starting mod injection ===");

    // Verify files exist
    if (access(g_localDex.c_str(), R_OK) != 0) {
        LOGE("Local DEX not readable: %s", g_localDex.c_str());
        return;
    }
    if (access((g_localSoDir + "/libgiyuutmk.so").c_str(), R_OK) != 0) {
        LOGE("Local SO not readable: %s/libgiyuutmk.so", g_localSoDir.c_str());
        return;
    }

    // Wait for an Activity to be available (not just Application)
    jobject activity = nullptr;
    for (int retry = 0; retry < 60; retry++) {
        activity = getCurrentActivity(env);
        if (activity) break;
        if (retry % 10 == 0) {
            LOGW("Waiting for Activity... retry %d/60", retry + 1);
        }
        usleep(500000); // 0.5s
    }
    if (!activity) {
        LOGE("No Activity found after 30s, giving up");
        return;
    }
    LOGI("Got Activity context");

    // Get context's classloader
    jclass contextClass = env->FindClass("android/content/Context");
    jmethodID getClassLoader = env->GetMethodID(contextClass, "getClassLoader",
                                                 "()Ljava/lang/ClassLoader;");
    jobject parentLoader = env->CallObjectMethod(activity, getClassLoader);
    if (!parentLoader) {
        LOGE("getClassLoader returned null");
        return;
    }

    // Create DexClassLoader with lib path so System.loadLibrary finds our .so
    jstring dexPathStr = env->NewStringUTF(g_localDex.c_str());
    jstring optDirStr  = env->NewStringUTF(g_modCacheDir.c_str());
    jstring libDirStr  = env->NewStringUTF(g_localSoDir.c_str());

    jclass dexLoaderClass = env->FindClass("dalvik/system/DexClassLoader");
    jmethodID dexLoaderInit = env->GetMethodID(dexLoaderClass, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");

    jobject dexLoader = env->NewObject(dexLoaderClass, dexLoaderInit,
                                        dexPathStr, optDirStr, libDirStr, parentLoader);
    if (env->ExceptionCheck()) {
        LOGE("DexClassLoader exception:");
        env->ExceptionDescribe();
        env->ExceptionClear();
        return;
    }
    if (!dexLoader) {
        LOGE("DexClassLoader creation returned null");
        return;
    }
    LOGI("DexClassLoader created OK");

    // Load Main class from the mod DEX
    jmethodID loadClass = env->GetMethodID(dexLoaderClass, "loadClass",
                                            "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring mainClassName = env->NewStringUTF("com.android.support.Main");
    jclass mainClass = (jclass) env->CallObjectMethod(dexLoader, loadClass, mainClassName);
    if (env->ExceptionCheck()) {
        LOGE("loadClass Main exception:");
        env->ExceptionDescribe();
        env->ExceptionClear();
        return;
    }
    if (!mainClass) {
        LOGE("Main class not found in DEX");
        return;
    }
    LOGI("Loaded com.android.support.Main");

    // Load ModLoader helper class which handles UI thread marshaling
    jstring loaderClassName = env->NewStringUTF("com.android.support.ModLoader");
    jclass modLoaderClass = (jclass) env->CallObjectMethod(dexLoader, loadClass, loaderClassName);
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }

    if (!modLoaderClass) {
        LOGE("ModLoader class not found in DEX");
        return;
    }

    // Call ModLoader.inject(activity) - this posts to UI thread internally
    jmethodID injectMethod = env->GetStaticMethodID(modLoaderClass, "inject",
                                                     "(Landroid/content/Context;)V");
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    if (!injectMethod) {
        LOGE("ModLoader.inject method not found");
        return;
    }

    LOGI("Calling ModLoader.inject(activity)...");
    env->CallStaticVoidMethod(modLoaderClass, injectMethod, activity);
    if (env->ExceptionCheck()) {
        LOGE("ModLoader.inject exception:");
        env->ExceptionDescribe();
        env->ExceptionClear();
    } else {
        LOGI("=== Mod menu injection scheduled on UI thread! ===");
    }
}

// Background thread: waits for app to init, then injects
static void delayedInject(JavaVM *jvm) {
    // Give the app time to fully initialize
    sleep(3);

    JNIEnv *env = nullptr;
    bool attached = false;
    int status = jvm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        if (jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            LOGE("AttachCurrentThread failed");
            return;
        }
        attached = true;
    }

    if (env) {
        injectModMenu(env);
    } else {
        LOGE("Failed to get JNIEnv");
    }

    if (attached) {
        jvm->DetachCurrentThread();
    }
}

// ======================== Zygisk Module ========================

class PUMod : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
        LOGI("onLoad: Zygisk module loaded (ABI=%s)", CURRENT_ABI);
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        const char *niceName = env->GetStringUTFChars(args->nice_name, nullptr);
        if (niceName) {
            isTarget = (strcmp(niceName, TARGET_PACKAGE) == 0);
            if (isTarget) {
                LOGI("preAppSpecialize: TARGET MATCH -> %s", niceName);
            }
            env->ReleaseStringUTFChars(args->nice_name, niceName);
        }

        if (!isTarget) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        // We have root access here - copy mod files to app's data dir
        int modDir = api->getModuleDir();
        g_filesCopied = copyModFiles(modDir);
        if (!g_filesCopied) {
            LOGE("preAppSpecialize: file copy FAILED");
        } else {
            LOGI("preAppSpecialize: files copied OK");
        }
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (!isTarget) return;

        if (!g_filesCopied) {
            LOGE("postAppSpecialize: skipping injection (files not copied)");
            return;
        }

        LOGI("postAppSpecialize: launching injection thread");

        JavaVM *jvm = nullptr;
        env->GetJavaVM(&jvm);
        if (jvm) {
            std::thread(delayedInject, jvm).detach();
        } else {
            LOGE("GetJavaVM failed");
        }
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    bool isTarget = false;
};

REGISTER_ZYGISK_MODULE(PUMod)

// Companion handler (can be used for root IPC if needed)
static void companion_handler(int fd) {
    LOGI("Companion handler called (fd=%d)", fd);
}

REGISTER_ZYGISK_COMPANION(companion_handler)
