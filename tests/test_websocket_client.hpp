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

#include "test_common.hpp" ///< for test_good_request_timeout, test_loopback_ip, test_non_routable_ips
#include "test_websocket_listener.hpp" ///< for test_websocket_listener
#include "test_websocket_server.hpp" ///< for test_websocket_server

#include <webthread/thread.hpp> ///< for web::thread
#include <webthread/websocket.hpp> ///< for web::websocket

#include <boost/asio/ip/address.hpp> ///< for boost::asio::ip::make_address
#include <curl/curl.h> ///< for CURLE_COULDNT_RESOLVE_HOST, CURLE_GOT_NOTHING, CURLE_OPERATION_TIMEDOUT, CURLE_RECV_ERROR, CURLE_SEND_ERROR, CURLE_SSL_CONNECT_ERROR
#include <gmock/gmock.h> ///< for EXPECT_CALL, EXPECT_THAT, testing::AnyOf, testing::StrictMock, testing::_
#include <gtest/gtest.h> ///< for ASSERT_GT, EXPECT_EQ, EXPECT_GT, EXPECT_LE, EXPECT_TRUE

#include <atomic> ///< for std::atomic, std::memory_order_acquire, std::memory_order_release
#include <chrono> ///< for std::chrono::milliseconds, std::chrono::system_clock
#include <cstddef> ///< for size_t
#include <future> ///< for std::future_status, std::promise
#include <memory> ///< for std::make_shared

template<typename test_websocket_client_stream>
struct test_websocket_client_traits;

constexpr size_t test_max_expected_inbound_message_size = 64;

