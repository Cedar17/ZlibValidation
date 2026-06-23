# Test utilities (header-only/INTERFACE)
add_library(test_utils INTERFACE)
target_include_directories(test_utils INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include_3rd_party
)

# -----------------------------------------------------------------------
# Per-test extra source mapping
# Tests that need project source files beyond their own .cpp list them here.
# -----------------------------------------------------------------------
set(TEST_EXTRA_SOURCES_test_libdb    "${PROJECT_SOURCE_DIR}/src/LibDatabase.cpp")
set(TEST_EXTRA_SOURCES_test_libfile_db
    ${PROJECT_SOURCE_DIR}/src/LibDatabase.cpp
    ${PROJECT_SOURCE_DIR}/src/LibFile.cpp
    ${PROJECT_SOURCE_DIR}/src/Iterators.cpp
    ${PROJECT_SOURCE_DIR}/src/LibGroup.cpp
    ${PROJECT_SOURCE_DIR}/src/LibAtrribute.cpp
    ${PROJECT_SOURCE_DIR}/src/json_utils.cpp
    ${PROJECT_SOURCE_DIR}/src/verilog_utils.cpp
)

# Discover all .cpp test files under test/ (allows adding new tests without editing CMake)
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
foreach(src IN LISTS TEST_SOURCES)
  get_filename_component(tname ${src} NAME_WE)
  if(tname STREQUAL "main")
    continue()
  endif()

  # Build source list: test file + any extra sources
  set(test_sources "${src}")
  if(DEFINED TEST_EXTRA_SOURCES_${tname})
    list(APPEND test_sources ${TEST_EXTRA_SOURCES_${tname}})
  endif()

  add_executable(${tname} ${test_sources})
  target_include_directories(${tname} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include_3rd_party
    ${CMAKE_CURRENT_SOURCE_DIR}
  )
  target_compile_definitions(${tname} PRIVATE
    TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/pdk"
  )
  target_link_libraries(${tname} PRIVATE
    gtest_main SQLiteCpp test_utils
    nlohmann_json::nlohmann_json
    tabulate::tabulate
    spdlog::spdlog
    slang::slang
    si2dr_liberty
  )
  target_link_directories(${tname} PRIVATE ${PROJECT_SOURCE_DIR}/lib)
  target_link_options(${tname} PRIVATE "-Wl,--allow-multiple-definition")
  target_compile_features(${tname} PRIVATE cxx_std_20)
  include(GoogleTest)
  gtest_discover_tests(${tname})
endforeach()