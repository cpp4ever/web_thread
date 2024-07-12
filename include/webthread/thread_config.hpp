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

#include <cassert> ///< for assert
#include <cstddef> ///< for size_t
#include <cstdint> ///< for uint16_t

namespace web
{

class [[nodiscard("discarded-value expression detected")]] thread_config final
{
public:
   static constexpr uint16_t no_affinity = static_cast<uint16_t>(-1);
   static constexpr size_t default_initial_connections_capacity = 0;
   static constexpr size_t default_max_expected_inbound_message_size = 512;

public:
   [[maybe_unused, nodiscard]] constexpr thread_config() noexcept = default;
   [[maybe_unused, nodiscard]] constexpr thread_config(thread_config &&rhs) noexcept = default;
   [[maybe_unused, nodiscard]] constexpr thread_config(thread_config const &rhs) noexcept = default;

   [[maybe_unused]] constexpr thread_config &operator = (thread_config &&rhs) noexcept = default;
   [[maybe_unused]] constexpr thread_config &operator = (thread_config const &rhs) noexcept = default;

   [[maybe_unused, nodiscard]] constexpr uint16_t cpu_affinity() const noexcept
   {
      return m_cpuId;
   }

   [[maybe_unused, nodiscard]] constexpr size_t initial_connections_capacity() const noexcept
   {
      return m_initialConnectionsCapacity;
   }

   [[maybe_unused, nodiscard]] constexpr size_t max_expected_inbound_message_size() const noexcept
   {
      return m_maxExpectedInboundMessageSize;
   }

   [[maybe_unused]] constexpr thread_config &with_cpu_affinity(uint16_t const cpuId) noexcept
   {
      m_cpuId = cpuId;
      return *this;
   }

   [[maybe_unused]] constexpr thread_config &with_initial_connections_capacity(size_t const initialConnectionsCapacity) noexcept
   {
      m_initialConnectionsCapacity = initialConnectionsCapacity;
      return *this;
   }

   [[maybe_unused]] constexpr thread_config &with_max_expected_inbound_message_size(size_t const maxExpectedInboundMessageSize) noexcept
   {
      assert(1 < maxExpectedInboundMessageSize);
      m_maxExpectedInboundMessageSize = maxExpectedInboundMessageSize;
      return *this;
   }

private:
   uint16_t m_cpuId = no_affinity;
   size_t m_initialConnectionsCapacity = default_initial_connections_capacity;
   size_t m_maxExpectedInboundMessageSize = default_max_expected_inbound_message_size;
};

}
