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

include(CMakeThirdpartyTargets)
include(FetchContent)

set(BUILD_CURL_EXE OFF CACHE BOOL "Disable curl executable" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Disable build of shared libraries" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "Enable build of static libraries" FORCE)
set(ENABLE_ARES ON CACHE BOOL "Enable c-ares support" FORCE)
set(CURL_DISABLE_INSTALL ON CACHE BOOL "Disable install targets" FORCE)
set(CURL_ENABLE_EXPORT_TARGET OFF CACHE BOOL "Disable cmake export target" FORCE)
set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "Disable documentation targets" FORCE)
set(BUILD_MISC_DOCS OFF CACHE BOOL "Disable documentation targets" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Disable examples targets" FORCE)
set(ENABLE_CURL_MANUAL OFF CACHE BOOL "Disable documentation targets" FORCE)
set(ENABLE_WEBSOCKETS ON CACHE BOOL "Enable websockets" FORCE)
if(WIN32)
   set(CURL_USE_SCHANNEL ON CACHE BOOL "Enable Windows native SSL/TLS" FORCE)
   set(CURL_WINDOWS_SSPI ON CACHE BOOL "Enable SSPI on Windows" FORCE)
elseif(OPENSSL_FOUND)
   set(CURL_USE_OPENSSL ON CACHE BOOL "Enable OpenSSL" FORCE)
elseif(APPLE)
   set(CURL_USE_SECTRANSP ON CACHE BOOL "Enable Apple OS native SSL/TLS" FORCE)
endif()
FetchContent_Declare(
   curl
   # Download Step Options
   GIT_PROGRESS ON
   GIT_REMOTE_UPDATE_STRATEGY CHECKOUT
   GIT_REPOSITORY https://github.com/curl/curl.git
   GIT_SHALLOW ON
   GIT_SUBMODULES_RECURSE ON
   GIT_TAG curl-8_9_1
)
FetchContent_MakeAvailable(curl)
if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
   # Disable Additional Security Checks
   set_target_c_compile_flag(libcurl_object PRIVATE /sdl-)
   # Enable Language Extensions (windows headers does not compile without)
   set_target_properties(libcurl_object PROPERTIES C_EXTENSIONS ON)
endif()
organize_thirdparty_directory_targets("${curl_SOURCE_DIR}" thirdparty)
