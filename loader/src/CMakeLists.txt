cmake_minimum_required(VERSION 3.22.1)
project(loader)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external)

if (LSPLT_STANDALONE)
    find_package(cxx REQUIRED CONFIG)
endif ()

add_library(elfutil STATIC utils/elf_util.cpp)
target_include_directories(elfutil PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(elfutil PUBLIC lzma)
if (TARGET cxx::cxx)
    target_link_libraries(elfutil PUBLIC cxx::cxx)
endif ()

add_library(loader SHARED
    entry.cpp
    config/hooks.cpp
    process/self.cpp
    utils/dlext.cpp
    hook/engine.cpp
    hook/inline.cpp
    hook/plt.cpp
    hook/backend/dobby.cpp
    hook/backend/shadowhook.cpp
    hook/backend/rv64hook.cpp
    hook/backend/lsplt.cpp
    hook/backend/bytehook.cpp
    hook/backend/xhook.cpp
    api/hook.cpp
    api/symbols.cpp
    api/companion.cpp
    api/runtime.cpp
    api/table.cpp
    hyos/state.cpp
    hyos/hooks.cpp
    hyos/runtime.cpp
    ipc/channel.cpp
    module/plan.cpp
    module/loader.cpp
)
target_include_directories(loader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
if (ANDROID_ABI STREQUAL "arm64-v8a" OR ANDROID_ABI STREQUAL "armeabi-v7a")
    target_include_directories(loader PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/external/shadowhook/shadowhook/src/main/cpp/include)
endif ()
if (NOT ANDROID_ABI STREQUAL "riscv64")
    target_include_directories(loader PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/external/bytehook/bytehook/src/main/cpp/include)
    target_include_directories(loader PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/external/xhook/libxhook/jni)
endif ()

set(LOADER_LINK_LIBS log elfutil lsplt_static)
if (ANDROID_ABI STREQUAL "riscv64")
    set(LOADER_LINK_LIBS ${LOADER_LINK_LIBS} rv64hook-static)
else()
    set(LOADER_LINK_LIBS ${LOADER_LINK_LIBS} dobby)
endif()
if (ANDROID_ABI STREQUAL "arm64-v8a" OR ANDROID_ABI STREQUAL "armeabi-v7a")
    set(LOADER_LINK_LIBS ${LOADER_LINK_LIBS} shadowhook)
endif()
if (NOT ANDROID_ABI STREQUAL "riscv64")
    set(LOADER_LINK_LIBS ${LOADER_LINK_LIBS} bytehook)
    set(LOADER_LINK_LIBS ${LOADER_LINK_LIBS} xhook)
endif()
target_link_libraries(loader PRIVATE ${LOADER_LINK_LIBS})
if (TARGET cxx::cxx)
    target_link_libraries(loader PRIVATE cxx::cxx)
endif ()

target_compile_options(loader PRIVATE -Wall -Wextra -fvisibility=hidden)
