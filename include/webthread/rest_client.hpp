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

#include "webthread/http_config.hpp" ///< for web::http_config
#include "webthread/https_config.hpp" ///< for web::https_config
#include "webthread/rest_client_listener.hpp" ///< for web::rest_client_listener
#include "webthread/rest_request.hpp" ///< for web::rest_request
#include "webthread/thread.hpp" ///< for web::thread, web::thread::rest_connection, web::thread::worker

#include <memory> ///< for std::shared_ptr
#include <string_view> ///< for std::string_view

namespace web
{

class [[nodiscard("discarded-value expression detected")]] thread::rest_client final
{
public:
   [[nodiscard]] rest_client() noexcept;
   [[nodiscard]] rest_client(rest_client &&rhs) noexcept;
   rest_client(rest_client const &) = delete;
   [[nodiscard]] rest_client(
      thread const &thread,
      std::shared_ptr<rest_client_listener> const &listener,
      http_config const &httpConfig
   );
   [[nodiscard]] rest_client(
      thread const &thread,
      std::shared_ptr<rest_client_listener> const &listener,
      https_config const &httpConfig
   );
   ~rest_client() noexcept;

   rest_client &operator = (rest_client &&rhs) noexcept;
   rest_client &operator = (rest_client const &) = delete;

   [[nodiscard]] rest_request custom_request(std::string_view method) const noexcept;
   [[nodiscard]] rest_request get_request() const noexcept;
   [[nodiscard]] rest_request post_request() const noexcept;
   [[nodiscard]] rest_request upload_request() const noexcept;

private:
   std::shared_ptr<worker> m_worker;
   std::shared_ptr<rest_connection> m_connection;
};

using rest_client [[maybe_unused]] = thread::rest_client;

}
