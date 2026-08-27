cmake_minimum_required(VERSION 3.22.1)
project(loader)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external)

if (LSPLT_STANDALONE)
    find_package(cxx REQUIRED CONFIG)
endif()

add_library(elfutil STATIC utils/elf_util.cpp)
target_include_directories(elfutil PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(elfutil PUBLIC lzma)
if (TARGET cxx::cxx)
    target_link_libraries(elfutil PUBLIC cxx::cxx)
endif()

add_library(loader SHARED loader.cpp)
target_include_directories(loader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(loader PRIVATE
    log
    elfutil
    dobby
    lsplt_static
)
if (TARGET cxx::cxx)
    target_link_libraries(loader PRIVATE cxx::cxx)
endif()

target_compile_options(loader PRIVATE -Wall -Wextra -fvisibility=hidden)

# The injector daemon lives in the top-level injector/ directory. It reuses the
# elfutil static library for ELF/maps parsing and handles both 32-bit and
# 64-bit targets at runtime.
add_executable(injector ${CMAKE_CURRENT_SOURCE_DIR}/injector.cpp)
target_include_directories(injector PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(injector PRIVATE ZNN_VERSION="${ZNN_VERSION}")
target_link_libraries(injector PRIVATE elfutil log)
if (TARGET cxx::cxx)
    target_link_libraries(injector PRIVATE cxx::cxx)
endif()
target_compile_options(injector PRIVATE -Wall -Wextra)
