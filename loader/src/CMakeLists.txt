cmake_minimum_required(VERSION 3.22.1)
project(loader)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external)

add_library(elfutil STATIC utils/elf_util.cpp)
target_include_directories(elfutil PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(elfutil PUBLIC lzma)

add_library(loader SHARED loader.cpp)
target_include_directories(loader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(LOADER_LINK_LIBS log elfutil lsplt_static)
if (ANDROID_ABI STREQUAL "riscv64")
    set(LOADER_LINK_LIBS ${LOADER_LINK_LIBS} rv64hook-static)
else()
    set(LOADER_LINK_LIBS ${LOADER_LINK_LIBS} dobby)
endif()
target_link_libraries(loader PRIVATE ${LOADER_LINK_LIBS})

target_compile_options(loader PRIVATE -Wall -Wextra -fvisibility=hidden)

add_executable(injector ${CMAKE_CURRENT_SOURCE_DIR}/injector.cpp)
target_include_directories(injector PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(injector PRIVATE ZNN_VERSION="${ZNN_VERSION}")
target_link_libraries(injector PRIVATE elfutil log)
target_compile_options(injector PRIVATE -Wall -Wextra)
