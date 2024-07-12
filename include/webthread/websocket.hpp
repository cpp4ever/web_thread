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

#include "webthread/thread.hpp" ///< for web::thread, web::thread::connection, web::thread::worker
#include "webthread/websocket_listener.hpp" ///< for web::websocket_listener
#include "webthread/ws_config.hpp" ///< for web::ws_config
#include "webthread/wss_config.hpp" ///< for web::wss_config

#include <memory> ///< for std::shared_ptr
#include <string_view> ///< for std::string_view

namespace web
{

class [[nodiscard("discarded-value expression detected")]] thread::websocket final
{
public:
   [[nodiscard]] websocket() noexcept;
   [[nodiscard]] websocket(websocket &&rhs) noexcept;
   websocket(websocket const &) = delete;
   [[nodiscard]] websocket(
      thread const &thread,
      std::shared_ptr<websocket_listener> const &listener,
      ws_config const &config
   );
   [[nodiscard]] websocket(
      thread const &thread,
      std::shared_ptr<websocket_listener> const &listener,
      wss_config const &config
   );
   ~websocket() noexcept;

   websocket &operator = (websocket &&rhs) noexcept;
   websocket &operator = (websocket const &) = delete;

   void reconnect();

   void write(std::string_view message);

private:
   std::shared_ptr<worker> m_worker;
   std::shared_ptr<websocket_connection> m_connection;
};

using websocket = thread::websocket;

}
