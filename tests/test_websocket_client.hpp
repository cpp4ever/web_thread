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

#include "test_common.hpp" ///< for test_bad_request_timeout, test_loopback_ip, test_non_routable_ips, test_wait
#include "test_websocket_listener.hpp" ///< for test_websocket_listener
#include "test_websocket_server.hpp" ///< for test_websocket_server

#include <webthread/thread.hpp> ///< for web::thread
#include <webthread/websocket.hpp> ///< for web::websocket

#include <boost/asio/ip/address.hpp> ///< for boost::asio::ip::make_address
#include <curl/curl.h> ///< for CURLE_COULDNT_RESOLVE_HOST, CURLE_GOT_NOTHING, CURLE_OPERATION_TIMEDOUT, CURLE_RECV_ERROR, CURLE_SEND_ERROR, CURLE_SSL_CONNECT_ERROR
#include <gmock/gmock.h> ///< for EXPECT_CALL, EXPECT_THAT, testing::AnyOf, testing::StrictMock, testing::_
#include <gtest/gtest.h> ///< for EXPECT_TRUE

#include <atomic> ///< for std::atomic_bool, std::memory_order_release
#include <cstddef> ///< for size_t
#include <memory> ///< for std::make_shared

template<typename test_websocket_client_stream>
struct test_websocket_client_traits;

