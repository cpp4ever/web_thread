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

#include "webthread/websocket_message_type.hpp" ///< for web::websocket_message_type

#include <cstddef> ///< for size_t
#include <string_view> ///< for std::string_view

namespace web
{

class [[nodiscard("discarded-value expression detected")]] websocket_listener
{
public:
   websocket_listener(websocket_listener &&) = delete;
   websocket_listener(websocket_listener const &) = delete;
   virtual ~websocket_listener() noexcept = default;

   websocket_listener &operator = (websocket_listener &&) = delete;
   websocket_listener &operator = (websocket_listener const &) = delete;

   virtual void on_error(intptr_t errorCode, std::string_view description) = 0;
   virtual void on_message_recv(
      std::string_view data,
      websocket_message_type messageType,
      size_t bytesLeft
   ) = 0;
   virtual void on_message_sent() = 0;
   virtual void on_tick() = 0;

protected:
   [[nodiscard]] websocket_listener() noexcept = default;
};

}
