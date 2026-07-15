"""
Build Zygisk Module for Honor of Kings Mod Menu
================================================
This script:
  1. Compiles the Zygisk loader .so using NDK (ndk-build)
  2. Compiles the HoK mod menu .so using NDK (ndk-build)
  3. Compiles the mod's Java classes into classes.dex
  4. Packages everything into a flashable .zip for Kitsune Mask

Requirements:
  - Android SDK with NDK installed
  - Java (JDK) in PATH for d8/dx (DEX compilation)
  - modmenu_honorofkings project alongside this folder

Usage:
  python build_module.py
"""

import os
import sys
import shutil
import subprocess
import zipfile
import glob

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TRANSFER_DIR = os.path.dirname(SCRIPT_DIR)
LGLTEAM_DIR = os.path.join(TRANSFER_DIR, "modmenu_original_lgl")
MOD_APK_DIR = os.path.join(LGLTEAM_DIR, "app", "src", "main")

# Auto-detect NDK
SDK_DIR = os.path.expandvars(r"%LOCALAPPDATA%\Android\Sdk")
NDK_DIR = None
ndk_base = os.path.join(SDK_DIR, "ndk")
if os.path.isdir(ndk_base):
    versions = sorted(os.listdir(ndk_base), reverse=True)
    if versions:
        NDK_DIR = os.path.join(ndk_base, versions[0])

if not NDK_DIR or not os.path.isdir(NDK_DIR):
    print(f"ERROR: NDK not found in {ndk_base}")
    print("Please set NDK_DIR manually or install NDK via SDK Manager")
    sys.exit(1)

NDK_BUILD = os.path.join(NDK_DIR, "ndk-build.cmd")
if not os.path.exists(NDK_BUILD):
    NDK_BUILD = os.path.join(NDK_DIR, "ndk-build")

# Auto-detect javac
JAVAC = None
javac_candidates = [
    os.path.join(os.environ.get("JAVA_HOME", ""), "bin", "javac.exe"),
    r"C:\Program Files\Android\Android Studio\jbr\bin\javac.exe",
    r"C:\Program Files\Android\Android Studio\jre\bin\javac.exe",
]
for jc in javac_candidates:
    if os.path.exists(jc):
        JAVAC = jc
        break
if not JAVAC:
    # Try PATH
    try:
        result = subprocess.run(["where", "javac"], capture_output=True, text=True)
        if result.returncode == 0 and result.stdout.strip():
            JAVAC = result.stdout.strip().split('\n')[0].strip()
    except:
        pass
if not JAVAC:
    print("ERROR: javac not found. Install JDK or Android Studio.")
    sys.exit(1)

print(f"Using NDK: {NDK_DIR}")
print(f"ndk-build: {NDK_BUILD}")
print(f"javac: {JAVAC}")

# Build tools for d8
BUILD_TOOLS_DIR = None
bt_base = os.path.join(SDK_DIR, "build-tools")
if os.path.isdir(bt_base):
    versions = sorted(os.listdir(bt_base), reverse=True)
    if versions:
        BUILD_TOOLS_DIR = os.path.join(bt_base, versions[0])

# Output directories
BUILD_DIR = os.path.join(SCRIPT_DIR, "build")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "output")
ZYGISK_LIBS_DIR = os.path.join(BUILD_DIR, "zygisk_libs")
MOD_LIBS_DIR = os.path.join(BUILD_DIR, "mod_libs")

# Clean and create build dirs
for d in [BUILD_DIR, OUTPUT_DIR, ZYGISK_LIBS_DIR, MOD_LIBS_DIR]:
    os.makedirs(d, exist_ok=True)


def run(cmd, cwd=None, check=True):
    print(f"  >> {' '.join(cmd) if isinstance(cmd, list) else cmd}")
    result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, shell=True)
    if result.stdout.strip():
        print(result.stdout.strip())
    if result.stderr.strip():
        print(result.stderr.strip())
    if check and result.returncode != 0:
        print(f"ERROR: Command failed with code {result.returncode}")
        sys.exit(1)
    return result


