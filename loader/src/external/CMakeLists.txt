cmake_minimum_required(VERSION 3.22.1)
project(external)

if (ANDROID_ABI STREQUAL "riscv64")
	set(ANDROID_PLATFORM "android-35")
endif ()

option(LSPLT_BUILD_SHARED "Build LSPlt as a shared library" OFF)
add_subdirectory(lsplt/lsplt/src/main/jni)

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
    )
endif ()

add_subdirectory(lzma)
