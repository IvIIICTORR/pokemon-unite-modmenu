#export THEOS=/var/mobile/theos
ARCHS = arm64
#Add arm64e if it needed
DEBUG = 0
FINALPACKAGE = 1
FOR_RELEASE = 1
# Sideload (sem jailbreak): NAO usar rootless.
#THEOS_PACKAGE_SCHEME = rootless

include $(THEOS)/makefiles/common.mk

TWEAK_NAME = pumenu
# O nome do filter plist deve bater com TWEAK_NAME (pumenu.plist).

$(TWEAK_NAME)_FRAMEWORKS = UIKit Foundation Security QuartzCore CoreGraphics CoreText AVFoundation Accelerate GLKit SystemConfiguration GameController Metal MetalKit

$(TWEAK_NAME)_CCFLAGS = -std=c++11 -fno-rtti -fno-exceptions -DNDEBUG
# Flags aplicadas a TODOS os compiladores (C/ObjC/C++/ObjC++).
# Suprime erros do toolchain novo do runner macOS (template feito com clang antigo):
#   -Wno-error                     : nao tratar warnings como erro
#   -fno-modules                   : evita "import of C++ module within extern C" (dobby.h)
#   -Wno-nontrivial-memcall        : imgui.h usa memset(this,...) em tipos nao-triviais
#   -Wno-module-import-in-extern-c : idem, seguranca extra
$(TWEAK_NAME)_CFLAGS = -fobjc-arc -fno-modules -Wno-error -Wno-nontrivial-memcall -Wno-module-import-in-extern-c -Wno-deprecated-declarations -Wno-unused-variable -Wno-unused-value

# Dobby (hook engine) - vem do template HuyJIT via CI
$(TWEAK_NAME)_OBJ_FILES = 5Toubun/libdobby.a

$(TWEAK_NAME)_FILES = ImGuiDrawView.mm $(wildcard Esp/*.mm) $(wildcard Esp/*.m) $(wildcard IMGUI/*.cpp) $(wildcard IMGUI/*.mm)

include $(THEOS_MAKE_PATH)/tweak.mk