def step1_compile_zygisk_loader():
    """Compile the Zygisk loader .so"""
    print("\n=== Step 1: Compiling Zygisk loader ===")
    jni_dir = os.path.join(SCRIPT_DIR, "jni")

    # ndk-build with output to build dir
    run(f'"{NDK_BUILD}" NDK_PROJECT_PATH="{SCRIPT_DIR}" '
        f'NDK_LIBS_OUT="{ZYGISK_LIBS_DIR}" '
        f'NDK_OUT="{os.path.join(BUILD_DIR, "zygisk_obj")}" '
        f'APP_BUILD_SCRIPT="{os.path.join(jni_dir, "Android.mk")}" '
        f'NDK_APPLICATION_MK="{os.path.join(jni_dir, "Application.mk")}" '
        f'-j4',
        cwd=SCRIPT_DIR)

    # Verify output
    for abi in ["arm64-v8a", "armeabi-v7a", "x86", "x86_64"]:
        so = os.path.join(ZYGISK_LIBS_DIR, abi, "libzygisk_loader.so")
        if os.path.exists(so):
            size = os.path.getsize(so)
            print(f"  OK: {abi}/libzygisk_loader.so ({size} bytes)")
        else:
            print(f"  WARN: {abi}/libzygisk_loader.so not found")


def step2_compile_mod_lib():
    """Compile the mod menu native lib (libgiyuutmk.so) from lglteam4.0"""
    print("\n=== Step 2: Compiling mod menu native lib ===")
    jni_dir = os.path.join(MOD_APK_DIR, "jni")

    if not os.path.isdir(jni_dir):
        print(f"ERROR: lglteam4.0 JNI dir not found: {jni_dir}")
        sys.exit(1)

    # Override APP_ABI to include x86/x86_64 for emulator support
    run(f'"{NDK_BUILD}" NDK_PROJECT_PATH="{os.path.join(LGLTEAM_DIR, "app")}" '
        f'NDK_LIBS_OUT="{MOD_LIBS_DIR}" '
        f'NDK_OUT="{os.path.join(BUILD_DIR, "mod_obj")}" '
        f'APP_BUILD_SCRIPT="{os.path.join(jni_dir, "Android.mk")}" '
        f'NDK_APPLICATION_MK="{os.path.join(jni_dir, "Application.mk")}" '
        f'APP_ABI="arm64-v8a" '
        f'-j4',
        cwd=os.path.join(LGLTEAM_DIR, "app"))

    for abi in ["arm64-v8a", "armeabi-v7a", "x86", "x86_64"]:
        so = os.path.join(MOD_LIBS_DIR, abi, "libgiyuutmk.so")
        if os.path.exists(so):
            size = os.path.getsize(so)
            print(f"  OK: {abi}/libgiyuutmk.so ({size} bytes)")
        else:
            print(f"  WARN: {abi}/libgiyuutmk.so not found")


