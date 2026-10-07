cmake_minimum_required(VERSION 3.16)
project(RcfTools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(cxxopts 3 QUIET CONFIG)
if(NOT cxxopts_FOUND)
  include(FetchContent)
  set(CXXOPTS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(CXXOPTS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(CXXOPTS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG v3.3.1
    GIT_SHALLOW TRUE
    FIND_PACKAGE_ARGS 3 CONFIG
  )
  FetchContent_MakeAvailable(cxxopts)
endif()

add_executable(RcfTools
  main.cpp
  rcf_file.cpp
  rcf_file.h
  rcf.h
  utils.h
)
target_link_libraries(RcfTools PRIVATE cxxopts::cxxopts)

if(MSVC)
  target_compile_options(RcfTools PRIVATE /W3 /permissive-)
else()
  target_compile_options(RcfTools PRIVATE -Wall -Wextra)
endif()
