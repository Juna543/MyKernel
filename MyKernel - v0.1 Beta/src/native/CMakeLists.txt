# CMakeLists.txt — builds the mykernel_native Python extension module.
#
# Requires: CMake >= 3.15, a C++17 compiler, Python 3 development
# headers, pybind11, and OpenSSL (libcrypto). See BUILD.md at the
# project root for platform-specific install instructions (WSL/Alpine,
# Ubuntu, MinGW).

cmake_minimum_required(VERSION 3.15)
project(mykernel_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to a Release build unless the caller specifies otherwise.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
find_package(pybind11 REQUIRED)
find_package(OpenSSL REQUIRED)

pybind11_add_module(mykernel_native
    bindings.cpp
    crypto.cpp
    isolation.cpp
)

target_link_libraries(mykernel_native PRIVATE OpenSSL::Crypto)

target_compile_options(mykernel_native PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
)

# Place the built module next to the Python package so
# `import mykernel_native` works from src/ during development.
set_target_properties(mykernel_native PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../mykernel"
)
