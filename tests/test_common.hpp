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

#pragma once

#include <gtest/gtest.h> ///< for EXPECT_FALSE

#include <array> ///< for std::to_array
#include <chrono> ///< for std::chrono::milliseconds, std::chrono::seconds
#include <cstdint> ///< for uint16_t
#include <string_view> ///< for std::string_view

constexpr uint16_t test_cpu_id = 0;
constexpr auto test_non_routable_ips = std::to_array<std::string_view>({
   "10.0.0.0",
   "10.255.255.1"
   "10.255.255.255",
   "172.16.0.0",
   "192.168.0.0",
   "192.168.255.255"
});
#if (defined(__APPLE__))
constexpr auto test_loopback_interface = std::string_view{"lo0"};
#elif (defined(__linux__))
constexpr auto test_loopback_interface = std::string_view{"lo"};
#endif
constexpr auto test_loopback_ip = std::string_view{"127.0.0.1"};
constexpr uint16_t test_peer_port = 81;
constexpr uint16_t test_peer_ssl_port = 444;

constexpr auto test_keep_alive_delay = std::chrono::seconds{1};
constexpr auto test_bad_request_timeout = std::chrono::milliseconds{100};
constexpr auto test_good_request_timeout = std::chrono::seconds{1};

#define EXPECT_ERROR_CODE(testErrorCode)  \
   EXPECT_FALSE(testErrorCode.failed())   \
      << testErrorCode.value() << ": " << testErrorCode.message()
