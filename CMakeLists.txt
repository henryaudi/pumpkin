cmake_minimum_required(VERSION 3.25)
project(pumpkin VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PUMPKIN_WERROR "Treat warnings as errors (CI turns this on)" OFF)
option(PUMPKIN_BUILD_TESTS "Build tests" ON)
set(PUMPKIN_SANITIZE "" CACHE STRING "Sanitizers: 'address,undefined' | 'thread' | empty")

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(Sanitizers)        # global: must apply to every TU, incl. third-party (TSan requires it)
include(CompilerWarnings)  # per-target: pumpkin::warnings

add_subdirectory(src)
add_subdirectory(apps)

if(PUMPKIN_BUILD_TESTS)
  enable_testing()
  include(Dependencies)
  add_subdirectory(tests)
endif()