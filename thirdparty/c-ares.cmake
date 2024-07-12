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

set(CARES_STATIC ON CACHE BOOL "Build as a static library" FORCE)
set(CARES_SHARED OFF CACHE BOOL "Disable build of shared library" FORCE)
set(CARES_INSTALL OFF CACHE BOOL "Disable install targets" FORCE)
set(CARES_STATIC_PIC ON CACHE BOOL "Build the static library as PIC (position independent)" FORCE)
set(CARES_BUILD_TESTS OFF CACHE BOOL "Disable tests targets" FORCE)
set(CARES_BUILD_CONTAINER_TESTS OFF CACHE BOOL "Disable tests targets" FORCE)
set(CARES_BUILD_TOOLS OFF CACHE BOOL "Disable tools targets" FORCE)
set(CARES_THREADS OFF CACHE BOOL "Disable thread-safety" FORCE)
FetchContent_Declare(
   c_ares
   # Download Step Options
   GIT_PROGRESS ON
   GIT_REMOTE_UPDATE_STRATEGY CHECKOUT
   GIT_REPOSITORY https://github.com/c-ares/c-ares.git
   GIT_SHALLOW ON
   GIT_SUBMODULES_RECURSE ON
   GIT_TAG v1.32.3
)
FetchContent_MakeAvailable(c_ares)
set(CARES_INCLUDE_DIR "${c_ares_SOURCE_DIR}/include/")
set(CARES_LIBRARY ${CARES_LIBRARIES})
organize_thirdparty_directory_targets("${c_ares_SOURCE_DIR}" thirdparty)
