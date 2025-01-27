cmake_minimum_required(VERSION 3.25)
project(ZlibValidation
    LANGUAGES CXX
    VERSION 1.0.0
)

# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default generator to Unix Makefiles (可选，如果需要)
set(CMAKE_GENERATOR "Unix Makefiles" CACHE INTERNAL "" FORCE)

# Enable verbose output from the makefile (可选，用于调试)
set(CMAKE_VERBOSE_MAKEFILE ON)

# Fetch Content for dependencies
include(FetchContent)

# Fetch CLI11
FetchContent_Declare(
    cli11_proj
    QUIET
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.2
)
FetchContent_MakeAvailable(cli11_proj)

# Fetch spdlog
FetchContent_Declare(
    spdlog_proj
    QUIET
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.0
)
FetchContent_MakeAvailable(spdlog_proj)

# Add target
add_executable(zlibvalidation)
target_sources(zlibvalidation PRIVATE src/main.cpp)

# Link libraries
target_include_directories(zlibvalidation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_directories(zlibvalidation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib)
target_link_libraries(zlibvalidation PRIVATE si2dr_liberty CLI11::CLI11 spdlog::spdlog)