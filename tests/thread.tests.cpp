/*
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
*/

#include "test_common.hpp" ///< for test_cpu_id

#include <webthread/thread.hpp> ///< for web::thread
#include <webthread/thread_config.hpp> ///< for web::thread_config

#include <gtest/gtest.h> ///< for TEST

#include <cstddef> ///< for size_t

TEST(Web, Thread)
{
   constexpr size_t testConnectionsCapacity = 1;
   constexpr size_t testMaxExpectedInboundMessageSize = 1024 * 1024;
   auto const testThread = web::thread
   {
      web::thread_config{}
         .with_cpu_affinity(web::thread_config::no_affinity)
         .with_initial_connections_capacity(testConnectionsCapacity)
         .with_max_expected_inbound_message_size(testMaxExpectedInboundMessageSize)
   };
   auto testThreadCopy = web::thread{testThread};
   [[maybe_unused]] auto const testThreadClone = web::thread{std::move(testThreadCopy)};
}