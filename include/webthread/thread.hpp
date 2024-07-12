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

#include "thread_config.hpp"

#include <cstddef> ///< for size_t
#include <cstdint> ///< for uint16_t
#include <memory> ///< for std::shared_ptr

namespace web
{

class [[nodiscard("discarded-value expression detected")]] thread final
{
private:
   class connection;
   class rest_connection;
   class websocket_connection;
   class worker;

public:
   class rest_client;
   class rest_request;
   class websocket;

public:
   thread() = delete;
   [[nodiscard]] thread(thread &&rhs) noexcept;
   [[nodiscard]] thread(thread const &rhs) noexcept;
   [[nodiscard]] explicit thread(thread_config config);
   ~thread();

   thread &operator = (thread &&) = delete;
   thread &operator = (thread const &) = delete;

private:
   std::shared_ptr<worker> m_worker;
};

}
