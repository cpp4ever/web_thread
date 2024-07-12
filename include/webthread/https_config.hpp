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
#include "webthread/ssl_certificate.hpp" ///< for web::ssl_certificate, web::ssl_certificate_type

#include <cassert> ///< for assert
#include <chrono> ///< for std::chrono::seconds
#include <optional> ///< for std::nullopt, std::optional
#include <string_view> ///< for std::string_view

namespace web
{

class [[nodiscard("discarded-value expression detected")]] https_config final
{
public:
   https_config() = delete;
   [[maybe_unused, nodiscard]] https_config(https_config &&) noexcept = default;
   [[maybe_unused, nodiscard]] https_config(https_config const &) noexcept = default;

   [[maybe_unused, nodiscard]] constexpr explicit https_config(std::string_view const peerHost, uint16_t const peerPort = 0) noexcept :
      m_address{.host = peerHost, .port = peerPort}
   {
      assert(false == peer_address().host.empty());
   }

   https_config &operator = (https_config &&) = delete;
   https_config &operator = (https_config const &) = delete;

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

   [[maybe_unused, nodiscard]] constexpr std::optional<web::peer_address> const &peer_real_address() const noexcept
   {
      return m_realAddress;
   }

   [[maybe_unused, nodiscard]] constexpr std::optional<web::ssl_certificate> const &ssl_certificate() const noexcept
   {
      return m_sslCertificate;
   }

   [[maybe_unused]] constexpr https_config &with_certificate_authority(std::string_view const authorityInfo) noexcept
   {
      assert(false == authorityInfo.empty());
      m_sslCertificate = web::ssl_certificate{.authorityInfo = authorityInfo, .certificate = {}, .type = ssl_certificate_type::pem};
      return *this;
   }

   [[maybe_unused]] constexpr https_config &with_interface(
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

   [[maybe_unused]] constexpr https_config &with_keep_alive_delay(std::chrono::seconds const keepAliveDelay) noexcept
   {
      assert(std::chrono::seconds::zero() < keepAliveDelay);
      m_keepAliveDelay = keepAliveDelay;
      return *this;
   }

   [[maybe_unused]] constexpr https_config &with_peer_real_address(std::string_view const peerIp, uint16_t const peerPort = 0) noexcept
   {
      assert(false == peerIp.empty());
      m_realAddress = web::peer_address{.host = peerIp, .port = peerPort};
      return *this;
   }

   [[maybe_unused]] constexpr https_config &with_ssl_certificate(
      std::string_view const certificate,
      ssl_certificate_type const certificateType
   ) noexcept
   {
      assert(false == certificate.empty());
      m_sslCertificate = web::ssl_certificate{.authorityInfo = {}, .certificate = certificate, .type = certificateType};
      return *this;
   }

private:
   web::peer_address m_address;
   std::optional<web::peer_address> m_realAddress = std::nullopt;
   web::net_interface m_interface = {.name = default_interface, .host = default_interface};
   std::chrono::seconds m_keepAliveDelay = default_keep_alive_delay;
   std::optional<web::ssl_certificate> m_sslCertificate = std::nullopt;
};

}
