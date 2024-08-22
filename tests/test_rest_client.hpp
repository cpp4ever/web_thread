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

#include "test_common.hpp" ///< for test_bad_request_timeout, test_good_request_timeout, test_loopback_ip, test_non_routable_ips
#include "test_rest_client_listener.hpp" ///< for EXPECT_HTTP_RESPONSE, test_rest_client_listener
#include "test_rest_server.hpp" ///< for EXPECT_HTTP_REQUEST, test_rest_server

#include <webthread/rest_client.hpp> ///< for web::rest_client
#include <webthread/rest_request.hpp> ///< for web::rest_request
#include <webthread/thread.hpp> ///< for web::thread
#include <webthread/thread_config.hpp> ///< for web::thread_config

#include <boost/asio/ip/address.hpp> ///< for boost::asio::ip::make_address
#include <boost/beast/http/field.hpp> ///< for boost::beast::http::field
#include <boost/beast/http/status.hpp> /// boost::beast::http::status
#include <boost/beast/http/verb.hpp> ///< for boost::beast::http::verb
#include <curl/curl.h> ///< for CURLE_COULDNT_RESOLVE_HOST, CURLE_GOT_NOTHING, CURLE_OPERATION_TIMEDOUT, CURLE_RECV_ERROR, CURLE_SSL_CONNECT_ERROR
#include <gmock/gmock.h> ///< for EXPECT_CALL, EXPECT_THAT, testing::AnyOf, testing::Return, testing::StrictMock, testing::_
#include <gtest/gtest.h> ///< for EXPECT_TRUE

#include <cstddef> ///< for size_t
#include <future> ///< for std::future_status, std::promise
#include <map> ///< for std::map
#include <memory> ///< for std::make_shared
#include <string> ///< for std::string, std::to_string
#include <string_view> ///< for std::string_view

template<typename test_rest_stream>
struct test_rest_client_traits;

