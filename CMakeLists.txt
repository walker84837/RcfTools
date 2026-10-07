cmake_minimum_required(VERSION 3.16)
project(RcfTools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(ZLIB REQUIRED)

add_executable(RcfTools
  main.cpp
  rcf_file.cpp
  rcf_file.h
  rcf.h
  rcf_hash.h
  rz.h
  rz.cpp
  utils.h
)

if(MSVC)
  target_compile_options(RcfTools PRIVATE /W3 /permissive-)
else()
  target_compile_options(RcfTools PRIVATE -Wall -Wextra)
endif()

target_link_libraries(RcfTools PRIVATE ZLIB::ZLIB)
