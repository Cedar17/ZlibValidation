# CMakeLists.txt
cmake_minimum_required(VERSION 3.25)
project(ZlibValidation
    LANGUAGES CXX
    VERSION 0.1.0
)

# Define author information (CMake 3.21+)
set(PROJECT_AUTHOR "Song Zixuan" CACHE STRING "Project maintainer")
set(PROJECT_CONTACT "cedar@zju.edu.cn" CACHE STRING "Contact info")

# Generate version header file
string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/version.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/include/version.h
)

# # Enable debug info
# set(CMAKE_BUILD_TYPE Debug)

# Set build type to Release for optimization
set(CMAKE_BUILD_TYPE Release)

# Set specific optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the default generator to Unix Makefiles. This is optional and can be modified if needed.
# The CACHE INTERNAL "" FORCE parameters ensure that this setting is applied globally and cannot be overridden by the user.
set(CMAKE_GENERATOR "Unix Makefiles" CACHE INTERNAL "" FORCE)

# Enable verbose output from the makefile. This is optional and useful for debugging purposes.
# When enabled, the makefile will print detailed information about the build process.
set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "ON" FORCE)

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

# Fetch nonlohmann/json
FetchContent_Declare(
    json 
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# Fetch tabulate
FetchContent_Declare(
    tabulate
    GIT_REPOSITORY https://github.com/p-ranav/tabulate.git
    GIT_TAG v1.5
)
FetchContent_MakeAvailable(tabulate)

# Collect all source and header files
file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)
file(GLOB_RECURSE HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)

# Add target
add_executable(zlibvalidation ${SOURCES} ${HEADERS})

# Link libraries
target_include_directories(zlibvalidation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_directories(zlibvalidation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib)
target_link_libraries(zlibvalidation PRIVATE 
    si2dr_liberty
    CLI11::CLI11 
    spdlog::spdlog 
    nlohmann_json::nlohmann_json
    tabulate::tabulate
    stdc++fs # Required for filesystem support in C++17
)