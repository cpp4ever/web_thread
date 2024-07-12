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

#include "test_common.hpp" ///< for EXPECT_ERROR_CODE
#include "test_server_context.hpp" ///< for test_server_context, test_tls_stream
#include "test_websocket_server.hpp" ///< for test_websocket_server

#include <boost/asio/ip/address.hpp> ///< for boost::asio::ip::address
#include <boost/asio/ip/tcp.hpp> ///< for boost::asio::ip::tcp::socket
#if (not defined(WIN32))
#  include <boost/asio/ssl/stream_base.hpp> ///< for boost::asio::ssl::stream_base::handshake_type
#endif
#include <boost/asio/strand.hpp> ///< for boost::asio::make_strand
#include <boost/beast.hpp> ///< for boost::beast::get_lowest_layer, boost::beast::role_type, boost::beast::tcp_stream
#include <boost/beast/websocket.hpp> ///< for boost::beast::websocket::close_code, boost::beast::websocket::close_reason, boost::beast::websocket::error, boost::beast::websocket::stream, boost::beast::websocket::stream_base::timeout
#if (not defined(WIN32))
#include <boost/beast/websocket/ssl.hpp> ///< for boost::beast::async_teardown
#endif
#include <boost/system/error_code.hpp> ///< for boost::system::error_code
#if (defined(WIN32))
#  include <boost/wintls/handshake_type.hpp> ///< for boost::wintls::handshake_type
#endif

#include <memory> ///< for std::make_unique
#include <thread> ///< for std::thread
#include <type_traits> ///< for std::is_same_v
#include <utility> ///< for std::forward, std::move

#if (defined(WIN32))
namespace boost::beast
{

template<typename TeardownHandler>
void async_teardown(role_type const, test_tls_stream &stream, TeardownHandler &&handler)
{
   stream.async_shutdown(std::forward<TeardownHandler>(handler));
}

}
#endif

template<typename test_websocket_stream>
test_websocket_server<test_websocket_stream>::test_websocket_server(boost::asio::ip::address const &testAddress) :
   m_ioContext(1),
   m_acceptor(m_ioContext, {testAddress, 0}),
   m_inboundBuffer(1024),
   m_outboundBuffer(),
   m_streams(),
   m_thread()
{
   m_thread = std::make_unique<std::thread>(
      &test_websocket_server::thread_handler,
      this
   );
}

template<typename test_websocket_stream>
test_websocket_server<test_websocket_stream>::~test_websocket_server()
{
   m_ioContext.stop();
   m_thread->join();
}

template<typename test_websocket_stream>
uint16_t test_websocket_server<test_websocket_stream>::local_port() const
{
   return m_acceptor.local_endpoint().port();
}

template<typename test_websocket_stream>
void test_websocket_server<test_websocket_stream>::async_read(test_websocket_stream &stream)
{
   m_inboundBuffer.clear();
   stream.async_read(
      m_inboundBuffer,
      [&] (auto testErrorCode, auto const)
      {
         if (boost::beast::websocket::error::closed == testErrorCode)
         {
            return;
         }
         EXPECT_ERROR_CODE(testErrorCode);
         m_outboundBuffer.clear();
         if ((false == testErrorCode.failed()) && (true == handle_message(m_inboundBuffer, m_outboundBuffer)))
         {
            async_write(stream);
         }
         else
         {
            boost::beast::get_lowest_layer(stream).close();
         }
      }
   );
}

template<typename test_websocket_stream>
void test_websocket_server<test_websocket_stream>::async_socket_accept()
{
   m_acceptor.async_accept(
      boost::asio::make_strand(m_ioContext),
      [&] (auto testErrorCode, auto testTcpSocket)
      {
         EXPECT_ERROR_CODE(testErrorCode);
         if ((false == testErrorCode.failed()) && (true == should_accept_socket()))
         {
            auto &stream = m_streams.emplace_back(test_server_context<test_websocket_stream>::accept(std::move(testTcpSocket)));
            if constexpr (std::is_same_v<test_websocket_stream, boost::beast::websocket::stream<boost::beast::tcp_stream>>)
            {
               if (true == should_pass_handshake())
               {
                  async_websocket_accept(stream);
               }
               else
               {
                  boost::beast::get_lowest_layer(stream).close();
               }
            }
            else if constexpr (std::is_same_v<test_websocket_stream, boost::beast::websocket::stream<test_tls_stream>>)
            {
               boost::beast::get_lowest_layer(stream).expires_after(test_good_request_timeout);
               stream.next_layer().async_handshake(
#if (defined(WIN32))
                  boost::wintls::handshake_type::server,
#else
                  boost::asio::ssl::stream_base::server,
#endif
                  [&] (auto testErrorCode)
                  {
                     EXPECT_ERROR_CODE(testErrorCode);
                     if ((false == testErrorCode.failed()) && (true == should_pass_handshake()))
                     {
                        async_websocket_accept(stream);
                     }
                     else
                     {
                        boost::beast::get_lowest_layer(stream).close();
                     }
                  }
               );
            }
            else
            {
               assert(false && "must be unreachable!");
            }
         }
         else
         {
            testTcpSocket.close(testErrorCode);
            EXPECT_ERROR_CODE(testErrorCode);
         }
         async_socket_accept();
      }
   );
}

template<typename test_websocket_stream>
void test_websocket_server<test_websocket_stream>::async_websocket_accept(test_websocket_stream &stream)
{
   boost::beast::get_lowest_layer(stream).expires_never();
   stream.set_option(
      boost::beast::websocket::stream_base::timeout::suggested(
         boost::beast::role_type::server
      )
   );
   stream.async_accept(
      [&] (auto testErrorCode)
      {
         if (boost::beast::websocket::error::closed == testErrorCode)
         {
            return;
         }
         EXPECT_ERROR_CODE(testErrorCode);
         if ((false == testErrorCode.failed()) && (true == should_accept_websocket()))
         {
            async_read(stream);
         }
         else
         {
            boost::beast::get_lowest_layer(stream).close();
         }
      }
   );
}

template<typename test_websocket_stream>
void test_websocket_server<test_websocket_stream>::async_write(test_websocket_stream &stream)
{
   stream.text(stream.got_text());
   stream.async_write(
      boost::asio::buffer(static_cast<std::string const &>(m_outboundBuffer)),
      [&](auto testErrorCode, auto const)
      {
         EXPECT_ERROR_CODE(testErrorCode);
         if ((false == testErrorCode.failed()) && (true == should_keep_alive()))
         {
            async_read(stream);
         }
         else
         {
            boost::beast::get_lowest_layer(stream).close();
         }
      }
   );
}

template<typename test_websocket_stream>
void test_websocket_server<test_websocket_stream>::thread_handler()
{
   async_socket_accept();
   m_ioContext.run();
}

template class test_websocket_server<boost::beast::websocket::stream<boost::beast::tcp_stream>>;
template class test_websocket_server<boost::beast::websocket::stream<test_tls_stream>>;
