cmake_minimum_required(VERSION 3.20)
project(replora LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Wasm uses the Emscripten toolchain (emcmake or the "wasm" configure preset).
# Native configure ignores this. Turning it on without that toolchain is an error.
option(REPLORA_WASM "Build public/wasm/replora.js and replora.wasm" OFF)
if(EMSCRIPTEN)
  set(REPLORA_WASM ON)
  set(BUILD_TESTING OFF CACHE BOOL "Native tests are not part of the wasm build" FORCE)
endif()
if(REPLORA_WASM AND NOT EMSCRIPTEN)
  message(FATAL_ERROR
    "REPLORA_WASM=ON needs the Emscripten toolchain. "
    "Install emsdk, run emsdk_env, then: emcmake cmake -S cpp -B build/wasm")
endif()

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra)
endif()

file(GLOB REPLORA_CORE_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(replora_core STATIC ${REPLORA_CORE_SOURCES})
target_include_directories(replora_core PUBLIC
                           "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(EMSCRIPTEN)
  # try/catch in the binding must observe exceptions from replora_core.
  target_compile_options(replora_core PRIVATE -fexceptions)
endif()

# Same replora_core sources. cli/ only parses flags and prints engine output.
if(NOT EMSCRIPTEN)
  add_executable(replora_cli cli/main.cpp)
  target_link_libraries(replora_cli PRIVATE replora_core)
  if(MINGW)
    target_link_options(replora_cli PRIVATE -static)
  endif()
endif()

include(CTest)

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

if(BUILD_TESTING)
  function(replora_add_test_executable target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE replora_core GTest::gtest_main)
    if(MINGW)
      # Keep the test binary off the shared MinGW runtime. An older
      # libwinpthread earlier on PATH (for example from PostgreSQL) otherwise
      # aborts the process at startup.
      target_link_options(${target} PRIVATE -static)
    endif()
    target_compile_definitions(${target} PRIVATE
                               REPLORA_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/../tests/golden")
  endfunction()

  replora_add_test_executable(replora_tests
                 tests/smoke_test.cpp
                 tests/fifo_test.cpp
                 tests/lru_test.cpp
                 tests/lfu_test.cpp
                 tests/random_test.cpp
                 tests/workloads_test.cpp
                 tests/engine_test.cpp
                 tests/engine_properties_test.cpp
                 tests/cli_test.cpp)
  add_dependencies(replora_tests replora_cli)
  target_include_directories(replora_tests PRIVATE
                             "${CMAKE_CURRENT_BINARY_DIR}/generated")
  file(GENERATE
       OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/replora_cli_path.hpp"
       CONTENT "#pragma once\nstatic constexpr const char* kReploraCliPath = R\"path($<TARGET_FILE:replora_cli>)path\";\n")
  replora_add_test_executable(replora_slow_tests tests/engine_slow_test.cpp)
  include(GoogleTest)
  gtest_discover_tests(replora_tests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS fast)
  gtest_discover_tests(replora_slow_tests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS slow)
endif()

# Emscripten flags (also documented in cpp/README-dev.md):
#   -sMODULARIZE=1          factory function, not a global Module
#   -sEXPORT_ES6=1          export default factory for the browser and Node
#   -sALLOW_MEMORY_GROWTH=1  traces up to 10000 requests can exceed the initial heap
#   -sEXPORT_NAME=createReplora
#   -sENVIRONMENT=web,node  browser viewer and the Node parity script
#   --bind                  embind wrapper for the JSON string
#   -fexceptions            binding catches C++ exceptions and returns JSON
if(EMSCRIPTEN)
  set(REPLORA_WASM_DIR "${CMAKE_SOURCE_DIR}/../public/wasm")
  add_executable(replora_wasm wasm/bindings.cpp)
  target_link_libraries(replora_wasm PRIVATE replora_core)
  target_compile_options(replora_wasm PRIVATE -fexceptions)
  target_link_options(replora_wasm PRIVATE
    -fexceptions
    "SHELL:--bind"
    "SHELL:-s MODULARIZE=1"
    "SHELL:-s EXPORT_ES6=1"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s EXPORT_NAME=createReplora"
    "SHELL:-s ENVIRONMENT=web,node")
  set_target_properties(replora_wasm PROPERTIES
    OUTPUT_NAME "replora"
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${REPLORA_WASM_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${REPLORA_WASM_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${REPLORA_WASM_DIR}")
endif()
