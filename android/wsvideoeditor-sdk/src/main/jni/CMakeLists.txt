cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_VERBOSE_MAKEFILE on)

# -ffunction-sections: https://blog.csdn.net/pengfei240/article/details/55228228 减小链接后的包体积
# -funwind-tables: 查看程序跑死后的堆栈信息
# -fstack-protector-strong: https://www.cnblogs.com/gm-201705/p/9863958.html Android 7.0 的内核安全更新中，引入了 stack-protector-strong。
# -no-canonical-prefixes: https://blog.csdn.net/a568478312/article/details/79195218 生成其他 gcc 组件的相对路径时不生成规范化的前缀
# -mfloat-abi=softfp: https://blog.csdn.net/soaringlee_fighting/article/details/72493224 指定fpu为neon
# -mfpu=neon: https://blog.csdn.net/qiek/article/details/50900890 ARM平台NEON指令的编译和优
# -Wa,--noexecstack:  http://blog.sina.com.cn/s/blog_4c451e0e010132o9.html openssl 的某处代码会产生 execstack ，这个也会被 SELinux 阻止
# -Wformat -Werror=format-security: 解决输出日志的各种问题
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DANDROID_NDK -DRENDER_SDK ${COMPILE_PARAM}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DANDROID_NDK -DRENDER_SDK -fvisibility=hidden ${COMPILE_PARAM}")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections,--icf=safe ${COMPILE_PARAM}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem /Users/whensunset/Library/Android/sdk/ndk-bundle/sysroot/usr/include/arm-linux-androideabi -ffunction-sections -funwind-tables -fstack-protector-strong -no-canonical-prefixes -mfloat-abi=softfp -mfpu=vfpv3-d16 -fno-integrated-as -mthumb -mfpu=neon -Wa,--noexecstack -Wformat -Werror=format-security")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isystem /Users/whensunset/Library/Android/sdk/ndk-bundle/sysroot/usr/include/arm-linux-androideabi -ffunction-sections -funwind-tables -fstack-protector-strong -no-canonical-prefixes -mfloat-abi=softfp -mfpu=vfpv3-d16 -fno-integrated-as -mthumb -mfpu=neon -Wa,--noexecstack -Wformat -Werror=format-security -std=c++11")
#set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--exclude-libs,libgcc.a -Wl,--exclude-libs,libatomic.a --sysroot /Users/whensunset/Library/Android/sdk/ndk-bundle/platforms/android-15/arch-arm -Wl,--build-id -Wl,--warn-shared-textrel -Wl,--fatal-warnings -Wl,--fix-cortex-a8 -Wl,--exclude-libs,libunwind.a -L/Users/whensunset/Library/Android/sdk/ndk-bundle/sources/cxx-stl/llvm-libc++/libs/armeabi-v7a -Wl,--no-undefined -Wl,-z,noexecstack -Qunused-arguments -Wl,-z,relro -Wl,-z,now -Wl,--gc-sections")
#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os -DNDEBUG -O3")

#set(CMAKE_C_FLAGS "-isystem /Users/whensunset/Library/Android/sdk/ndk-bundle/sysroot/usr/include/arm-linux-androideabi -ffunction-sections -funwind-tables -fstack-protector-strong -no-canonical-prefixes -mfloat-abi=softfp -mfpu=vfpv3-d16 -fno-integrated-as -mthumb -mfpu=neon -Wa,--noexecstack -Wformat -Werror=format-security")
#set(CMAKE_CXX_FLAGS "-isystem /Users/whensunset/Library/Android/sdk/ndk-bundle/sysroot/usr/include/arm-linux-androideabi -ffunction-sections -funwind-tables -fstack-protector-strong -no-canonical-prefixes -mfloat-abi=softfp -mfpu=vfpv3-d16 -fno-integrated-as -mthumb -mfpu=neon -Wa,--noexecstack -Wformat -Werror=format-security -std=c++11")
#set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--exclude-libs,libgcc.a -Wl,--exclude-libs,libatomic.a --sysroot /Users/whensunset/Library/Android/sdk/ndk-bundle/platforms/android-15/arch-arm -Wl,--build-id -Wl,--warn-shared-textrel -Wl,--fatal-warnings -Wl,--fix-cortex-a8 -Wl,--exclude-libs,libunwind.a -L/Users/whensunset/Library/Android/sdk/ndk-bundle/sources/cxx-stl/llvm-libc++/libs/armeabi-v7a -Wl,--no-undefined -Wl,-z,noexecstack -Qunused-arguments -Wl,-z,relro -Wl,-z,now -Wl,--gc-sections")
#set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG -O3")

