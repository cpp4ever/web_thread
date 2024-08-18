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

#include <boost/asio/io_context.hpp> ///< for boost::asio::io_context
#include <boost/asio/ip/address.hpp> ///< for boost::asio::ip::address
#include <boost/asio/ip/tcp.hpp> ///< for boost::asio::ip::tcp::acceptor
#include <boost/beast.hpp> ///< for boost::beast::flat_buffer, boost::beast::tcp_stream
#include <boost/beast/websocket.hpp> ///< for boost::beast::websocket::stream
#include <gmock/gmock.h> ///< for MOCK_METHOD

#include <cstdint> ///< for uint16_t
#include <deque> ///< for std::deque
#include <memory> ///< for std::unique_ptr
#include <thread> ///< for std::thread

template<typename test_websocket_stream>
class [[nodiscard]] test_websocket_server
{
public:
   test_websocket_server() = delete;
   test_websocket_server(test_websocket_server &&) = delete;
   test_websocket_server(test_websocket_server const &) = delete;
   [[nodiscard]] explicit test_websocket_server(boost::asio::ip::address const &testAddress);
   virtual ~test_websocket_server();

   test_websocket_server &operator = (test_websocket_server &&) = delete;
   test_websocket_server &operator = (test_websocket_server const &) = delete;

   [[nodiscard]] uint16_t local_port() const;

   MOCK_METHOD(bool, should_accept_socket, (), ());
   MOCK_METHOD(bool, should_pass_handshake, (), ());
   MOCK_METHOD(bool, should_accept_websocket, (), ());
   MOCK_METHOD(bool, handle_message, (boost::beast::flat_buffer const &inboundBuffer, std::string &outboundBuffer), ());
   MOCK_METHOD(bool, should_keep_alive, (), ());

private:
   boost::asio::io_context m_ioContext;
   boost::asio::ip::tcp::acceptor m_acceptor;
   boost::beast::flat_buffer m_inboundBuffer;
   std::string m_outboundBuffer;
   std::deque<test_websocket_stream> m_streams;
   std::unique_ptr<std::thread> m_thread;

   void async_read(test_websocket_stream &stream);
   void async_socket_accept();
   void async_websocket_accept(test_websocket_stream &stream);
   void async_write(test_websocket_stream &stream);

   void thread_handler();
};
