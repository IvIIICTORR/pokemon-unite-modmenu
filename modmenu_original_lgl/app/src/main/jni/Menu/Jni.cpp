
#include "Jni.hpp"
#include <list>
#include <vector>
#include <cstring>
#include <string>
#include <pthread.h>
#include <thread>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <dlfcn.h>
#include "Includes/obfuscate.h"
#include "Includes/get_device_api_level_inlines.h"
#include "Menu/Jni.hpp"
#include "Includes/Logger.h"

//Jni stuff from MrDarkRX https://github.com/MrDarkRXx/DarkMod-Floating
void setDialog(jobject ctx, JNIEnv *env, const char *title, const char *msg){
    jclass Alert = env->FindClass(OBFUSCATE("android/app/AlertDialog$Builder"));
    jmethodID AlertCons = env->GetMethodID(Alert, OBFUSCATE("<init>"), OBFUSCATE("(Landroid/content/Context;)V"));

    jobject MainAlert = env->NewObject(Alert, AlertCons, ctx);

    jmethodID setTitle = env->GetMethodID(Alert, OBFUSCATE("setTitle"), OBFUSCATE("(Ljava/lang/CharSequence;)Landroid/app/AlertDialog$Builder;"));
    env->CallObjectMethod(MainAlert, setTitle, env->NewStringUTF(title));

    jmethodID setMsg = env->GetMethodID(Alert, OBFUSCATE("setMessage"), OBFUSCATE("(Ljava/lang/CharSequence;)Landroid/app/AlertDialog$Builder;"));
    env->CallObjectMethod(MainAlert, setMsg, env->NewStringUTF(msg));

    jmethodID setCa = env->GetMethodID(Alert, OBFUSCATE("setCancelable"), OBFUSCATE("(Z)Landroid/app/AlertDialog$Builder;"));
    env->CallObjectMethod(MainAlert, setCa, false);

    jmethodID setPB = env->GetMethodID(Alert, OBFUSCATE("setPositiveButton"), OBFUSCATE("(Ljava/lang/CharSequence;Landroid/content/DialogInterface$OnClickListener;)Landroid/app/AlertDialog$Builder;"));
    env->CallObjectMethod(MainAlert, setPB, env->NewStringUTF("Ok"), static_cast<jobject>(NULL));

    jmethodID create = env->GetMethodID(Alert, OBFUSCATE("create"), OBFUSCATE("()Landroid/app/AlertDialog;"));
    jobject creaetob = env->CallObjectMethod(MainAlert, create);

    jclass AlertN = env->FindClass(OBFUSCATE("android/app/AlertDialog"));

    jmethodID show = env->GetMethodID(AlertN, OBFUSCATE("show"), OBFUSCATE("()V"));
    env->CallVoidMethod(creaetob, show);
}

void Toast(JNIEnv *env, jobject thiz, const char *text, int length) {
    jstring jstr = env->NewStringUTF(text);
    jclass toast = env->FindClass(OBFUSCATE("android/widget/Toast"));
    jmethodID methodMakeText =env->GetStaticMethodID(toast,OBFUSCATE("makeText"),OBFUSCATE("(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;"));
    jobject toastobj = env->CallStaticObjectMethod(toast, methodMakeText,thiz, jstr, length);
    jmethodID methodShow = env->GetMethodID(toast, OBFUSCATE("show"), OBFUSCATE("()V"));
    env->CallVoidMethod(toastobj, methodShow);
}

void startService(JNIEnv *env, jobject ctx){
    jclass native_context = env->GetObjectClass(ctx);
    jclass intentClass = env->FindClass(OBFUSCATE("android/content/Intent"));
    jclass actionString = env->FindClass(OBFUSCATE("com/android/support/Launcher"));
    jmethodID newIntent = env->GetMethodID(intentClass, OBFUSCATE("<init>"), OBFUSCATE("(Landroid/content/Context;Ljava/lang/Class;)V"));
    jobject intent = env->NewObject(intentClass,newIntent,ctx,actionString);
    jmethodID startActivityMethodId = env->GetMethodID(native_context, OBFUSCATE("startService"), OBFUSCATE("(Landroid/content/Intent;)Landroid/content/ComponentName;"));
    env->CallObjectMethod(ctx, startActivityMethodId, intent);
}

void *exit_thread(void *) {
    sleep(5);
    exit(0);
}

