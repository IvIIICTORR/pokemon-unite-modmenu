#include <list>
#include <vector>
#include <cstring>
#include <pthread.h>
#include <thread>
#include <cstring>
#include <string>
#include <jni.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <dlfcn.h>
#include "Includes/Logger.h"
#include "Includes/obfuscate.h"
#include "Includes/Utils.hpp"
#include "Menu/Menu.hpp"
#include "Menu/Jni.hpp"
#include "Includes/Macros.h"

// ======================== Map Hack ========================
// The 3 confirmed working patches (must be active BEFORE match starts):
//   1. MVisionSysUtil.IsVisible(Actor, Actor, Boolean) @ 0x384b874
//   2. LVisionSys.IsCampVisible(LVisionSysComp, Int32, Entity) @ 0x351003c
//   3. LVisionSys.CheckVisible(LVisionSysComp, Entity, Entity, out Boolean&) @ 0x3510154
//
// KEY: patches are auto-enabled on creation so they're active before the match loads.
// Toggling off mid-match may cause issues since the game already cached visibility.

struct MemPatches {
    MemoryPatch isVisible;      // MVisionSysUtil.IsVisible @ 0x384b874
    MemoryPatch isCampVisible;  // LVisionSys.IsCampVisible @ 0x351003c
    MemoryPatch checkVisible;   // LVisionSys.CheckVisible @ 0x3510154
} gPatches;

jobjectArray GetFeatureList(JNIEnv *env, jobject context) {
    jobjectArray ret;

    const char *features[] = {
            OBFUSCATE("Category_Map Hack (ativar ANTES da partida!)"),
            OBFUSCATE("Toggle_Map Hack"),
    };

    int Total_Feature = (sizeof features / sizeof features[0]);
    ret = (jobjectArray)
            env->NewObjectArray(Total_Feature, env->FindClass(OBFUSCATE("java/lang/String")),
                                env->NewStringUTF(""));

    for (int i = 0; i < Total_Feature; i++)
        env->SetObjectArrayElement(ret, i, env->NewStringUTF(features[i]));

    return (ret);
}

void Changes(JNIEnv *env, jclass clazz, jobject obj, jint featNum, jstring featName, jint value, jlong Lvalue, jboolean boolean, jstring text) {

    switch (featNum) {
        case 0:
            if (boolean) {
                gPatches.isVisible.Modify();
                gPatches.isCampVisible.Modify();
                gPatches.checkVisible.Modify();
                LOGI(OBFUSCATE("Map Hack ON (all 3 patches)"));
            } else {
                gPatches.isVisible.Restore();
                gPatches.isCampVisible.Restore();
                gPatches.checkVisible.Restore();
                LOGI(OBFUSCATE("Map Hack OFF (all 3 restored)"));
            }
            break;
    }
}

//Target lib here
#define targetLibName OBFUSCATE("libil2cpp.so")

ElfScanner g_il2cppELF;

// we will run our hacks in a new thread so our while loop doesn't block process main thread
void *hack_thread(void *) {
    LOGI(OBFUSCATE("pthread created - waiting 30s for game to fully load..."));

    // Wait 30 seconds for the game to fully load, menu to appear, etc.
    sleep(30);

    LOGI(OBFUSCATE("30s passed, looking for libil2cpp..."));

    do {
        sleep(1);
        g_il2cppELF = ElfScanner::createWithPath(targetLibName);
    } while (!g_il2cppELF.isValid());

    LOGI(OBFUSCATE("%s ready, base=%p"), (const char *) targetLibName, (void*)g_il2cppELF.base());

#if defined(__aarch64__)
    uintptr_t il2cppBase = g_il2cppELF.base();

    // Patch 1: MVisionSysUtil.IsVisible(Actor, Actor, Boolean) @ 0x384b874
    // MOV W0, #1; RET
    gPatches.isVisible = MemoryPatch::createWithHex(
        il2cppBase + str2Offset(OBFUSCATE("0x384b874")),
        OBFUSCATE("20 00 80 52 C0 03 5F D6"));
    LOGI(OBFUSCATE("IsVisible @ 0x384b874: valid=%d"), gPatches.isVisible.isValid());

    // Patch 2: LVisionSys.IsCampVisible @ 0x351003c
    // MOV W0, #1; RET
    gPatches.isCampVisible = MemoryPatch::createWithHex(
        il2cppBase + str2Offset(OBFUSCATE("0x351003c")),
        OBFUSCATE("20 00 80 52 C0 03 5F D6"));
    LOGI(OBFUSCATE("IsCampVisible @ 0x351003c: valid=%d"), gPatches.isCampVisible.isValid());

    // Patch 3: LVisionSys.CheckVisible @ 0x3510154
    // MOV W0, #1; RET
    gPatches.checkVisible = MemoryPatch::createWithHex(
        il2cppBase + str2Offset(OBFUSCATE("0x3510154")),
        OBFUSCATE("20 00 80 52 C0 03 5F D6"));
    LOGI(OBFUSCATE("CheckVisible @ 0x3510154: valid=%d"), gPatches.checkVisible.isValid());

    // NOT applied - user must enable BEFORE entering a match
    LOGI(OBFUSCATE("3 Map Hack patches ready (enable BEFORE match!)"));

#elif defined(__arm__)
    LOGI(OBFUSCATE("ARMv7 not supported"));
#endif

    LOGI(OBFUSCATE("Done"));
    return nullptr;
}

__attribute__((constructor))
void lib_main() {
    // Create a new thread so it does not block the main thread, means the game would not freeze
    pthread_t ptid;
    pthread_create(&ptid, NULL, hack_thread, NULL);
}