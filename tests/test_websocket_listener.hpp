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

#include <webthread/websocket_listener.hpp> ///< for web::websocket_listener
#include <webthread/websocket_message_type.hpp> ///< for web::websocket_message_type

#include <gmock/gmock.h> ///< for MOCK_METHOD

#include <string_view> ///< for std::string_view

class [[nodiscard]] test_websocket_listener : public web::websocket_listener
{
public:
   test_websocket_listener() noexcept = default;
   test_websocket_listener(test_websocket_listener &&) = delete;
   test_websocket_listener(test_websocket_listener const &) = delete;

   test_websocket_listener &operator = (test_websocket_listener &&) = delete;
   test_websocket_listener &operator = (test_websocket_listener const &) = delete;

   MOCK_METHOD(void, on_error, (int testErrorCode, std::string_view testErrorDescription), (override));
   MOCK_METHOD(void, on_message_recv, (std::string_view testData, web::websocket_message_type testMessageType, size_t testBytesLeft), (override));
   MOCK_METHOD(void, on_message_sent, (), (override));
};
