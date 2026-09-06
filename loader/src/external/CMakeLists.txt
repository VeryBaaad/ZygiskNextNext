cmake_minimum_required(VERSION 3.22.1)
project(external)

if (ANDROID_ABI STREQUAL "riscv64")
	set(ANDROID_PLATFORM "android-35")
endif ()

option(LSPLT_BUILD_SHARED "Build LSPlt as a shared library" OFF)
add_subdirectory(lsplt/lsplt/src/main/jni)

if (ANDROID_ABI STREQUAL "arm64-v8a" OR ANDROID_ABI STREQUAL "armeabi-v7a")
    enable_language(ASM)
    set(SHADOWHOOK_SRC ${CMAKE_CURRENT_SOURCE_DIR}/shadowhook/shadowhook/src/main/cpp)
    if (ANDROID_ABI STREQUAL "arm64-v8a")
        set(SHADOWHOOK_ARCH "arm64")
    else ()
        set(SHADOWHOOK_ARCH "arm")
    endif ()
    file(GLOB SHADOWHOOK_SOURCES
        ${SHADOWHOOK_SRC}/*.c
        ${SHADOWHOOK_SRC}/arch/${SHADOWHOOK_ARCH}/*.c
        ${SHADOWHOOK_SRC}/arch/${SHADOWHOOK_ARCH}/*.S
        ${SHADOWHOOK_SRC}/common/*.c
        ${SHADOWHOOK_SRC}/third_party/*/*.c
    )
    # The upstream sh_linker.c initializes the dlopen/dlclose linker monitor,
    # which is unreliable in this statically linked loader's injection context;
    # our overlay copy (verbatim upstream, with sh_linker_init() skipping that
    # monitor) is compiled in its place. DO NOT TOUCH THIS.
    list(REMOVE_ITEM SHADOWHOOK_SOURCES ${SHADOWHOOK_SRC}/sh_linker.c)
    list(APPEND SHADOWHOOK_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/znn_shadowhook/sh_linker.c)
    add_library(shadowhook STATIC ${SHADOWHOOK_SOURCES})
    target_include_directories(shadowhook PRIVATE
        ${SHADOWHOOK_SRC}
        ${SHADOWHOOK_SRC}/include
        ${SHADOWHOOK_SRC}/arch/${SHADOWHOOK_ARCH}
        ${SHADOWHOOK_SRC}/common
        ${SHADOWHOOK_SRC}/third_party/xdl
        ${SHADOWHOOK_SRC}/third_party/bsd
        ${SHADOWHOOK_SRC}/third_party/lss
    )
    target_compile_options(shadowhook PRIVATE
        -Wno-unused-parameter
        -Wno-unused-but-set-variable
        -Wno-unused-variable
        -Wno-unused-function
    )
endif ()

if (NOT ANDROID_ABI STREQUAL "riscv64")
    set(BYTEHOOK_SRC ${CMAKE_CURRENT_SOURCE_DIR}/bytehook/bytehook/src/main/cpp)
    file(GLOB BYTEHOOK_SOURCES ${BYTEHOOK_SRC}/*.c)
    add_library(bytehook STATIC ${BYTEHOOK_SOURCES})
    target_compile_definitions(bytehook PRIVATE
        bytesig_init=bytehook_bytesig_init
        bytesig_protect=bytehook_bytesig_protect
        bytesig_unprotect=bytehook_bytesig_unprotect
    )
    target_include_directories(bytehook PRIVATE
        ${BYTEHOOK_SRC}
        ${BYTEHOOK_SRC}/include
        ${BYTEHOOK_SRC}/third_party/bsd
        ${BYTEHOOK_SRC}/third_party/lss
    )
    if (ANDROID_ABI STREQUAL "arm64-v8a" OR ANDROID_ABI STREQUAL "armeabi-v7a")
        target_include_directories(bytehook PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/shadowhook/shadowhook/src/main/cpp/include)
        target_link_libraries(bytehook PRIVATE shadowhook)
    endif ()
    target_compile_options(bytehook PRIVATE
        -Wno-unused-parameter
        -Wno-unused-but-set-variable
        -Wno-unused-variable
        -Wno-unused-function
    )
endif ()

if (ANDROID_ABI STREQUAL "riscv64")
    add_subdirectory(rv64hook)
        target_compile_options(rv64hook-static PRIVATE
        -Wno-sign-compare
    )
else ()
    option(DOBBY_GENERATE_SHARED "Build Dobby as a shared library" OFF)
    option(BUILD_EXAMPLE "Build Dobby example" OFF)
    option(BUILD_TEST "Build Dobby test" OFF)
    add_subdirectory(dobby)
    target_compile_options(dobby PRIVATE
        -Wno-reorder-ctor
        -Wno-sometimes-uninitialized
        -Wno-delete-non-abstract-non-virtual-dtor
        -Wno-sign-compare
        -Wno-overloaded-virtual
        -Wno-missing-field-initializers
        -Wno-missing-braces
        -Wno-macro-redefined
        -Wno-format-truncation
        -Wno-shift-count-overflow
		-Wno-unused-parameter
		-Wno-unused-but-set-variable
		-Wno-unused-private-field
		-Wno-unused-variable
		-Wno-unused-function
    )
endif ()

add_subdirectory(lzma)
