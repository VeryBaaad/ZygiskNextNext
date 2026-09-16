cmake_minimum_required(VERSION 3.22.1)
project(injector)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The shared native code this executable needs lives outside this module, under
# includes/ at the repository root.
set(ZNN_INCLUDES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../includes)

add_executable(injector
    injector.cpp
    ${ZNN_INCLUDES_DIR}/maps_util.cpp)
target_include_directories(injector PRIVATE ${ZNN_INCLUDES_DIR})
target_compile_definitions(injector PRIVATE ZNN_VERSION="${ZNN_VERSION}")
target_link_libraries(injector PRIVATE log)
target_compile_options(injector PRIVATE -Wall -Wextra)
