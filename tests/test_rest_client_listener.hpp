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

#include <webthread/rest_client_listener.hpp> ///< for web::rest_client_listener

#include <gmock/gmock.h> ///< for MOCK_METHOD

#include <cstdint> ///< for intptr_t
#include <string_view> ///< for std::string_view

class [[nodiscard]] test_rest_client_listener : public web::rest_client_listener
{
public:
   test_rest_client_listener() noexcept = default;
   test_rest_client_listener(test_rest_client_listener &&) = delete;
   test_rest_client_listener(test_rest_client_listener const &) = delete;

   test_rest_client_listener &operator = (test_rest_client_listener &&) = delete;
   test_rest_client_listener &operator = (test_rest_client_listener const &) = delete;

   MOCK_METHOD(void, on_error, (intptr_t testErrorCode, std::string_view testErrorDescription), (override));
   MOCK_METHOD(void, on_response_body, (std::string_view testResponseBody), (override));
   MOCK_METHOD(void, on_response_complete, (intptr_t testResponseStatusCode), (override));
   MOCK_METHOD(void, on_response_header, (std::string_view testResponseHeaderName, std::string_view testResponseHeaderValue), (override));
};