def step3_compile_dex():
    """Compile the mod's Java classes into classes.dex"""
    print("\n=== Step 3: Compiling mod Java classes to DEX ===")
    java_dir = os.path.join(MOD_APK_DIR, "java")
    classes_dir = os.path.join(BUILD_DIR, "classes")
    os.makedirs(classes_dir, exist_ok=True)

    # Find all .java files
    java_files = []
    for root, dirs, files in os.walk(java_dir):
        for f in files:
            if f.endswith(".java"):
                java_files.append(os.path.join(root, f))

    if not java_files:
        print("ERROR: No .java files found")
        sys.exit(1)

    print(f"  Found {len(java_files)} Java files")

    # Find android.jar for compilation
    platforms_dir = os.path.join(SDK_DIR, "platforms")
    android_jar = None
    if os.path.isdir(platforms_dir):
        plats = sorted(os.listdir(platforms_dir), reverse=True)
        for p in plats:
            jar = os.path.join(platforms_dir, p, "android.jar")
            if os.path.exists(jar):
                android_jar = jar
                break

    if not android_jar:
        print("ERROR: android.jar not found in SDK platforms")
        sys.exit(1)

    print(f"  Using android.jar: {android_jar}")

    # Compile Java -> .class
    javac_cmd = [JAVAC, '-source', '8', '-target', '8',
                 '-cp', android_jar, '-d', classes_dir] + java_files
    print(f"  >> javac ({len(java_files)} files)")
    result = subprocess.run(javac_cmd, capture_output=True, text=True, cwd=java_dir)
    if result.stdout.strip(): print(result.stdout.strip())
    if result.stderr.strip(): print(result.stderr.strip())
    if result.returncode != 0:
        print(f"ERROR: javac failed with code {result.returncode}")
        sys.exit(1)

    # Convert .class -> .dex using d8
    class_files = []
    for root, dirs, files in os.walk(classes_dir):
        for f in files:
            if f.endswith(".class"):
                class_files.append(os.path.join(root, f))

    dex_output = os.path.join(BUILD_DIR, "dex_output")
    os.makedirs(dex_output, exist_ok=True)

    # Find d8.jar and use JBR's java to run it (system java may be too old)
    JAVA_EXE = os.path.dirname(JAVAC) + os.sep + "java.exe"
    if not os.path.exists(JAVA_EXE):
        JAVA_EXE = os.path.dirname(JAVAC) + os.sep + "java"
    if not os.path.exists(JAVA_EXE):
        JAVA_EXE = "java"  # fallback to system

    d8_jar = None
    if BUILD_TOOLS_DIR:
        d8_jar = os.path.join(BUILD_TOOLS_DIR, "lib", "d8.jar")
        if not os.path.exists(d8_jar):
            d8_jar = None

    if not d8_jar:
        print("ERROR: d8.jar not found in build-tools")
        sys.exit(1)

    print(f"  Using java: {JAVA_EXE}")
    print(f"  Using d8.jar: {d8_jar}")

    d8_cmd = [JAVA_EXE, '-cp', d8_jar, 'com.android.tools.r8.D8',
              '--output', dex_output, '--lib', android_jar] + class_files
    print(f"  >> d8 ({len(class_files)} class files)")
    result = subprocess.run(d8_cmd, capture_output=True, text=True)
    if result.stdout.strip(): print(result.stdout.strip())
    if result.stderr.strip(): print(result.stderr.strip())
    if result.returncode != 0:
        print(f"ERROR: d8 failed with code {result.returncode}")
        sys.exit(1)

    dex_path = os.path.join(dex_output, "classes.dex")
    if os.path.exists(dex_path):
        print(f"  OK: classes.dex ({os.path.getsize(dex_path)} bytes)")
    else:
        print("ERROR: classes.dex not generated")
        sys.exit(1)

    return dex_path


def to_lf(text):
    """Convert text to Unix LF line endings"""
    return text.replace('\r\n', '\n').replace('\r', '\n')


def read_text_lf(path):
    """Read a text file and return contents with LF line endings"""
    with open(path, 'r', encoding='utf-8') as f:
        return to_lf(f.read())