############ wsvideoeditorsdk ############

set(source_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(root_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)
set(library_DIR ${root_DIR}/lib)
set(ffmpeg_lib_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../ffmpeg-cpp)
set(protobuf_lib_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../protobuf-cpp)
set(thirdparty_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../thirdparty)
set(SHARED_CPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../sharedcpp)

set(EDITORSDK2_JNI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/wsvideoeditorsdkjni)

set(CMAKE_CXX_STANDARD 11)

add_definitions("-DDYNAMIC_ES3")
set(OPENGL_LIB GLESv2)

aux_source_directory(${source_DIR} SOURCE_DIR_ROOT)
list(APPEND SOURCE_DIR_ROOT
        ${EDITORSDK2_JNI_DIR}/android_logger.cc
        ${EDITORSDK2_JNI_DIR}/jni_helper.cc)

list(APPEND SOURCE_DIR_ROOT
        ${SHARED_CPP_DIR}/wsvideoeditorsdk/base/av_utils.cc
        ${SHARED_CPP_DIR}/wsvideoeditorsdk/ws_editor_video_sdk_utils.cpp
        ${SHARED_CPP_DIR}/wsvideoeditorsdk/preview_timeline.cc
        ${SHARED_CPP_DIR}/wsvideoeditorsdk/video_decode/video_decode_service.cc
        ${SHARED_CPP_DIR}/wsvideoeditorsdk/video_decode/video_decode_context.cpp
        ${SHARED_CPP_DIR}/wsvideoeditorsdk/prebuilt_protobuf/ws_video_editor_sdk.pb.cc)

list(APPEND SOURCE_DIR_INCLUDE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/wsvideoeditorsdkjni
        ${ffmpeg_lib_DIR}/include/armeabi-v7a
        ${protobuf_lib_DIR}/include
        ${SHARED_CPP_DIR}
        ${SHARED_CPP_DIR}/wsvideoeditorsdk
        ${SHARED_CPP_DIR}/wsvideoeditorsdk/model
        ${SHARED_CPP_DIR}/wsvideoeditorsdk/base
        ${SHARED_CPP_DIR}/wsvideoeditorsdk/video_decode
        ${SHARED_CPP_DIR}/wsvideoeditorsdk/prebuilt_protobuf
        )

list(APPEND LINK_LIBRARIES
        wsvideoeditorsdk
        android
        log
        EGL
        OpenSLES
        ${OPENGL_LIB}
        jnigraphics
        z
        m
        lib_ffmpeg
        libprotobuf-lite
        )

add_library( # Sets the name of the library.
        wsvideoeditorsdk

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        ${SOURCE_DIR_ROOT}
        )

add_library(lib_ffmpeg SHARED IMPORTED)
set_target_properties(lib_ffmpeg PROPERTIES IMPORTED_LOCATION
        ${ffmpeg_lib_DIR}/lib/armeabi-v7a/libffmpeg.so)

add_library(libprotobuf-lite STATIC IMPORTED)
set_target_properties(libprotobuf-lite PROPERTIES IMPORTED_LOCATION
        ${protobuf_lib_DIR}/lib/libprotobuf-lite.a)

target_include_directories(wsvideoeditorsdk PRIVATE ${SOURCE_DIR_INCLUDE})
target_compile_options(wsvideoeditorsdk PUBLIC -D_LIBCPP_HAS_THREAD_SAFETY_ANNOTATIONS -Wthread-safety -Werror -Wall -Wno-documentation -Wno-shorten-64-to-32 -Wno-nullability-completeness)

target_link_libraries(${LINK_LIBRARIES})
