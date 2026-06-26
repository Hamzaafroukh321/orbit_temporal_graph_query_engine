cmake_minimum_required(VERSION 3.24)

project(orbit_temporal_graph_query_engine
  VERSION 0.1.0
  DESCRIPTION "Orbit temporal property-graph query engine"
  LANGUAGES CXX)

option(ORBIT_BUILD_TESTS "Build Orbit tests" ON)
option(ORBIT_BUILD_FUZZ "Build Orbit fuzz smoke targets" ON)
option(ORBIT_BUILD_TOOLS "Build Orbit developer tools" ON)
option(ORBIT_WARNINGS_AS_ERRORS "Treat first-party warnings as errors" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/Warnings.cmake)
include(cmake/Sanitizers.cmake)

add_library(orbit
  src/base/checked.cpp
  src/base/error.cpp
  src/base/value.cpp
  src/format/ogr.cpp
  src/store/graph_store.cpp
  src/query/query.cpp)
add_library(orbit::orbit ALIAS orbit)
target_include_directories(orbit PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
orbit_set_warnings(orbit)
orbit_enable_sanitizers(orbit)

add_executable(orbit_cli src/cli/main.cpp)
set_target_properties(orbit_cli PROPERTIES OUTPUT_NAME orbit)
target_link_libraries(orbit_cli PRIVATE orbit::orbit)
orbit_set_warnings(orbit_cli)
orbit_enable_sanitizers(orbit_cli)

if(ORBIT_BUILD_TESTS)
  enable_testing()
  add_executable(orbit_unit_tests
    tests/unit/test_main.cpp
    tests/unit/base_tests.cpp
    tests/unit/format_tests.cpp
    tests/integration/store_query_tests.cpp)
  target_link_libraries(orbit_unit_tests PRIVATE orbit::orbit)
  target_include_directories(orbit_unit_tests PRIVATE tests/unit)
  orbit_set_warnings(orbit_unit_tests)
  orbit_enable_sanitizers(orbit_unit_tests)
  add_test(NAME orbit_unit_tests COMMAND orbit_unit_tests)
endif()

if(ORBIT_BUILD_FUZZ)
  include(cmake/FuzzTargets.cmake)
  orbit_add_fuzz_smoke(orbit_ogr_parser_fuzz fuzz/fuzz_ogr_parser.cpp)
  orbit_add_fuzz_smoke(orbit_graph_sequence_fuzz fuzz/fuzz_graph_sequence.cpp)
  orbit_add_fuzz_smoke(orbit_query_pipeline_fuzz fuzz/fuzz_query_pipeline.cpp)
endif()

if(ORBIT_BUILD_TOOLS)
  add_executable(orbit_benchmark tools/orbit_benchmark.cpp)
  target_link_libraries(orbit_benchmark PRIVATE orbit::orbit)
  orbit_set_warnings(orbit_benchmark)
  orbit_enable_sanitizers(orbit_benchmark)
endif()

install(TARGETS orbit orbit_cli EXPORT orbitTargets)
install(DIRECTORY include/ DESTINATION include)