template<typename test_websocket_client_stream>
void test_websocket_client()
{
   using test_config = typename test_websocket_client_traits<test_websocket_client_stream>::test_config;
   constexpr size_t testConnectionsCapacity = 0;
   constexpr size_t testMaxExpectedInboundMessageSize = 25;
   auto const testThread = web::thread
   {
      web::thread_config{}
         .with_cpu_affinity(test_cpu_id)
         .with_initial_connections_capacity(testConnectionsCapacity)
         .with_max_expected_inbound_message_size(testMaxExpectedInboundMessageSize)
   };
   for (auto testNonRoutableIp : test_non_routable_ips)
   {
      std::atomic_bool testLock{false};
      auto testWebSocketListener = std::make_shared<testing::StrictMock<test_websocket_listener>>();
      EXPECT_CALL(*testWebSocketListener, on_error(testing::_, testing::_))
         .WillOnce(
            [&] (auto const testErrorCode, auto const testErrorDescription)
            {
               EXPECT_THAT(testErrorCode, testing::AnyOf(CURLE_COULDNT_RESOLVE_HOST, CURLE_OPERATION_TIMEDOUT))
                  << testNonRoutableIp
                  << ": " << testErrorDescription
               ;
               testLock.store(true, std::memory_order_release);
            }
         )
      ;
      auto const testConfig = test_config{test_websocket_client_traits<test_websocket_client_stream>::make_external_config(testNonRoutableIp)};
      web::websocket testWebSocket(testThread, testWebSocketListener, testConfig);
      EXPECT_TRUE(test_wait(testLock, test_bad_request_timeout)) << testNonRoutableIp;
   }
   auto const testAddress = boost::asio::ip::make_address(test_loopback_ip);
   testing::StrictMock<test_websocket_server<test_websocket_client_stream>> testWebSocketServer{testAddress};
   auto const testConfig = test_config{test_websocket_client_traits<test_websocket_client_stream>::make_loopback_config(testWebSocketServer.local_port())};
   auto const testErrorCodeMatcher = testing::AnyOf(CURLE_GOT_NOTHING, CURLE_RECV_ERROR, CURLE_SEND_ERROR, CURLE_SSL_CONNECT_ERROR);
   /// Disconnect on socket accept
   {
      EXPECT_CALL(testWebSocketServer, should_accept_socket()).WillOnce(testing::Return(false));
      auto testLock = std::atomic_bool{false};
      auto testWebSocketistener = std::make_shared<testing::StrictMock<test_websocket_listener>>();
      EXPECT_CALL(*testWebSocketistener, on_error(testing::_, testing::_))
         .WillOnce(
            [&] (auto const testErrorCode, auto const testErrorDescription)
            {
               EXPECT_THAT(testErrorCode, testing::AnyOf(CURLE_BAD_FUNCTION_ARGUMENT, CURLE_GOT_NOTHING, CURLE_RECV_ERROR, CURLE_SEND_ERROR, CURLE_SSL_CONNECT_ERROR)) << testErrorDescription;
               testLock.store(true, std::memory_order_release);
            }
         )
      ;
      auto testWebSocket = web::websocket{testThread, testWebSocketistener, testConfig};
      testWebSocket.write("test");
      EXPECT_TRUE(test_wait(testLock, test_good_request_timeout));
   }
   /// Disconnect on handshake
   {
      EXPECT_CALL(testWebSocketServer, should_accept_socket()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_pass_handshake()).WillOnce(testing::Return(false));
      auto testLock = std::atomic_bool{false};
      auto testWebSocketListener = std::make_shared<testing::StrictMock<test_websocket_listener>>();
      EXPECT_CALL(*testWebSocketListener, on_error(testing::_, testing::_))
         .WillOnce(
            [&] (auto const testErrorCode, auto const testErrorDescription)
            {
               EXPECT_THAT(testErrorCode, testErrorCodeMatcher) << testErrorDescription;
               testLock.store(true, std::memory_order_release);
            }
         )
      ;
      auto testWebSocket = web::websocket{testThread, testWebSocketListener, testConfig};
      testWebSocket.write("test");
      EXPECT_TRUE(test_wait(testLock, test_good_request_timeout));
   }
   /// Disconnect on websocket accept
   {
      EXPECT_CALL(testWebSocketServer, should_accept_socket()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_pass_handshake()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_accept_websocket()).WillOnce(testing::Return(false));
      auto testLock = std::atomic_bool{false};
      auto testWebSocketListener = std::make_shared<testing::StrictMock<test_websocket_listener>>();
      EXPECT_CALL(*testWebSocketListener, on_message_sent()).Times(testing::AtMost(1));
      EXPECT_CALL(*testWebSocketListener, on_error(testing::_, testing::_))
         .WillOnce(
            [&] (auto const testErrorCode, auto const testErrorDescription)
            {
               EXPECT_THAT(testErrorCode, testErrorCodeMatcher) << testErrorDescription;
               testLock.store(true, std::memory_order_release);
            }
         )
      ;
      auto testWebSocket = web::websocket{testThread, testWebSocketListener, testConfig};
      testWebSocket.write("test");
      EXPECT_TRUE(test_wait(testLock, test_good_request_timeout));
   }
   /// Disconnect on message
   {
      EXPECT_CALL(testWebSocketServer, should_accept_socket()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_pass_handshake()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_accept_websocket()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, handle_message(testing::_, testing::_))
         .WillOnce(
            [&] (auto const &testInboundBuffer, auto &)
            {
               auto const testMessage = std::string_view
               {
                  static_cast<char const *>(testInboundBuffer.data().data()),
                  testInboundBuffer.data().size(),
               };
               EXPECT_EQ(testMessage, "test");
               return false;
            }
         )
      ;
      auto testLock = std::atomic_bool{false};
      auto testWebSocketListener = std::make_shared<testing::StrictMock<test_websocket_listener>>();
      auto testWebSocket = web::websocket{testThread, testWebSocketListener, testConfig};
      EXPECT_CALL(*testWebSocketListener, on_message_sent()).Times(testing::AtMost(1));
      EXPECT_CALL(*testWebSocketListener, on_error(testing::_, testing::_))
         .WillOnce(
            [&] (auto const testErrorCode, auto const testErrorDescription)
            {
               EXPECT_THAT(testErrorCode, testErrorCodeMatcher) << testErrorDescription;
               testLock.store(true, std::memory_order_release);
            }
         )
      ;
      testWebSocket.write("test");
      EXPECT_TRUE(test_wait(testLock, test_good_request_timeout));
   }
   /// Do not keep alive
   {
      constexpr auto testGoodRequest = std::string_view{R"raw({"op":"good_request"})raw"};
      constexpr auto testGoodResponse = std::string_view{R"raw({"op":"good_response"})raw"};
      constexpr auto testBadRequest = std::string_view{R"raw({"op":"bad_request"})raw"};
      EXPECT_CALL(testWebSocketServer, should_accept_socket()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_pass_handshake()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_accept_websocket()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, handle_message(testing::_, testing::_))
         .WillOnce(
            [&] (auto const &testInboundBuffer, auto &testOutboundBuffer)
            {
               auto const testMessage = std::string
               {
                  static_cast<char const *>(testInboundBuffer.data().data()),
                  testInboundBuffer.data().size(),
               };
               EXPECT_EQ(testMessage, testGoodRequest);

               testOutboundBuffer.append(testGoodResponse);
               return true;
            }
         )
      ;
      EXPECT_CALL(testWebSocketServer, should_keep_alive()).WillOnce(testing::Return(false));
      auto testLock = std::atomic_bool{false};
      auto testWebSocketListener = std::make_shared<testing::StrictMock<test_websocket_listener>>();
      auto testWebSocket = web::websocket{testThread, testWebSocketListener, testConfig};
      EXPECT_CALL(*testWebSocketListener, on_message_sent()).Times(testing::AtLeast(1));
      EXPECT_CALL(*testWebSocketListener, on_message_recv(testing::_, testing::_, testing::_))
         .WillOnce(
            [&] (auto const testMessage, auto const testMessageType, auto const testBytesLeft)
            {
               EXPECT_EQ(testMessage, testGoodResponse);
               EXPECT_EQ(testMessageType, web::websocket_message_type::text);
               EXPECT_EQ(testBytesLeft, 0);

               EXPECT_CALL(*testWebSocketListener, on_error(testing::_, testing::_))
                  .WillOnce(
                     [&] (auto const testErrorCode, auto const testErrorDescription)
                     {
                        EXPECT_THAT(testErrorCode, testErrorCodeMatcher) << testErrorDescription;
                        testLock.store(true, std::memory_order_release);
                     }
                  )
               ;
               testWebSocket.write(testBadRequest);
            }
         )
      ;
      testWebSocket.write(testGoodRequest);
      EXPECT_TRUE(test_wait(testLock, test_good_request_timeout));
   }
}
