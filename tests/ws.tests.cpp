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

#include "test_common.hpp" ///< for test_bad_request_timeout, test_good_request_timeout, test_keep_alive_delay, test_loopback_interface, test_loopback_ip, test_peer_port
#include "test_websocket_client.hpp" ///< for test_websocket_client, test_websocket_client_traits

#include <webthread/net_interface.hpp> ///< for web::default_interface
#include <webthread/ws_config.hpp> ///< for web::ws_config

#include <boost/beast.hpp> ///< for boost::beast::tcp_stream
#include <boost/beast/websocket.hpp> ///< for boost::beast::websocket::stream
#include <gtest/gtest.h> ///< for TEST

#include <cstdint> ///< for uint16_t
#include <string_view> ///< for std::string_view

using test_websocket_stream = boost::beast::websocket::stream<boost::beast::tcp_stream>;

template<>
struct test_websocket_client_traits<test_websocket_stream>
{
   using test_config = web::ws_config;

   static test_config make_external_config(std::string_view const testPeerIp)
   {
      auto const testConfig = test_config{testPeerIp, test_peer_port}
         .with_keep_alive_delay(test_keep_alive_delay)
         .with_connect_timeout(test_bad_request_timeout)
      ;
      return testConfig;
   }

   static test_config make_loopback_config(uint16_t const testPeerPort)
   {
      auto const testConfig = test_config{test_loopback_ip, testPeerPort}
#if (defined(__APPLE__))
         .with_interface(test_loopback_interface, web::default_interface)
#elif (defined(__linux__))
         .with_interface(test_loopback_interface, test_loopback_ip)
#else
         .with_interface(web::default_interface, test_loopback_ip)
#endif
         .with_keep_alive_delay(test_keep_alive_delay)
         .with_connect_timeout(test_good_request_timeout)
         .with_url_path("/")
      ;
      return testConfig;
   }
};

TEST(Web, Ws)
{
   test_websocket_client<test_websocket_stream>();
}