template<typename test_websocket_client_stream>
void test_websocket_client()
{
   using test_config = typename test_websocket_client_traits<test_websocket_client_stream>::test_config;
   constexpr size_t testConnectionsCapacity = 0;
   auto const testThread = web::thread
   {
      web::thread_config{}
         .with_cpu_affinity(test_cpu_id)
         .with_initial_connections_capacity(testConnectionsCapacity)
         .with_max_expected_inbound_message_size(test_max_expected_inbound_message_size)
   };
   for (auto testNonRoutableIp : test_non_routable_ips)
   {
      auto testWebSocketListener = std::make_shared<testing::StrictMock<test_websocket_listener>>();
      auto testPromise = std::promise<void>{};
      auto const testFuture = testPromise.get_future();
      EXPECT_CALL(*testWebSocketListener, on_error(testing::_, testing::_))
         .WillOnce(
            [testNonRoutableIp, &testPromise] (auto const testErrorCode, auto const testErrorDescription)
            {
               EXPECT_THAT(testErrorCode, testing::AnyOf(CURLE_COULDNT_RESOLVE_HOST, CURLE_OPERATION_TIMEDOUT))
                  << testNonRoutableIp
                  << ": " << testErrorDescription
               ;
               testPromise.set_value();
            }
         )
      ;
      auto const testConfig = test_config{test_websocket_client_traits<test_websocket_client_stream>::make_external_config(testNonRoutableIp)};
      web::websocket testWebSocket(testThread, testWebSocketListener, testConfig);
      EXPECT_EQ(std::future_status::ready, testFuture.wait_for(test_good_request_timeout)) << testNonRoutableIp;
   }
   auto const testAddress = boost::asio::ip::make_address(test_loopback_ip);
   testing::StrictMock<test_websocket_server<test_websocket_client_stream>> testWebSocketServer{testAddress};
   auto const testConfig = test_config{test_websocket_client_traits<test_websocket_client_stream>::make_loopback_config(testWebSocketServer.local_port())};
   auto const testErrorCodeMatcher = testing::AnyOf(
      CURLE_BAD_FUNCTION_ARGUMENT,
      CURLE_GOT_NOTHING,
      CURLE_RECV_ERROR,
      CURLE_SEND_ERROR,
      CURLE_SSL_CONNECT_ERROR
   );
   /// Disconnect on socket accept
   {
      EXPECT_CALL(testWebSocketServer, should_accept_socket()).WillOnce(testing::Return(false));
      auto testWebSocketistener = std::make_shared<testing::StrictMock<test_websocket_listener>>();
      auto testPromise = std::promise<void>{};
      auto const testFuture = testPromise.get_future();
      EXPECT_CALL(*testWebSocketistener, on_error(testing::_, testing::_))
         .WillOnce(
            [&testPromise, &testErrorCodeMatcher] (auto const testErrorCode, auto const testErrorDescription)
            {
               EXPECT_THAT(testErrorCode, testErrorCodeMatcher) << testErrorDescription;
               testPromise.set_value();
            }
         )
      ;
      auto testWebSocket = web::websocket{testThread, testWebSocketistener, testConfig};
      testWebSocket.write(R"raw({"test_request":"disconnect_on_socket_accept"})raw");
      EXPECT_EQ(std::future_status::ready, testFuture.wait_for(test_good_request_timeout));
   }
   /// Disconnect on handshake
   {
      EXPECT_CALL(testWebSocketServer, should_accept_socket()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_pass_handshake()).WillOnce(testing::Return(false));
      auto testWebSocketListener = std::make_shared<testing::StrictMock<test_websocket_listener>>();
      auto testPromise = std::promise<void>{};
      auto const testFuture = testPromise.get_future();
      EXPECT_CALL(*testWebSocketListener, on_error(testing::_, testing::_))
         .WillOnce(
            [&testPromise, &testErrorCodeMatcher] (auto const testErrorCode, auto const testErrorDescription)
            {
               EXPECT_THAT(testErrorCode, testErrorCodeMatcher) << testErrorDescription;
               testPromise.set_value();
            }
         )
      ;
      auto testWebSocket = web::websocket{testThread, testWebSocketListener, testConfig};
      testWebSocket.write(R"raw({"test_request":"disconnect_on_handshake"})raw");
      EXPECT_EQ(std::future_status::ready, testFuture.wait_for(test_good_request_timeout));
   }
   /// Disconnect on websocket accept
   {
      EXPECT_CALL(testWebSocketServer, should_accept_socket()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_pass_handshake()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_accept_websocket()).WillOnce(testing::Return(false));
      auto testWebSocketListener = std::make_shared<testing::StrictMock<test_websocket_listener>>();
      EXPECT_CALL(*testWebSocketListener, on_message_sent()).Times(testing::AtMost(1));
      auto testPromise = std::promise<void>{};
      auto const testFuture = testPromise.get_future();
      EXPECT_CALL(*testWebSocketListener, on_error(testing::_, testing::_))
         .WillOnce(
            [&testPromise, &testErrorCodeMatcher] (auto const testErrorCode, auto const testErrorDescription)
            {
               EXPECT_THAT(testErrorCode, testErrorCodeMatcher) << testErrorDescription;
               testPromise.set_value();
            }
         )
      ;
      auto testWebSocket = web::websocket{testThread, testWebSocketListener, testConfig};
      testWebSocket.write(R"raw({"test_request":"disconnect_on_websocket_accept"})raw");
      EXPECT_EQ(std::future_status::ready, testFuture.wait_for(test_good_request_timeout));
   }
   /// Disconnect on message
   {
      EXPECT_CALL(testWebSocketServer, should_accept_socket()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_pass_handshake()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_accept_websocket()).WillOnce(testing::Return(true));
      constexpr auto testRequest = std::string_view{R"raw({"test_request":"disconnect_on_message"})raw"};
      EXPECT_CALL(testWebSocketServer, handle_message(testing::_, testing::_))
         .WillOnce(
            [testRequest] (auto const &testInboundBuffer, auto &)
            {
               auto const testMessage = std::string_view
               {
                  static_cast<char const *>(testInboundBuffer.data().data()),
                  testInboundBuffer.data().size(),
               };
               EXPECT_EQ(testMessage, testRequest);
               return false;
            }
         )
      ;
      auto testWebSocketListener = std::make_shared<testing::StrictMock<test_websocket_listener>>();
      EXPECT_CALL(*testWebSocketListener, on_message_sent()).Times(testing::AtMost(1));
      auto testPromise = std::promise<void>{};
      auto const testFuture = testPromise.get_future();
      EXPECT_CALL(*testWebSocketListener, on_error(testing::_, testing::_))
         .WillOnce(
            [&testPromise, &testErrorCodeMatcher] (auto const testErrorCode, auto const testErrorDescription)
            {
               EXPECT_THAT(testErrorCode, testErrorCodeMatcher) << testErrorDescription;
               testPromise.set_value();
            }
         )
      ;
      auto testWebSocket = web::websocket{testThread, testWebSocketListener, testConfig};
      testWebSocket.write(testRequest);
      EXPECT_EQ(std::future_status::ready, testFuture.wait_for(test_good_request_timeout));
   }
   /// Do not keep alive
   {
      EXPECT_CALL(testWebSocketServer, should_accept_socket()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_pass_handshake()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_accept_websocket()).WillOnce(testing::Return(true));
      constexpr auto testRequest = std::string_view{R"raw({"test_request":"do_not_keep_alive"})raw"};
      constexpr auto testResponse = std::string_view{R"raw({"test_response":"do_not_keep_alive"})raw"};
      EXPECT_CALL(testWebSocketServer, handle_message(testing::_, testing::_))
         .WillOnce(
            [testRequest, testResponse] (auto const &testInboundBuffer, auto &testOutboundBuffer)
            {
               auto const testMessage = std::string
               {
                  static_cast<char const *>(testInboundBuffer.data().data()),
                  testInboundBuffer.data().size(),
               };
               EXPECT_EQ(testMessage, testRequest);

               testOutboundBuffer.append(testResponse);
               return true;
            }
         )
      ;
      EXPECT_CALL(testWebSocketServer, should_keep_alive()).WillOnce(testing::Return(false));
      auto testWebSocketListener = std::make_shared<testing::StrictMock<test_websocket_listener>>();
      EXPECT_CALL(*testWebSocketListener, on_message_sent()).Times(testing::AtLeast(1));
      auto testPromise = std::promise<void>{};
      auto const testFuture = testPromise.get_future();
      auto testWebSocket = web::websocket{testThread, testWebSocketListener, testConfig};
      EXPECT_CALL(*testWebSocketListener, on_message_recv(testing::_, testing::_, testing::_))
         .WillOnce(
            [
               &testWebSocketListener,
               &testPromise,
               testResponse,
               &testErrorCodeMatcher
            ] (auto const testMessage, auto const testMessageType, auto const testBytesLeft)
            {
               EXPECT_EQ(testMessage, testResponse);
               EXPECT_EQ(testMessageType, web::websocket_message_type::text);
               EXPECT_EQ(testBytesLeft, 0);

               EXPECT_CALL(*testWebSocketListener, on_error(testing::_, testing::_))
                  .WillOnce(
                     [&testPromise, &testErrorCodeMatcher] (auto const testErrorCode, auto const testErrorDescription)
                     {
                        EXPECT_THAT(testErrorCode, testErrorCodeMatcher) << testErrorDescription;
                        testPromise.set_value();
                     }
                  )
               ;
            }
         )
      ;
      testWebSocket.write(testRequest);
      EXPECT_EQ(std::future_status::ready, testFuture.wait_for(test_good_request_timeout));
   }
   /// Reconnect
   {
      EXPECT_CALL(testWebSocketServer, should_accept_socket()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_pass_handshake()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_accept_websocket()).WillOnce(testing::Return(true));
      constexpr auto testBeforeReconnectRequest = std::string_view{R"raw({"test_request":"before_reconnect"})raw"};
      constexpr auto testBeforeReconnectResponse = std::string_view{R"raw({"test_response":"before_reconnect"})raw"};
      EXPECT_CALL(testWebSocketServer, handle_message(testing::_, testing::_))
         .WillOnce(
            [testBeforeReconnectRequest, testBeforeReconnectResponse] (auto const &testInboundBuffer, auto &testOutboundBuffer)
            {
               auto const testMessage = std::string
               {
                  static_cast<char const *>(testInboundBuffer.data().data()),
                  testInboundBuffer.data().size(),
               };
               EXPECT_EQ(testMessage, testBeforeReconnectRequest);

               testOutboundBuffer.append(testBeforeReconnectResponse);
               return true;
            }
         )
      ;
      EXPECT_CALL(testWebSocketServer, should_keep_alive()).WillOnce(testing::Return(false));
      auto testWebSocketListener = std::make_shared<testing::StrictMock<test_websocket_listener>>();
      EXPECT_CALL(*testWebSocketListener, on_message_sent()).Times(1);
      auto testPromise = std::promise<void>{};
      auto const testFuture = testPromise.get_future();
      auto testWebSocket = web::websocket{testThread, testWebSocketListener, testConfig};
      constexpr auto testAfterReconnectRequest = std::string_view{R"raw({"test_request":"after_reconnect"})raw"};
      constexpr auto testAfterReconnectResponse = std::string_view{R"raw({"test_response":"after_reconnect"})raw"};
      EXPECT_CALL(*testWebSocketListener, on_message_recv(testing::_, testing::_, testing::_))
         .WillOnce(
            [
               &testWebSocketServer,
               &testWebSocketListener,
               &testPromise,
               &testWebSocket,
               testBeforeReconnectResponse,
               testAfterReconnectRequest,
               testAfterReconnectResponse,
               &testErrorCodeMatcher
            ] (auto const testMessage, auto const testMessageType, auto const testBytesLeft)
            {
               EXPECT_EQ(testMessage, testBeforeReconnectResponse);
               EXPECT_EQ(testMessageType, web::websocket_message_type::text);
               EXPECT_EQ(testBytesLeft, 0);

               EXPECT_CALL(*testWebSocketListener, on_error(testing::_, testing::_))
                  .WillOnce(
                     [
                        &testWebSocketServer,
                        &testWebSocketListener,
                        &testPromise,
                        &testWebSocket,
                        testAfterReconnectRequest,
                        testAfterReconnectResponse,
                        &testErrorCodeMatcher
                     ] (auto const testErrorCode, auto const testErrorDescription)
                     {
                        EXPECT_THAT(testErrorCode, testErrorCodeMatcher) << testErrorDescription;

                        EXPECT_CALL(testWebSocketServer, should_accept_socket()).WillOnce(testing::Return(true));
                        EXPECT_CALL(testWebSocketServer, should_pass_handshake()).WillOnce(testing::Return(true));
                        EXPECT_CALL(testWebSocketServer, should_accept_websocket()).WillOnce(testing::Return(true));
                        EXPECT_CALL(testWebSocketServer, handle_message(testing::_, testing::_))
                           .WillOnce(
                              [testAfterReconnectRequest, testAfterReconnectResponse] (auto const &testInboundBuffer, auto &testOutboundBuffer)
                              {
                                 auto const testMessage = std::string
                                 {
                                    static_cast<char const *>(testInboundBuffer.data().data()),
                                    testInboundBuffer.data().size(),
                                 };
                                 EXPECT_EQ(testMessage, testAfterReconnectRequest);

                                 testOutboundBuffer.append(testAfterReconnectResponse);
                                 return true;
                              }
                           )
                        ;
                        EXPECT_CALL(testWebSocketServer, should_keep_alive()).WillOnce(testing::Return(true));
                        EXPECT_CALL(*testWebSocketListener, on_message_sent()).Times(1);
                        EXPECT_CALL(*testWebSocketListener, on_message_recv(testing::_, testing::_, testing::_))
                           .WillOnce(
                              [&testPromise, testAfterReconnectResponse] (auto const testMessage, auto const testMessageType, auto const testBytesLeft)
                              {
                                 EXPECT_EQ(testMessage, testAfterReconnectResponse);
                                 EXPECT_EQ(testMessageType, web::websocket_message_type::text);
                                 EXPECT_EQ(testBytesLeft, 0);

                                 testPromise.set_value();
                              }
                           )
                        ;
                        testWebSocket.reconnect();
                        testWebSocket.write(testAfterReconnectRequest);
                     }
                  )
               ;
            }
         )
      ;
      testWebSocket.write(testBeforeReconnectRequest);
      EXPECT_EQ(std::future_status::ready, testFuture.wait_for(test_good_request_timeout));
   }
   /// Tick
   {
      EXPECT_CALL(testWebSocketServer, should_accept_socket()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_pass_handshake()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_accept_websocket()).WillOnce(testing::Return(true));
      auto testWebSocketListener = std::make_shared<testing::StrictMock<test_websocket_listener>>();
      auto testPromise = std::promise<void>{};
      auto const testFuture = testPromise.get_future();
      constexpr auto testTickTimeout = std::chrono::milliseconds{100};
      std::atomic<web::websocket *> testWebSocketPtr;
      constexpr auto testPingRequest = std::string_view{R"raw({"test_request":"ping"})raw"};
      constexpr auto testPongResponse = std::string_view{R"raw({"test_response":"pong"})raw"};
      auto const testExpirationTime = std::chrono::system_clock::now() + testTickTimeout;
      EXPECT_CALL(*testWebSocketListener, on_tick())
         .WillOnce(
            [
               &testWebSocketServer,
               &testWebSocketListener,
               &testPromise,
               &testWebSocketPtr,
               testPingRequest,
               testPongResponse,
               testExpirationTime,
               testTickTimeout
            ] ()
            {
               EXPECT_LE(testExpirationTime, std::chrono::system_clock::now());
               EXPECT_GT(testExpirationTime + testTickTimeout, std::chrono::system_clock::now());

               EXPECT_CALL(testWebSocketServer, handle_message(testing::_, testing::_))
                  .WillOnce(
                     [testPingRequest, testPongResponse] (auto const &testInboundBuffer, auto &testOutboundBuffer)
                     {
                        auto const testMessage = std::string
                        {
                           static_cast<char const *>(testInboundBuffer.data().data()),
                           testInboundBuffer.data().size(),
                        };
                        EXPECT_EQ(testMessage, testPingRequest);

                        testOutboundBuffer.append(testPongResponse);
                        return true;
                     }
                  )
               ;
               EXPECT_CALL(testWebSocketServer, should_keep_alive()).WillOnce(testing::Return(true));
               EXPECT_CALL(*testWebSocketListener, on_message_sent()).Times(1);
               EXPECT_CALL(*testWebSocketListener, on_message_recv(testing::_, testing::_, testing::_))
                  .WillOnce(
                     [&testPromise, testPongResponse] (auto const testMessage, auto const testMessageType, auto const testBytesLeft)
                     {
                        EXPECT_EQ(testMessage, testPongResponse);
                        EXPECT_EQ(testMessageType, web::websocket_message_type::text);
                        EXPECT_EQ(testBytesLeft, 0);

                        testPromise.set_value();
                     }
                  )
               ;
               testWebSocketPtr.load(std::memory_order_acquire)->write(testPingRequest);
            }
         )
      ;
      auto testWebSocket = web::websocket
      {
         testThread,
         testWebSocketListener,
         test_config{testConfig}.with_tick_timeout(testTickTimeout)
      };
      testWebSocketPtr.store(std::addressof(testWebSocket), std::memory_order_release);
      EXPECT_EQ(std::future_status::ready, testFuture.wait_for(test_good_request_timeout));
   }
   /// Multipart response
   {
      constexpr auto testRequest = std::string_view{R"raw({"test_request":"multipart_response"})raw"};
      constexpr auto testResponse = std::string_view{R"raw({"test_response":"The message must be longer then {test_max_expected_inbound_message_size} bytes"})raw"};
      ASSERT_GT(testResponse.size(), test_max_expected_inbound_message_size);
      EXPECT_CALL(testWebSocketServer, should_accept_socket()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_pass_handshake()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, should_accept_websocket()).WillOnce(testing::Return(true));
      EXPECT_CALL(testWebSocketServer, handle_message(testing::_, testing::_))
         .WillOnce(
            [testRequest, testResponse] (auto const &testInboundBuffer, auto &testOutboundBuffer)
            {
               auto const testMessage = std::string
               {
                  static_cast<char const *>(testInboundBuffer.data().data()),
                  testInboundBuffer.data().size(),
               };
               EXPECT_EQ(testMessage, testRequest);

               testOutboundBuffer.append(testResponse);
               return true;
            }
         )
      ;
      EXPECT_CALL(testWebSocketServer, should_keep_alive()).WillOnce(testing::Return(true));
      auto testWebSocketListener = std::make_shared<testing::StrictMock<test_websocket_listener>>();
      EXPECT_CALL(*testWebSocketListener, on_message_sent()).Times(1);
      auto testPromise = std::promise<void>{};
      auto const testFuture = testPromise.get_future();
      EXPECT_CALL(*testWebSocketListener, on_message_recv(testing::_, testing::_, testing::_))
         .WillOnce(
            [
               &testWebSocketListener,
               &testPromise,
               testResponse
            ] (auto const testMessage, auto const testMessageType, auto const testBytesLeft)
            {
               EXPECT_EQ(testMessage, testResponse.substr(0, test_max_expected_inbound_message_size));
               EXPECT_EQ(testMessageType, web::websocket_message_type::text);
               EXPECT_EQ(testBytesLeft, testResponse.size() - test_max_expected_inbound_message_size);

               EXPECT_CALL(*testWebSocketListener, on_message_recv(testing::_, testing::_, testing::_))
                  .WillOnce(
                     [
                        &testPromise,
                        testResponse
                     ] (auto const testMessage, auto const testMessageType, auto const testBytesLeft)
                     {
                        EXPECT_EQ(testMessage, testResponse.substr(test_max_expected_inbound_message_size));
                        EXPECT_EQ(testMessageType, web::websocket_message_type::text);
                        EXPECT_EQ(testBytesLeft, 0);

                        testPromise.set_value();
                     }
                  )
               ;
            }
         )
      ;
      auto testWebSocket = web::websocket{testThread, testWebSocketListener, testConfig};
      testWebSocket.write(testRequest);
      EXPECT_EQ(std::future_status::ready, testFuture.wait_for(test_good_request_timeout));
   }
}
