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

#include "test_certificate.hpp" ///< for test_webthread_certificate_der, test_webthread_certificate_p12, test_webthread_certificate_pem, test_webthread_domain
#include "test_common.hpp" ///< for test_keep_alive_delay, test_loopback_interface, test_loopback_ip, test_peer_ssl_port
#include "test_rest_client.hpp" ///< for test_rest_client, test_rest_client_traits
#include "test_server_context.hpp" ///< for test_tls_stream

#include <webthread/https_config.hpp> ///< for web::https_config
#include <webthread/net_interface.hpp> ///< for web::default_interface
#include <webthread/ssl_certificate.hpp> ///< for web::ssl_certificate_type

#include <gtest/gtest.h> ///< for TEST

#include <cstdint> ///< for uint16_t
#include <string_view> ///< for std::string_view

using test_rest_stream = test_tls_stream;

template<>
struct test_rest_client_traits<test_rest_stream>
{
   using test_config = web::https_config;

   static test_config make_external_config(std::string_view const testPeerIp)
   {
      auto const testConfig = test_config{test_webthread_domain, test_peer_ssl_port}
         .with_keep_alive_delay(test_keep_alive_delay)
         .with_peer_real_address(testPeerIp, test_peer_ssl_port)
#if (defined(__APPLE__))
         .with_ssl_certificate(test_webthread_certificate_der(), web::ssl_certificate_type::der)
#elif (defined(WIN32))
         .with_ssl_certificate(test_webthread_certificate_p12(), web::ssl_certificate_type::p12)
#else
         .with_ssl_certificate(test_webthread_certificate_pem(), web::ssl_certificate_type::pem)
#endif
      ;
      return testConfig;
   }

   static test_config make_loopback_config(uint16_t const testPeerPort)
   {
      auto const testConfig = test_config{test_webthread_domain, testPeerPort}
         .with_certificate_authority(test_webthread_certificate_pem())
#if (defined(__APPLE__))
         .with_interface(test_loopback_interface, web::default_interface)
#elif (defined(__linux__))
         .with_interface(test_loopback_interface, test_loopback_ip)
#else
         .with_interface(web::default_interface, test_loopback_ip)
#endif
         .with_keep_alive_delay(test_keep_alive_delay)
         .with_peer_real_address(test_loopback_ip, testPeerPort)
      ;
      return testConfig;
   }
};

TEST(Web, Https)
{
   test_rest_client<test_rest_stream>();
}
