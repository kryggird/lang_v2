cmake_minimum_required(VERSION 3.13)
project(lang_v2)

include(FetchContent)
FetchContent_Declare(
        cpp-peglib
        GIT_REPOSITORY https://github.com/yhirose/cpp-peglib.git
        GIT_TAG        master
)

FetchContent_GetProperties(cpp-peglib)
if(NOT cpp-peglib_POPULATED)
    FetchContent_Populate(cpp-peglib)
#    add_subdirectory(${cpp-peglib_SOURCE_DIR} ${cpp-peglib_BINARY_DIR})
endif()

set(CMAKE_CXX_STANDARD 17)

add_executable(lang_v2 src/main.cpp src/cfg.h)
target_include_directories(lang_v2 PRIVATE ${cpp-peglib_SOURCE_DIR})