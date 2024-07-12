#[[
   Part of the webThread Project (https://github.com/cpp4ever/webthread), under the MIT License
   SPDX-License-Identifier: MIT

   Copyright (c) 2024 Mikhail Smirnov

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
]]

cmake_minimum_required(VERSION 3.20)

project(webthread LANGUAGES C CXX)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

find_package(OpenSSL QUIET)

include("${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/CMakeTargets.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/c-ares.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/curl.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libevent.cmake")

file(GLOB_RECURSE WEBTHREAD_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")
file(GLOB_RECURSE WEBTHREAD_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
set(
   WEBTHREAD_THIRDPARTY
   "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/c-ares.cmake"
   "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/CMakeTargets.cmake"
   "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/curl.cmake"
   "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libevent.cmake"
)
add_library(webthread STATIC ${WEBTHREAD_HEADERS} ${WEBTHREAD_SOURCES} ${WEBTHREAD_THIRDPARTY})
add_library(webthread::webthread ALIAS webthread)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include/webthread/" PREFIX "include" FILES ${WEBTHREAD_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src/" PREFIX "src" FILES ${WEBTHREAD_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/" FILES ${WEBTHREAD_THIRDPARTY})
if(WIN32)
   # Minimal Windows wersions with TLS 1.3 support:
   #  - Windows 11, Version 21H2 (10.0.22000)
   #  - Windows Server 2022, Version 21H2 (10.0.20348)
   if(CMAKE_SYSTEM_VERSION VERSION_GREATER_EQUAL "10.0.20348")
      target_compile_definitions(webthread PUBLIC WEBTHREAD_TLSv1_3=1)
   endif()
elseif(OPENSSL_FOUND)
   if(OPENSSL_VERSION VERSION_GREATER_EQUAL "1.1.1")
      target_compile_definitions(webthread PUBLIC WEBTHREAD_TLSv1_3=1)
   endif()
endif()
target_include_directories(webthread PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")
target_link_libraries(webthread PRIVATE CURL::libcurl event)

get_filename_component(WEBTHREAD_PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} REALPATH)
get_filename_component(MAIN_PROJECT_SOURCE_DIR ${CMAKE_SOURCE_DIR} REALPATH)
if(WEBTHREAD_PROJECT_SOURCE_DIR STREQUAL MAIN_PROJECT_SOURCE_DIR)
   include("${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/CMakeCodeCov.cmake")

   if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
      target_compile_options(webthread PUBLIC /W4)
   else()
      target_compile_options(webthread PUBLIC -Wall -Wextra -Wpedantic)
   endif()
   enable_testing()
   add_subdirectory(tests)
   set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT tests)

   add_coverage(webthread)
   coverage_evaluate()
endif()
