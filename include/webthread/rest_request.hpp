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

#include "webthread/rest_method.hpp" ///< for web::rest_method
#include "webthread/thread.hpp" ///< for web::thread, web::thread::rest_connection, web::thread::worker

#include <chrono> ///< for std::chrono::milliseconds
#include <memory> ///< for std::shared_ptr
#include <string_view> ///< for std::string_view

namespace web
{

class [[nodiscard("discarded-value expression detected")]] thread::rest_request final
{
public:
   rest_request() = delete;
   [[nodiscard]] rest_request(rest_request &&rhs) noexcept;
   rest_request(rest_request const &) = delete;
   [[nodiscard]] rest_request(
      std::shared_ptr<worker> const &worker,
      std::shared_ptr<rest_connection> const &connection,
      rest_method method,
      std::string_view customMethod
   ) noexcept;
   ~rest_request() noexcept;

   rest_request &operator = (rest_request &&) = delete;
   rest_request &operator = (rest_request const &) = delete;

   void perform() noexcept;

   rest_request &with_body(std::string_view body, std::string_view contentType);
   rest_request &with_header(std::string_view header);
   rest_request &with_header(std::string_view headerName, std::string_view headerValue);
   rest_request &with_path(std::string_view path) noexcept;
   rest_request &with_query(std::string_view query) noexcept;
   rest_request &with_timeout(std::chrono::milliseconds timeout) noexcept;

private:
   std::shared_ptr<rest_connection> m_connection;
   std::shared_ptr<worker> m_worker;
};

using rest_request [[maybe_unused]] = thread::rest_request;

}