template<typename test_rest_stream>
void test_rest_client()
{
   using test_config = typename test_rest_client_traits<test_rest_stream>::test_config;
   constexpr size_t testConnectionsCapacity = 0;
   constexpr size_t testMaxExpectedInboundMessageSize = 25;
   auto const testThread = web::thread
   {
      web::thread_config{}
         .with_cpu_affinity(test_cpu_id)
         .with_initial_connections_capacity(testConnectionsCapacity)
         .with_max_expected_inbound_message_size(testMaxExpectedInboundMessageSize)
   };
   /// Connect timeout
   for (auto const testNonRoutableIp : test_non_routable_ips)
   {
      auto testRestClientListener = std::make_shared<testing::StrictMock<test_rest_client_listener>>();
      auto testPromise = std::promise<void>{};
      auto const testFuture = testPromise.get_future();
      EXPECT_CALL(*testRestClientListener, on_error(testing::_, testing::_))
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
      auto const testConfig = test_config{test_rest_client_traits<test_rest_stream>::make_external_config(testNonRoutableIp)};
      auto testRestClient = web::rest_client{testThread, testRestClientListener, testConfig};
      auto testRequest = web::rest_request{testRestClient.get_request()};
      testRequest
         .with_timeout(test_bad_request_timeout)
         .perform()
      ;
      EXPECT_EQ(std::future_status::ready, testFuture.wait_for(test_good_request_timeout)) << testNonRoutableIp;
   }
   auto const testAddress = boost::asio::ip::make_address(test_loopback_ip);
   testing::StrictMock<test_rest_server<test_rest_stream>> testRestServer{testAddress};
   auto const testConfig = test_config{test_rest_client_traits<test_rest_stream>::make_loopback_config(testRestServer.local_port())};
   auto const testPeerHost = std::string{testConfig.peer_address().host}.append(":").append(std::to_string(testConfig.peer_address().port));
   auto const testContentType = std::string{"application/json"};
   auto const testErrorCodeMatcher = testing::AnyOf(CURLE_GOT_NOTHING, CURLE_RECV_ERROR, CURLE_SSL_CONNECT_ERROR);
   /// Disconnect on socket accept
   {
      EXPECT_CALL(testRestServer, should_accept_socket()).WillOnce(testing::Return(false));
      auto testRestClientListener = std::make_shared<testing::StrictMock<test_rest_client_listener>>();
      auto testPromise = std::promise<void>{};
      auto const testFuture = testPromise.get_future();
      EXPECT_CALL(*testRestClientListener, on_error(testing::_, testing::_))
         .WillOnce(
            [&testPromise, &testErrorCodeMatcher] (auto const testErrorCode, auto const testErrorDescription)
            {
               EXPECT_THAT(testErrorCode, testErrorCodeMatcher) << testErrorDescription;
               testPromise.set_value();
            }
         )
      ;
      auto testRestClient = web::rest_client{testThread, testRestClientListener, testConfig};
      auto testRequest = web::rest_request{testRestClient.get_request()};
      testRequest
         .with_timeout(test_good_request_timeout)
         .perform()
      ;
      EXPECT_EQ(std::future_status::ready, testFuture.wait_for(test_good_request_timeout));
   }
   /// Disconnect on handshake
   {
      EXPECT_CALL(testRestServer, should_accept_socket()).WillOnce(testing::Return(true));
      EXPECT_CALL(testRestServer, should_pass_handshake()).WillOnce(testing::Return(false));
      auto testRestClientListener = std::make_shared<testing::StrictMock<test_rest_client_listener>>();
      auto testPromise = std::promise<void>{};
      auto const testFuture = testPromise.get_future();
      EXPECT_CALL(*testRestClientListener, on_error(testing::_, testing::_))
         .WillOnce(
            [&testPromise, &testErrorCodeMatcher] (auto const testErrorCode, auto const testErrorDescription)
            {
               EXPECT_THAT(testErrorCode, testErrorCodeMatcher) << testErrorDescription;
               testPromise.set_value();
            }
         )
      ;
      auto testRestClient = web::rest_client{testThread, testRestClientListener, testConfig};
      auto testRequest = web::rest_request{testRestClient.get_request()};
      testRequest
         .with_timeout(test_good_request_timeout)
         .perform()
      ;
      EXPECT_EQ(std::future_status::ready, testFuture.wait_for(test_good_request_timeout));
   }
   /// Disconnect on request
   {
      EXPECT_CALL(testRestServer, should_accept_socket()).WillOnce(testing::Return(true));
      EXPECT_CALL(testRestServer, should_pass_handshake()).WillOnce(testing::Return(true));
      EXPECT_CALL(testRestServer, handle_request(testing::_, testing::_))
         .WillOnce(
            [&] (auto const &testRequest, auto &)
            {
               auto testHeaders = std::map<std::string_view, std::string_view>
               {
                  {"Accept", "*/*"},
                  {"Host", testPeerHost},
               };
               EXPECT_HTTP_REQUEST(testRequest, boost::beast::http::verb::get, "/", testHeaders, "");
               return false;
            }
         )
      ;
      auto testRestClientListener = std::make_shared<testing::StrictMock<test_rest_client_listener>>();
      auto testPromise = std::promise<void>{};
      auto const testFuture = testPromise.get_future();
      EXPECT_CALL(*testRestClientListener, on_error(testing::_, testing::_))
         .WillOnce(
            [&testPromise, &testErrorCodeMatcher] (auto const testErrorCode, auto const testErrorDescription)
            {
               EXPECT_THAT(testErrorCode, testErrorCodeMatcher) << testErrorDescription;
               testPromise.set_value();
            }
         )
      ;
      auto testRestClient = web::rest_client{testThread, testRestClientListener, testConfig};
      auto testRequest = web::rest_request{testRestClient.get_request()};
      testRequest
         .with_timeout(test_good_request_timeout)
         .perform()
      ;
      EXPECT_EQ(std::future_status::ready, testFuture.wait_for(test_good_request_timeout));
   }
   /// Do not keep alive
   {
      constexpr auto testResponseBody = std::string_view{R"raw({"test":"response"})raw"};
      EXPECT_CALL(testRestServer, should_accept_socket()).WillOnce(testing::Return(true));
      EXPECT_CALL(testRestServer, should_pass_handshake()).WillOnce(testing::Return(true));
      EXPECT_CALL(testRestServer, handle_request(testing::_, testing::_))
         .WillOnce(
            [&] (auto const &testRequest, auto &testResponse)
            {
               auto testHeaders = std::map<std::string_view, std::string_view>
               {
                  {"Accept", "*/*"},
                  {"Host", testPeerHost},
               };
               EXPECT_HTTP_REQUEST(testRequest, boost::beast::http::verb::get, "/", testHeaders, "");

               testResponse.clear();
               testResponse.result(boost::beast::http::status::ok);
               testResponse.version(testRequest.version());
               testResponse.keep_alive(true);
               testResponse.set(boost::beast::http::field::content_type, testContentType);
               testResponse.body() = testResponseBody;
               testResponse.prepare_payload();
               testResponse.content_length(testResponse.body().size());
               return true;
            }
         )
      ;
      EXPECT_CALL(testRestServer, should_keep_alive()).WillOnce(testing::Return(false));
      auto testRestClientListener = std::make_shared<testing::StrictMock<test_rest_client_listener>>();
      EXPECT_CALL(*testRestClientListener, on_response_body(testing::_))
         .WillOnce(
            [testResponseBody] (auto const testRecvResponseBody)
            {
               EXPECT_EQ(testResponseBody, testRecvResponseBody);
            }
         )
      ;
      auto const testContentLength = std::to_string(testResponseBody.size());
      auto testHeaders = std::map<std::string_view, std::string_view>
      {
         {"Content-Length", testContentLength},
         {"Content-Type", testContentType},
      };
      EXPECT_CALL(*testRestClientListener, on_response_header(testing::_, testing::_))
         .WillRepeatedly(
            [&testHeaders] (auto const testResponseHeaderName, auto const testResponseHeaderValue)
            {
               EXPECT_EQ(testResponseHeaderValue, testHeaders.at(testResponseHeaderName)) << testResponseHeaderName;
               testHeaders.erase(testResponseHeaderName);
            }
         )
      ;
      auto testPromise = std::promise<void>{};
      auto const testFuture = testPromise.get_future();
      auto testRestClient = web::rest_client{testThread, testRestClientListener, testConfig};
      EXPECT_CALL(*testRestClientListener, on_response_complete(testing::_))
         .WillOnce(
            [
               &testRestServer,
               &testRestClientListener,
               &testPromise,
               &testRestClient,
               &testErrorCodeMatcher
            ] (auto const &testResponseStatusCode)
            {
               EXPECT_EQ(testResponseStatusCode, 200);

               EXPECT_CALL(testRestServer, should_accept_socket()).WillOnce(testing::Return(false));
               EXPECT_CALL(*testRestClientListener, on_error(testing::_, testing::_))
                  .WillOnce(
                     [&testPromise, &testErrorCodeMatcher] (auto const testErrorCode, auto const testErrorDescription)
                     {
                        EXPECT_THAT(testErrorCode, testErrorCodeMatcher) << testErrorDescription;
                        testPromise.set_value();
                     }
                  )
               ;
               auto testRequest = web::rest_request{testRestClient.get_request()};
               testRequest
                  .with_timeout(test_good_request_timeout)
                  .perform()
               ;
            }
         )
      ;
      auto testRequest = web::rest_request{testRestClient.get_request()};
      testRequest
         .with_timeout(test_good_request_timeout)
         .with_path("/")
         .perform()
      ;
      EXPECT_EQ(std::future_status::ready, testFuture.wait_for(test_good_request_timeout));
      EXPECT_TRUE(testHeaders.empty());
   }
   /// CRUD
   {
      EXPECT_CALL(testRestServer, should_accept_socket()).WillOnce(testing::Return(true));
      EXPECT_CALL(testRestServer, should_pass_handshake()).WillOnce(testing::Return(true));
      auto testRestClientListener = std::make_shared<testing::StrictMock<test_rest_client_listener>>();
      auto testPromise = std::promise<void>{};
      auto const testFuture = testPromise.get_future();
      auto testRestClient = web::rest_client{testThread, testRestClientListener, testConfig};
      auto const testInternalCleanupCheck = [&] ()
      {
         EXPECT_CALL(testRestServer, handle_request(testing::_, testing::_))
            .WillOnce(
               [testPeerHost = std::string_view{testPeerHost}, testContentType] (auto const &testRequest, auto &testResponse)
               {
                  auto testHeaders = std::map<std::string_view, std::string_view>
                  {
                     {"Accept", "*/*"},
                     {"Host", testPeerHost},
                  };
                  EXPECT_HTTP_REQUEST(testRequest, boost::beast::http::verb::get, "/", testHeaders, "");

                  testResponse.clear();
                  testResponse.result(boost::beast::http::status::ok);
                  testResponse.version(testRequest.version());
                  testResponse.keep_alive(true);
                  testResponse.set(boost::beast::http::field::content_type, testContentType);
                  testResponse.body() = "{}";
                  testResponse.prepare_payload();
                  testResponse.content_length(testResponse.body().size());
                  return true;
               }
            )
         ;
         EXPECT_CALL(testRestServer, should_keep_alive()).WillOnce(testing::Return(true));
         EXPECT_CALL(*testRestClientListener, on_response_body(testing::_))
            .WillOnce(
               [testResponseBody = "{}"] (auto const testRecvResponseBody)
               {
                  EXPECT_EQ(testResponseBody, testRecvResponseBody);
               }
            )
         ;
         auto testHeaders = std::make_shared<std::map<std::string_view, std::string>>(
            std::map<std::string_view, std::string>
            {
               {"Content-Length", std::string{"2"}},
               {"Content-Type", testContentType},
            }
         );
         EXPECT_CALL(*testRestClientListener, on_response_complete(testing::_))
            .WillOnce(
               [testHeaders, &testPromise] (auto const &testResponseStatusCode)
               {
                  EXPECT_EQ(testResponseStatusCode, 200);

                  testPromise.set_value();
                  EXPECT_TRUE(testHeaders->empty());
               }
            )
         ;
         EXPECT_CALL(*testRestClientListener, on_response_header(testing::_, testing::_))
            .WillRepeatedly(
               [testHeaders] (auto const testResponseHeaderName, auto const testResponseHeaderValue)
               {
                  EXPECT_EQ(testResponseHeaderValue, testHeaders->at(testResponseHeaderName)) << testResponseHeaderName;
                  testHeaders->erase(testResponseHeaderName);
               }
            )
         ;

         auto testRequest = web::rest_request{testRestClient.get_request()};
         testRequest
            .with_timeout(test_good_request_timeout)
            .perform()
         ;
      };
      auto const testDeleteRequest = [&] ()
      {
         constexpr auto testUrlPath = std::string_view{"/test_method/delete"};
         constexpr auto testUrlQuery = std::string_view{"?test_operation=delete"};
         constexpr auto testRequestBody = std::string_view{R"raw({"test_operation":"delete"})raw"};
         constexpr auto testResponseBody = std::string_view{R"raw({"test_result":"deleted"})raw"};
         EXPECT_CALL(testRestServer, handle_request(testing::_, testing::_))
            .WillOnce(
               [testPeerHost = std::string_view{testPeerHost}, testUrlPath, testUrlQuery, testContentType, testRequestBody, testResponseBody] (auto const &testRequest, auto &testResponse)
               {
                  auto const testTarget = std::string{}.append(testUrlPath).append(testUrlQuery);
                  auto const testContentLength = std::to_string(testRequestBody.size());
                  auto testHeaders = std::map<std::string_view, std::string_view>
                  {
                     {"Accept", "*/*"},
                     {"Content-Length", testContentLength},
                     {"Content-Type", testContentType},
                     {"Host", testPeerHost},
                     {"Test-Method", "DELETE"},
                     {"Test-Operation", "DELETE"},
                  };
                  EXPECT_HTTP_REQUEST(testRequest, boost::beast::http::verb::delete_, testTarget, testHeaders, testRequestBody);

                  testResponse.clear();
                  testResponse.result(boost::beast::http::status::accepted);
                  testResponse.version(testRequest.version());
                  testResponse.keep_alive(true);
                  testResponse.set(boost::beast::http::field::content_type, testContentType);
                  testResponse.body() = testResponseBody;
                  testResponse.prepare_payload();
                  testResponse.content_length(testResponse.body().size());
                  return true;
               }
            )
         ;
         EXPECT_CALL(testRestServer, should_keep_alive()).WillOnce(testing::Return(true));
         EXPECT_CALL(*testRestClientListener, on_response_body(testing::_))
            .WillOnce(
               [testResponseBody] (auto const testRecvResponseBody)
               {
                  EXPECT_EQ(testResponseBody, testRecvResponseBody);
               }
            )
         ;
         auto testHeaders = std::make_shared<std::map<std::string_view, std::string>>(
            std::map<std::string_view, std::string>
            {
               {"Content-Length", std::to_string(testResponseBody.size())},
               {"Content-Type", testContentType},
            }
         );
         EXPECT_CALL(*testRestClientListener, on_response_complete(testing::_))
            .WillOnce(
               [testHeaders, &testInternalCleanupCheck] (auto const &testResponseStatusCode)
               {
                  EXPECT_EQ(testResponseStatusCode, 202);

                  testInternalCleanupCheck();
                  EXPECT_TRUE(testHeaders->empty());
               }
            )
         ;
         EXPECT_CALL(*testRestClientListener, on_response_header(testing::_, testing::_))
            .WillRepeatedly(
               [testHeaders] (auto const testResponseHeaderName, auto const testResponseHeaderValue)
               {
                  EXPECT_EQ(testResponseHeaderValue, testHeaders->at(testResponseHeaderName)) << testResponseHeaderName;
                  testHeaders->erase(testResponseHeaderName);
               }
            )
         ;

         auto testRequest = web::rest_request{testRestClient.custom_request("DELETE")};
         testRequest
            .with_timeout(test_good_request_timeout)
            .with_path(testUrlPath)
            .with_query(testUrlQuery)
            .with_header("Test-Method:DELETE")
            .with_header("Test-Operation", "DELETE")
            .with_body(testRequestBody, testContentType)
            .perform()
         ;
      };
      auto const testUpdateRequest = [&] ()
      {
         constexpr auto testUrlPath = std::string_view{"test_method/put"};
         constexpr auto testUrlQuery = std::string_view{"test_operation=update"};
         constexpr auto testRequestBody = std::string_view{R"raw({"test_operation":"update"})raw"};
         constexpr auto testResponseBody = std::string_view{R"raw({"test_result":"updated"})raw"};
         EXPECT_CALL(testRestServer, handle_request(testing::_, testing::_))
            .WillOnce(
               [testPeerHost = std::string_view{testPeerHost}, testUrlPath, testUrlQuery, testContentType, testRequestBody, testResponseBody] (auto const &testRequest, auto &testResponse)
               {
                  auto const testTarget = std::string{"/"}.append(testUrlPath).append("?").append(testUrlQuery);
                  auto const testContentLength = std::to_string(testRequestBody.size());
                  auto testHeaders = std::map<std::string_view, std::string_view>
                  {
                     {"Accept", "*/*"},
                     {"Host", testPeerHost},
                     {"Content-Length", testContentLength},
                     {"Content-Type", testContentType},
                     {"Test-Method", "PUT"},
                     {"Test-Operation", "UPDATE"},
                  };
                  EXPECT_HTTP_REQUEST(testRequest, boost::beast::http::verb::put, testTarget, testHeaders, testRequestBody);

                  testResponse.clear();
                  testResponse.result(boost::beast::http::status::ok);
                  testResponse.version(testRequest.version());
                  testResponse.keep_alive(true);
                  testResponse.set(boost::beast::http::field::content_type, testContentType);
                  testResponse.body() = testResponseBody;
                  testResponse.prepare_payload();
                  testResponse.content_length(testResponse.body().size());
                  return true;
               }
            )
         ;
         EXPECT_CALL(testRestServer, should_keep_alive()).WillOnce(testing::Return(true));
         EXPECT_CALL(*testRestClientListener, on_response_body(testing::_))
            .WillOnce(
               [testResponseBody] (auto const testRecvResponseBody)
               {
                  EXPECT_EQ(testResponseBody, testRecvResponseBody);
               }
            )
         ;
         auto testHeaders = std::make_shared<std::map<std::string_view, std::string>>(
            std::map<std::string_view, std::string>
            {
               {"Content-Length", std::to_string(testResponseBody.size())},
               {"Content-Type", testContentType},
            }
         );
         EXPECT_CALL(*testRestClientListener, on_response_complete(testing::_))
            .WillOnce(
               [testHeaders, &testDeleteRequest] (auto const &testResponseStatusCode)
               {
                  EXPECT_EQ(testResponseStatusCode, 200);

                  testDeleteRequest();
                  EXPECT_TRUE(testHeaders->empty());
               }
            )
         ;
         EXPECT_CALL(*testRestClientListener, on_response_header(testing::_, testing::_))
            .WillRepeatedly(
               [testHeaders] (auto const testResponseHeaderName, auto const testResponseHeaderValue)
               {
                  EXPECT_EQ(testResponseHeaderValue, testHeaders->at(testResponseHeaderName)) << testResponseHeaderName;
                  testHeaders->erase(testResponseHeaderName);
               }
            )
         ;

         auto testRequest = web::rest_request{testRestClient.upload_request()};
         testRequest
            .with_timeout(test_good_request_timeout)
            .with_path(testUrlPath)
            .with_query(testUrlQuery)
            .with_header("Test-Method:PUT")
            .with_header("Test-Operation", "UPDATE")
            .with_body(testRequestBody, testContentType)
            .perform()
         ;
      };
      auto const testReadRequest = [&] ()
      {
         constexpr auto testUrlPath = std::string_view{"test_method/get"};
         constexpr auto testUrlQuery = std::string_view{"test_operation=read"};
         constexpr auto testResponseBody = std::string_view{R"raw({"test_result":"read"})raw"};
         EXPECT_CALL(testRestServer, handle_request(testing::_, testing::_))
            .WillOnce(
               [testPeerHost = std::string_view{testPeerHost}, testUrlPath, testUrlQuery, testContentType, testResponseBody] (auto const &testRequest, auto &testResponse)
               {
                  auto const testTarget = std::string{"/"}.append(testUrlPath).append("?").append(testUrlQuery);
                  auto testHeaders = std::map<std::string_view, std::string_view>
                  {
                     {"Accept", "*/*"},
                     {"Host", testPeerHost},
                     {"Test-Method", "GET"},
                     {"Test-Operation", "READ"},
                  };
                  EXPECT_HTTP_REQUEST(testRequest, boost::beast::http::verb::get, testTarget, testHeaders, "");

                  testResponse.clear();
                  testResponse.result(boost::beast::http::status::ok);
                  testResponse.version(testRequest.version());
                  testResponse.keep_alive(true);
                  testResponse.set(boost::beast::http::field::content_type, testContentType);
                  testResponse.body() = testResponseBody;
                  testResponse.prepare_payload();
                  testResponse.content_length(testResponse.body().size());
                  return true;
               }
            )
         ;
         EXPECT_CALL(testRestServer, should_keep_alive()).WillOnce(testing::Return(true));
         EXPECT_CALL(*testRestClientListener, on_response_body(testing::_))
            .WillOnce(
               [testResponseBody] (auto const testRecvResponseBody)
               {
                  EXPECT_EQ(testResponseBody, testRecvResponseBody);
               }
            )
         ;
         auto testHeaders = std::make_shared<std::map<std::string_view, std::string>>(
            std::map<std::string_view, std::string>
            {
               {"Content-Length", std::to_string(testResponseBody.size())},
               {"Content-Type", testContentType},
            }
         );
         EXPECT_CALL(*testRestClientListener, on_response_complete(testing::_))
            .WillOnce(
               [testHeaders, &testUpdateRequest] (auto const &testResponseStatusCode)
               {
                  EXPECT_EQ(testResponseStatusCode, 200);

                  testUpdateRequest();
                  EXPECT_TRUE(testHeaders->empty());
               }
            )
         ;
         EXPECT_CALL(*testRestClientListener, on_response_header(testing::_, testing::_))
            .WillRepeatedly(
               [testHeaders] (auto const testResponseHeaderName, auto const testResponseHeaderValue)
               {
                  EXPECT_EQ(testResponseHeaderValue, testHeaders->at(testResponseHeaderName)) << testResponseHeaderName;
                  testHeaders->erase(testResponseHeaderName);
               }
            )
         ;

         auto testRequest = web::rest_request{testRestClient.get_request()};
         testRequest
            .with_timeout(test_good_request_timeout)
            .with_path(testUrlPath)
            .with_query(testUrlQuery)
            .with_header("Test-Method:GET")
            .with_header("Test-Operation", "READ")
            .perform()
         ;
      };
      auto const testCreateRequest = [&] ()
      {
         constexpr auto testUrlPath = std::string_view{"test_method/post"};
         constexpr auto testUrlQuery = std::string_view{"test_operation=create"};
         constexpr auto testRequestBody = std::string_view{R"raw({"test_operation":"create"})raw"};
         constexpr auto testResponseBody = std::string_view{R"raw({"test_result":"created"})raw"};
         EXPECT_CALL(testRestServer, handle_request(testing::_, testing::_))
            .WillOnce(
               [testPeerHost = std::string_view{testPeerHost}, testUrlPath, testUrlQuery, testContentType, testRequestBody, testResponseBody](auto const &testRequest, auto &testResponse)
               {
                  auto const testTarget = std::string{"/"}.append(testUrlPath).append("?").append(testUrlQuery);
                  auto const testContentLength = std::to_string(testRequestBody.size());
                  auto testHeaders = std::map<std::string_view, std::string_view>
                  {
                     {"Accept", "*/*"},
                     {"Content-Length", testContentLength},
                     {"Content-Type", testContentType},
                     {"Host", testPeerHost},
                     {"Test-Method", "POST"},
                     {"Test-Operation", "CREATE"},
                  };
                  EXPECT_HTTP_REQUEST(testRequest, boost::beast::http::verb::post, testTarget, testHeaders, testRequestBody);

                  testResponse.clear();
                  testResponse.result(boost::beast::http::status::created);
                  testResponse.version(testRequest.version());
                  testResponse.keep_alive(true);
                  testResponse.set(boost::beast::http::field::content_type, testContentType);
                  testResponse.body() = testResponseBody;
                  testResponse.prepare_payload();
                  testResponse.content_length(testResponse.body().size());
                  return true;
               }
            )
         ;
         EXPECT_CALL(testRestServer, should_keep_alive()).WillOnce(testing::Return(true));
         EXPECT_CALL(*testRestClientListener, on_response_body(testing::_))
            .WillOnce(
               [testResponseBody] (auto const testRecvResponseBody)
               {
                  EXPECT_EQ(testResponseBody, testRecvResponseBody);
               }
            )
         ;
         auto testHeaders = std::make_shared<std::map<std::string_view, std::string>>(
            std::map<std::string_view, std::string>
            {
               {"Content-Length", std::to_string(testResponseBody.size())},
               {"Content-Type", testContentType},
            }
         );
         EXPECT_CALL(*testRestClientListener, on_response_complete(testing::_))
            .WillOnce(
               [testHeaders, &testReadRequest] (auto const &testResponseStatusCode)
               {
                  EXPECT_EQ(testResponseStatusCode, 201);

                  testReadRequest();
                  EXPECT_TRUE(testHeaders->empty());
               }
            )
         ;
         EXPECT_CALL(*testRestClientListener, on_response_header(testing::_, testing::_))
            .WillRepeatedly(
               [testHeaders] (auto const testResponseHeaderName, auto const testResponseHeaderValue)
               {
                  EXPECT_EQ(testResponseHeaderValue, testHeaders->at(testResponseHeaderName)) << testResponseHeaderName;
                  testHeaders->erase(testResponseHeaderName);
               }
            )
         ;

         auto testRequest = web::rest_request{testRestClient.post_request()};
         testRequest
            .with_timeout(test_good_request_timeout)
            .with_path(testUrlPath)
            .with_query(testUrlQuery)
            .with_header("Test-Method:POST")
            .with_header("Test-Operation", "CREATE")
            .with_body(testRequestBody, testContentType)
            .perform()
         ;
      };
      testCreateRequest();
      EXPECT_EQ(std::future_status::ready, testFuture.wait_for(test_good_request_timeout));
   }
}
