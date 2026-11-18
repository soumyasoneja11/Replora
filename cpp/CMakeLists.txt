cmake_minimum_required(VERSION 3.20)
project(replora LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

include(CTest)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
  GIT_SHALLOW TRUE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

if(BUILD_TESTING)
  add_executable(replora_tests tests/smoke_test.cpp)
  target_link_libraries(replora_tests PRIVATE replora_core GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(replora_tests DISCOVERY_MODE PRE_TEST)
endif()

# CLI placeholder. Same replora_core sources; do not add a second engine.
# add_executable(replora_cli cli/main.cpp)
# target_link_libraries(replora_cli PRIVATE replora_core)

# WASM placeholder. Emscripten builds this target from the same cpp/src files.
# add_executable(replora_wasm wasm/bindings.cpp)
# target_link_libraries(replora_wasm PRIVATE replora_core)
