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

include("${WEBTHREAD_PROJECT_SOURCE_DIR}/thirdparty/boost.cmake")
include("${WEBTHREAD_PROJECT_SOURCE_DIR}/thirdparty/boost.wintls.cmake")
include("${WEBTHREAD_PROJECT_SOURCE_DIR}/thirdparty/CMakeSanitizer.cmake")
include("${WEBTHREAD_PROJECT_SOURCE_DIR}/thirdparty/googletest.cmake")

file(GLOB_RECURSE WEBTHREAD_TESTS_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE WEBTHREAD_TESTS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
set(
   WEBTHREAD_TESTS_THIRDPARTY
   "${WEBTHREAD_PROJECT_SOURCE_DIR}/thirdparty/boost.cmake"
   "${WEBTHREAD_PROJECT_SOURCE_DIR}/thirdparty/boost.wintls.cmake"
   "${WEBTHREAD_PROJECT_SOURCE_DIR}/thirdparty/CMakeCodeCov.cmake"
   "${WEBTHREAD_PROJECT_SOURCE_DIR}/thirdparty/CMakeSanitizer.cmake"
   "${WEBTHREAD_PROJECT_SOURCE_DIR}/thirdparty/CMakeTargets.cmake"
   "${WEBTHREAD_PROJECT_SOURCE_DIR}/thirdparty/googletest.cmake"
)
add_executable(tests ${WEBTHREAD_TESTS_HEADERS} ${WEBTHREAD_TESTS_SOURCES} ${WEBTHREAD_TESTS_THIRDPARTY})
add_executable(webthread::tests ALIAS tests)
set_target_properties(tests PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/" FILES ${WEBTHREAD_TESTS_HEADERS} ${WEBTHREAD_TESTS_SOURCES})
source_group(TREE "${WEBTHREAD_PROJECT_SOURCE_DIR}/" FILES ${WEBTHREAD_TESTS_THIRDPARTY})
target_compile_definitions(tests PUBLIC $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=0x0A00 WINVER=0x0A00 NOMINMAX>)
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
   target_compile_options(webthread PUBLIC /bigobj)
endif()
target_include_directories(tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tests PRIVATE Boost::beast CURL::libcurl gmock_main webthread $<$<PLATFORM_ID:Windows>:Boost::wintls>)
if(OPENSSL_FOUND)
   target_include_directories(tests PRIVATE "${OPENSSL_INCLUDE_DIR}")
   target_link_libraries(tests PRIVATE ${OPENSSL_LIBRARIES})
endif()
available_cxx_sanitizers(CXX_SANITIZERS)
list(LENGTH CXX_SANITIZERS CXX_SANITIZERS_LENGTH)
if(CXX_SANITIZERS_LENGTH GREATER 0)
   list(GET CXX_SANITIZERS 0 DEFAULT_CXX_SANITIZER)
   set(SANITIZER ${DEFAULT_CXX_SANITIZER} CACHE STRING "Choose sanitizer for the build")
   set_property(CACHE SANITIZER PROPERTY STRINGS ${CXX_SANITIZERS})
   if(SANITIZER STREQUAL "UBSan")
      target_cxx_ubsan(
         tests
         FLAGS
            exitcode=1
            print_stacktrace=true
            report_error_type=true
            silence_unsigned_overflow=true
         SUPPRESSIONS
            shift-base:*/c_ares-src/*
            shift-base:*/curl-src/*
            shift-base:*/MacOSX.platform/Developer/SDKs/*
      )
   else()
      target_cxx_sanitizer(tests ${SANITIZER} FLAGS exitcode=1)
   endif()
endif()
get_filename_component(CXX_LINKER_DIRECTORY ${CMAKE_LINKER} DIRECTORY)
string(LENGTH "${CXX_LINKER_DIRECTORY}" CXX_LINKER_DIRECTORY_LENGTH)
if(CXX_LINKER_DIRECTORY_LENGTH GREATER 0)
   set(WEBTHREAD_CTEST_PROPERTIES ENVIRONMENT "PATH=${CXX_LINKER_DIRECTORY};$ENV{PATH}")
endif()

include(GoogleTest)
gtest_discover_tests(tests PROPERTIES ${WEBTHREAD_CTEST_PROPERTIES})

add_coverage(tests)
