cmake_minimum_required(VERSION 3.25)
project(ZlibValidation
    LANGUAGES CXX
    VERSION 0.1.0
)

# Define author information (CMake 3.21+)
set(PROJECT_AUTHOR "Song Zixuan" CACHE STRING "Project maintainer")
set(PROJECT_CONTACT "cedar@zju.edu.cn" CACHE STRING "Contact info")

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "Build type not specified, defaulting to Release")
    # Set specific optimization flags
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
endif()

# Generate version header file
string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/version.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/include/version.h
)

# Disable Position Independent Executable (PIE) to ensure fixed memory layout
# This may be required for certain libraries that assume absolute addressing
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -no-pie")

# Fetch Content for dependencies
include(FetchContent)

# Fetch CLI11
FetchContent_Declare(
    cli11_proj
    QUIET
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_SHALLOW ON
    GIT_TAG v2.4.2
)
FetchContent_MakeAvailable(cli11_proj)

# Fetch spdlog
FetchContent_Declare(
    spdlog_proj
    QUIET
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_SHALLOW ON
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
    GIT_SHALLOW ON
    GIT_TAG v1.5
)
FetchContent_MakeAvailable(tabulate)

# Fetch slang
FetchContent_Declare(
    slang
    GIT_REPOSITORY https://github.com/MikePopoloski/slang.git
    GIT_SHALLOW ON
    GIT_TAG v8.0
)
FetchContent_MakeAvailable(slang)

# Collect all source files
file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# Collect header files
file(GLOB_RECURSE HEADERS 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
)

# Add target
add_executable(zlibvalidation ${SOURCES} ${HEADERS})

# Set C++ standard at target level
target_compile_features(zlibvalidation PRIVATE cxx_std_20)

# Link libraries
# Configure includes and libraries
target_include_directories(zlibvalidation PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
)

target_link_directories(zlibvalidation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib)
target_link_libraries(zlibvalidation PRIVATE 
    si2dr_liberty
    CLI11::CLI11 
    spdlog::spdlog 
    nlohmann_json::nlohmann_json
    tabulate::tabulate
    slang::slang
)

# Allow multiple definition of symbols
target_link_options(zlibvalidation PRIVATE "-Wl,--allow-multiple-definition")