def step4_package_module(dex_path):
    """Package everything into a flashable ZIP for Magisk/Kitsune Mask"""
    print("\n=== Step 4: Packaging Kitsune Mask module ===")
    zip_path = os.path.join(OUTPUT_DIR, "PUMod.zip")

    # The Magisk module installer (update-binary) - standard Magisk boot script
    # This is required for Magisk/Kitsune to recognize the ZIP as a module
    UPDATE_BINARY = to_lf(
        '#!/sbin/sh\n'
        '\n'
        '#################\n'
        '# Initialization\n'
        '#################\n'
        '\n'
        'umask 022\n'
        '\n'
        '# echo before loading util_functions\n'
        'ui_print() { echo "$1"; }\n'
        '\n'
        'require_new_magisk() {\n'
        '  ui_print "*******************************"\n'
        '  ui_print " Please install Magisk v20.4+! "\n'
        '  ui_print "*******************************"\n'
        '  exit 1\n'
        '}\n'
        '\n'
        '#########################\n'
        '# Load util_functions.sh\n'
        '#########################\n'
        '\n'
        'OUTFD=$2\n'
        'ZIPFILE=$3\n'
        '\n'
        'mount /data 2>/dev/null\n'
        '\n'
        '[ -f /data/adb/magisk/util_functions.sh ] || require_new_magisk\n'
        '. /data/adb/magisk/util_functions.sh\n'
        '[ $MAGISK_VER_CODE -lt 20400 ] && require_new_magisk\n'
        '\n'
        'install_module\n'
        'exit 0\n'
    )

    UPDATER_SCRIPT = '#MAGISK\n'

    with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zf:
        # META-INF (required for Magisk/Kitsune to process the ZIP)
        zf.writestr("META-INF/com/google/android/update-binary", UPDATE_BINARY)
        print("  + META-INF/com/google/android/update-binary")
        zf.writestr("META-INF/com/google/android/updater-script", UPDATER_SCRIPT)
        print("  + META-INF/com/google/android/updater-script")

        # module.prop (text - ensure LF)
        zf.writestr("module.prop", read_text_lf(os.path.join(SCRIPT_DIR, "module.prop")))
        print("  + module.prop")

        # customize.sh (text - ensure LF)
        zf.writestr("customize.sh", read_text_lf(os.path.join(SCRIPT_DIR, "customize.sh")))
        print("  + customize.sh")

        # Zygisk loader .so files (named as <abi>.so in zygisk/ folder)
        # Include x86/x86_64 for emulators (LDPlayer, BlueStacks, MuMu, etc.)
        for abi in ["arm64-v8a", "armeabi-v7a", "x86", "x86_64"]:
            src = os.path.join(ZYGISK_LIBS_DIR, abi, "libzygisk_loader.so")
            if os.path.exists(src):
                zf.write(src, f"zygisk/{abi}.so")
                print(f"  + zygisk/{abi}.so")

        # Mod native libs (including x86/x86_64 for emulators)
        for abi in ["arm64-v8a", "armeabi-v7a", "x86", "x86_64"]:
            src = os.path.join(MOD_LIBS_DIR, abi, "libgiyuutmk.so")
            if os.path.exists(src):
                zf.write(src, f"mod/lib/{abi}/libgiyuutmk.so")
                print(f"  + mod/lib/{abi}/libgiyuutmk.so")

        # Mod DEX
        zf.write(dex_path, "mod/classes.dex")
        print("  + mod/classes.dex")

        # Module icon (if exists)
        icon_path = os.path.join(TRANSFER_DIR, "ico", "modmenu.jpg")
        if os.path.exists(icon_path):
            zf.write(icon_path, "common/modmenu.jpg")
            print("  + common/modmenu.jpg (icon)")

    size_mb = os.path.getsize(zip_path) / (1024 * 1024)
    print(f"\n  MODULE ZIP: {zip_path}")
    print(f"  Size: {size_mb:.1f} MB")
    return zip_path


def main():
    print("=" * 60)
    print("Honor of Kings Mod - Zygisk Module Builder")
    print("=" * 60)

    step1_compile_zygisk_loader()
    step2_compile_mod_lib()
    dex_path = step3_compile_dex()
    zip_path = step4_package_module(dex_path)

    print("\n" + "=" * 60)
    print("BUILD COMPLETE!")
    print("=" * 60)
    print(f"\nFlashable ZIP: {zip_path}")
    print("\nInstall instructions:")
    print("  1. Copy the ZIP to your emulator/device")
    print("  2. Open Kitsune Mask")
    print("  3. Go to Modules -> Install from storage")
    print("  4. Select the ZIP file")
    print("  5. Reboot")
    print("  6. Open Pokemon Unite normally")
    print("  7. The mod menu should appear automatically!")


if __name__ == "__main__":
    main()
