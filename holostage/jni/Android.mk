LOCAL_PATH := $(call my-dir)/../../holostage
CODE_PATH := $(LOCAL_PATH)/../code
# assimp

include $(CLEAR_VARS)

LOCAL_MODULE := assimp
LOCAL_SRC_FILES := $(CODE_PATH)/libs/assimp/$(TARGET_ARCH_ABI)/libassimp.a
include $(PREBUILT_STATIC_LIBRARY)

# vuforia
include $(CLEAR_VARS)
LOCAL_MODULE := Vuforia
LOCAL_SRC_FILES = $(CODE_PATH)/libs/vuforia/$(TARGET_ARCH_ABI)/libVuforia.so
LOCAL_EXPORT_C_INCLUDES := $(CODE_PATH)/external/vuforia
include $(PREBUILT_SHARED_LIBRARY)

# vuforia jni
include $(CLEAR_VARS)
LOCAL_MODULE := ImageTargetsNative

# Set OpenGL ES version-specific settings.
OPENGLES_LIB  := -lGLESv2
OPENGLES_DEF  := -DUSE_OPENGL_ES_2_0

PROJECT_FILES := $(wildcard $(CODE_PATH)/imagetargets/*.h)
PROJECT_FILES += $(wildcard $(CODE_PATH)/imagetargets/*.cpp)

LOCAL_C_INCLUDES := $(CODE_PATH)/external/vuforia

LOCAL_SRC_FILES := $(PROJECT_FILES)

LOCAL_STATIC_LIBRARIES += libVuforia

LOCAL_CFLAGS := -Wno-write-strings -Wno-psabi $(OPENGLES_DEF) -std=c++11

LOCAL_LDLIBS := -landroid -llog -lz $(OPENGLES_LIB)

LOCAL_DISABLE_FORMAT_STRING_CHECKS := true
LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true

include $(BUILD_SHARED_LIBRARY)

# vulkan example

#DATADIR := $(LOCAL_PATH)/../../data

include $(CLEAR_VARS)

LOCAL_MODULE := vulkanPipelines

PROJECT_FILES := $(wildcard $(CODE_PATH)/pipelines/*.cpp)
# PROJECT_FILES += $(wildcard $(CODE_PATH)/pipelines/*.hpp)
PROJECT_FILES += $(wildcard $(CODE_PATH)/base/*.cpp)
#PROJECT_FILES += $(wildcard $(CODE_PATH)/imagetargets/*.h)
#PROJECT_FILES += $(wildcard $(CODE_PATH)/imagetargets/*.cpp)

LOCAL_CPPFLAGS := -std=c++11
LOCAL_CPPFLAGS += -D__STDC_LIMIT_MACROS
LOCAL_CPPFLAGS += -DVK_NO_PROTOTYPES
LOCAL_CPPFLAGS += -DVK_USE_PLATFORM_ANDROID_KHR

LOCAL_C_INCLUDES := $(CODE_PATH)/external/
LOCAL_C_INCLUDES += $(CODE_PATH)/external/glm
LOCAL_C_INCLUDES += $(CODE_PATH)/external/gli
LOCAL_C_INCLUDES += $(CODE_PATH)/external/assimp
LOCAL_C_INCLUDES += $(CODE_PATH)/base/
#LOCAL_C_INCLUDES += $(CODE_PATH)/external/vuforia

LOCAL_SRC_FILES := $(PROJECT_FILES)

LOCAL_LDLIBS := -landroid -llog -lz

LOCAL_DISABLE_FORMAT_STRING_CHECKS := true
LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true

LOCAL_STATIC_LIBRARIES += android_native_app_glue
LOCAL_STATIC_LIBRARIES += cpufeatures
LOCAL_STATIC_LIBRARIES += libassimp
LOCAL_STATIC_LIBRARIES += libImageTargetsNative

include $(BUILD_SHARED_LIBRARY)

$(call import-module, android/native_app_glue)
$(call import-module, android/cpufeatures)
