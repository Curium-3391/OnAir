# libonair.so for Android = the desktop receiver library (dect2core, from this repository) + a JNI bridge.
# The OnAir source tree is only read from here. By default it is this checkout itself (android/native/../..); ONAIR_SRC can point at
# another checkout or worktree. Built by android/tools/build_native.sh (see android/README.md).
cmake_minimum_required(VERSION 3.22)
project(onair_android LANGUAGES C CXX)

get_filename_component(_onair_repo "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(ONAIR_SRC "${_onair_repo}" CACHE PATH "OnAir repository root (holds the receiver library); default: this checkout")
if(NOT EXISTS "${ONAIR_SRC}/CMakeLists.txt" OR NOT EXISTS "${ONAIR_SRC}/core/include")
  message(FATAL_ERROR "ONAIR_SRC does not hold the OnAir sources: ${ONAIR_SRC}")
endif()

# The core only: no desktop app, no tests, no SoapySDR or Hamlib (none of them exists for Android here). Rust is mandatory in the core
# (cmake/OnairRust.cmake): the target aarch64-linux-android must be installed (rustup target add aarch64-linux-android).
set(DECT2_BUILD_APP OFF CACHE BOOL "" FORCE)
set(DECT2_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DECT2_WITH_SOAPY OFF CACHE BOOL "" FORCE)
set(DECT2_HAVE_HAMLIB OFF CACHE BOOL "" FORCE)
add_subdirectory("${ONAIR_SRC}" "${CMAKE_BINARY_DIR}/onair-core" EXCLUDE_FROM_ALL)

add_library(onair SHARED
  src/jni_onair.cpp
  src/results_json.cpp
  src/stubs_ext.c
)
set_target_properties(onair PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(onair PRIVATE "${ONAIR_SRC}/core/include" "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_options(onair PRIVATE -O2 -fvisibility=hidden -Wall)
# the core is a very large static library: keep only what the bridge reaches (the engine reaches every mode), drop the rest
target_link_libraries(onair PRIVATE dect2core log android)
# 16 KB pages (Android 15+ devices), no build id so two builds of the same tree give the same file
target_link_options(onair PRIVATE -Wl,--gc-sections -Wl,--exclude-libs,ALL -Wl,-z,max-page-size=16384 -Wl,--build-id=none)
