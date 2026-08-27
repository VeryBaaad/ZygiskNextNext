cmake_minimum_required(VERSION 3.22.1)
project(external)

# The Gradle build passes -DANDROID_STL=none and supplies libc++ through the
# dev.rikka.ndk.thirdparty:cxx prefab. Make it available to every native target
# here (Dobby's symbol resolver plugin uses a little std::string/std::vector).
if (LSPLT_STANDALONE)
    find_package(cxx REQUIRED CONFIG)
endif()

option(LSPLT_BUILD_SHARED "Build LSPlt as a shared library" OFF)
add_subdirectory(lsplt/lsplt/src/main/jni)

option(DOBBY_GENERATE_SHARED "Build Dobby as a shared library" OFF)
option(BUILD_EXAMPLE "Build Dobby example" OFF)
option(BUILD_TEST "Build Dobby test" OFF)
add_subdirectory(dobby)

add_subdirectory(lzma)

# Dobby is vendored third-party code compiled with -Wall -Wextra; silence its
# internal warnings so our own targets stay warning-clean.
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
)

if (TARGET cxx::cxx)
    # Plain signature on purpose: Dobby's own CMakeLists links libraries with the
    # plain signature, and CMake requires consistent usage per target.
    target_link_libraries(dobby cxx::cxx)
endif()
