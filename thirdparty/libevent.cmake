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

set(EVENT__LIBRARY_TYPE STATIC CACHE STRING "Build as a static library" FORCE)
set(EVENT__DISABLE_DEBUG_MODE ON CACHE BOOL "Disable debug mode" FORCE)
set(EVENT__ENABLE_VERBOSE_DEBUG OFF CACHE BOOL "Disable debug mode" FORCE)
set(EVENT__DISABLE_THREAD_SUPPORT ON CACHE BOOL "Disable thread-safety" FORCE)
set(EVENT__DISABLE_OPENSSL ON CACHE BOOL "Disable OpenSSL" FORCE)
set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "Disable betchmark targets" FORCE)
set(EVENT__DISABLE_TESTS ON CACHE BOOL "Disable tests targets" FORCE)
set(EVENT__DISABLE_REGRESS ON CACHE BOOL "Disable tests targets" FORCE)
set(EVENT__DISABLE_SAMPLES ON CACHE BOOL "Disable samples targets" FORCE)
if(MSVC)
   set(EVENT__MSVC_STATIC_RUNTIME OFF CACHE BOOL "Link shared runtime libraries" FORCE)
endif()
FetchContent_Declare(
    libevent
   # Download Step Options
   GIT_PROGRESS ON
   GIT_REMOTE_UPDATE_STRATEGY CHECKOUT
   GIT_REPOSITORY https://github.com/libevent/libevent.git
   GIT_SHALLOW ON
   GIT_SUBMODULES_RECURSE ON
   GIT_TAG release-2.1.12-stable
)
FetchContent_MakeAvailable(libevent)
organize_thirdparty_directory_targets("${libevent_SOURCE_DIR}" thirdparty)
