enable_testing()

# Test utilities (header-only/INTERFACE)
add_library(test_utils INTERFACE)
target_include_directories(test_utils INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include_3rd_party
)

# Discover all .cpp test files under test/ (allows adding new tests without editing CMake)
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
foreach(src IN LISTS TEST_SOURCES)
  get_filename_component(tname ${src} NAME_WE)
  if(tname STREQUAL "main")
    continue()
  endif()
  add_executable(${tname} ${src})
  target_include_directories(${tname} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include_3rd_party
    ${CMAKE_CURRENT_SOURCE_DIR}
  )
  target_link_libraries(${tname} PRIVATE gtest_main SQLiteCpp test_utils)
  target_compile_features(${tname} PRIVATE cxx_std_20)
  include(GoogleTest)
  gtest_discover_tests(${tname})
endforeach()