void startActivityPermisson(JNIEnv *env, jobject ctx){
    jclass native_context = env->GetObjectClass(ctx);
    jmethodID startActivity = env->GetMethodID(native_context, OBFUSCATE("startActivity"),OBFUSCATE("(Landroid/content/Intent;)V"));

    jmethodID pack = env->GetMethodID(native_context, OBFUSCATE("getPackageName"),OBFUSCATE("()Ljava/lang/String;"));
    jstring packageName = static_cast<jstring>(env->CallObjectMethod(ctx, pack));

    const char *pkg = env->GetStringUTFChars(packageName, 0);

    std::stringstream strpkg;
    strpkg << OBFUSCATE("package:");
    strpkg << pkg;
    std::string pakg = strpkg.str();

    jclass Uri = env->FindClass(OBFUSCATE("android/net/Uri"));
    jmethodID Parce = env->GetStaticMethodID(Uri, OBFUSCATE("parse"), OBFUSCATE("(Ljava/lang/String;)Landroid/net/Uri;"));
    jobject UriMethod = env->CallStaticObjectMethod(Uri, Parce, env->NewStringUTF(pakg.c_str()));

    jclass intentclass = env->FindClass(OBFUSCATE("android/content/Intent"));
    jmethodID newIntent = env->GetMethodID(intentclass, OBFUSCATE("<init>"), OBFUSCATE("(Ljava/lang/String;Landroid/net/Uri;)V"));
    jobject intent = env->NewObject(intentclass,newIntent,env->NewStringUTF(OBFUSCATE("android.settings.action.MANAGE_OVERLAY_PERMISSION")), UriMethod);

    env->CallVoidMethod(ctx, startActivity, intent);
}


//Needed jclass parameter because this is a static java method
void CheckOverlayPermission(JNIEnv *env, jclass thiz, jobject ctx){
    //If overlay permission option is greyed out, make sure to add android.permission.SYSTEM_ALERT_WINDOW in manifest

    LOGI(OBFUSCATE("Check overlay permission"));

    int sdkVer = api_level();
    if (sdkVer >= 23){ //Android 6.0
        jclass Settings = env->FindClass(OBFUSCATE("android/provider/Settings"));
        jmethodID canDraw = env->GetStaticMethodID(Settings, OBFUSCATE("canDrawOverlays"), OBFUSCATE("(Landroid/content/Context;)Z"));

        if (!env->CallStaticBooleanMethod(Settings, canDraw, ctx)){
            // Try to auto-grant via root (we are running inside a Zygisk module)
            LOGI(OBFUSCATE("Overlay not granted, attempting root auto-grant..."));

            // Get package name
            jclass native_context = env->GetObjectClass(ctx);
            jmethodID getPkg = env->GetMethodID(native_context, OBFUSCATE("getPackageName"), OBFUSCATE("()Ljava/lang/String;"));
            jstring jPkg = static_cast<jstring>(env->CallObjectMethod(ctx, getPkg));
            const char *pkg = env->GetStringUTFChars(jPkg, 0);

            // Grant via appops (root)
            char cmd[256];
            snprintf(cmd, sizeof(cmd), "su -c 'cmd appops set %s SYSTEM_ALERT_WINDOW allow' 2>/dev/null", pkg);
            system(cmd);
            LOGI(OBFUSCATE("Ran: %s"), cmd);

            // Also try without su (in case we're already root)
            snprintf(cmd, sizeof(cmd), "cmd appops set %s SYSTEM_ALERT_WINDOW allow 2>/dev/null", pkg);
            system(cmd);

            env->ReleaseStringUTFChars(jPkg, pkg);

            // Wait a moment for the permission to take effect, then re-check
            usleep(500000); // 500ms

            // Re-check up to 5 times
            for (int i = 0; i < 5; i++) {
                if (env->CallStaticBooleanMethod(Settings, canDraw, ctx)) {
                    LOGI(OBFUSCATE("Overlay permission granted via root (attempt %d)"), i+1);
                    goto permission_ok;
                }
                usleep(500000); // 500ms
                LOGI(OBFUSCATE("Overlay still not granted, retrying... (%d/5)"), i+1);
            }

            // If still not granted, try the manual way but DON'T exit
            LOGI(OBFUSCATE("Root auto-grant failed, trying manual..."));
            Toast(env, ctx, OBFUSCATE("Please grant overlay permission and restart the game."), 1);
            startActivityPermisson(env, ctx);

            pthread_t ptid;
            pthread_create(&ptid, NULL, exit_thread, NULL);
            return;
        }
    }

permission_ok:
    LOGI(OBFUSCATE("Start service"));

    //StartMod Normal
    startService(env, ctx);
}