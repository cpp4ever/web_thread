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

#include "webthread/constants.hpp" ///< for web::default_keep_alive_delay
#include "webthread/net_interface.hpp" ///< for web::net_interface
#include "webthread/peer_address.hpp" ///< for web::peer_address

#include <cassert> ///< for assert
#include <chrono> ///< for std::chrono::seconds
#include <optional> ///< for std::optional
#include <string_view> ///< for std::string_view

namespace web
{

class [[nodiscard("discarded-value expression detected")]] http_config final
{
public:
   http_config() = delete;
   [[maybe_unused, nodiscard]] http_config(http_config &&) noexcept = default;
   [[maybe_unused, nodiscard]] http_config(http_config const &) noexcept = default;

   [[maybe_unused, nodiscard]] constexpr explicit http_config(std::string_view const peerHost, uint16_t const peerPort = 0) noexcept :
      m_address{.host = peerHost, .port = peerPort}
   {
      assert(false == peer_address().host.empty());
   }

   http_config &operator = (http_config &&) = delete;
   http_config &operator = (http_config const &) = delete;

   [[maybe_unused, nodiscard]] constexpr std::chrono::seconds keep_alive_delay() const noexcept
   {
      return m_keepAliveDelay;
   }

   [[maybe_unused, nodiscard]] constexpr web::net_interface const &net_interface() const noexcept
   {
      return m_interface;
   }

   [[maybe_unused, nodiscard]] constexpr web::peer_address const &peer_address() const noexcept
   {
      return m_address;
   }

   [[maybe_unused]] constexpr http_config &with_interface(
      std::optional<std::string_view> const interfaceName = default_interface,
      std::optional<std::string_view> const interfaceHost = default_interface
   ) noexcept
   {
      assert((false == interfaceName.has_value()) || (false == interfaceName.value().empty()));
      assert((false == interfaceHost.has_value()) || (false == interfaceHost.value().empty()));
      m_interface.name = interfaceName;
      m_interface.host = interfaceHost;
      return *this;
   }

   [[maybe_unused]] constexpr http_config &with_keep_alive_delay(std::chrono::seconds const keepAliveDelay) noexcept
   {
      assert(std::chrono::seconds::zero() < keepAliveDelay);
      m_keepAliveDelay = keepAliveDelay;
      return *this;
   }

private:
   web::peer_address m_address;
   web::net_interface m_interface = {.name = default_interface, .host = default_interface};
   std::chrono::seconds m_keepAliveDelay = default_keep_alive_delay;
};

}
