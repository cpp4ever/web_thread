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

#include "webthread/constants.hpp" ///< for web::default_request_timeout
#include "webthread/rest_client.hpp" ///< for web::thread::rest_client
#include "webthread/rest_method.hpp" ///< for web::rest_method
#include "webthread/rest_request.hpp" ///< for web::thread::rest_request
#include "webthread/thread.hpp" ///< for web::thread
#include "webthread/thread_config.hpp" ///< for web::thread_config
#include "webthread/websocket.hpp" ///< for web::thread::websocket
#include "webthread/websocket_message_type.hpp" ///< for web::websocket_message_type

#include <curl/curl.h>
#include <event.h>
#if (defined(__linux__))
#  include <sched.h> ///< for CPU_SET, cpu_set_t, CPU_ZERO, sched_setaffinity
#elif (defined(WIN32))
#  include <processthreadsapi.h> ///< for GetCurrentThread
#  include <winbase.h> ///< for SetThreadAffinityMask
#endif

#include <algorithm> ///< for std::find
#include <atomic> ///< for ATOMIC_FLAG_INIT, std::atomic_bool, std::atomic_flag
#include <cassert> ///< for assert
#include <chrono> ///< for std::chrono::milliseconds, std::chrono::seconds, std::chrono::system_clock
#include <cstddef> ///< for size_t
#include <cstdint> ///< for intptr_t, uint16_t
#include <cstring> ///< for std::memcpy
#include <deque> ///< for std::deque
#include <memory> ///< for std::addressof, std::make_unique, std::memory_order_acquire, std::memory_order_relaxed, std::memory_order_release, std::unique_ptr
#include <mutex> ///< for std::scoped_lock
#if (201911L <= __cpp_lib_jthread)
#  include <stop_token> ///< for std::stop_token
#endif
#include <string> ///< for std::string
#if (defined(WIN32))
#  include <string.h> ///< for _strnicmp
#else
#  include <strings.h> ///< for strncasecmp
#endif
#include <thread> ///< for std::jthread, std::thread
#include <variant> ///< for std::get, std::variant
#include <vector> ///< for std::begin, std::end, std::erase, std::vector
#include <utility> ///< for std::pair, std::swap

namespace web
{

class [[nodiscard("discarded-value expression detected")]] thread::timer_queue final
{
private:
   struct [[nodiscard("discarded-value expression detected")]] timer_task final
   {
      timer_task *next = nullptr;
      std::chrono::system_clock::time_point expirationTime = {};
      thread::connection *connection = nullptr;
   };

public:
   timer_queue() = delete;

   [[nodiscard]] timer_queue(timer_queue &&rhs) noexcept
   {
      std::swap(m_priorityTaskList, rhs.m_priorityTaskList);
      std::swap(m_freeTaskList, rhs.m_freeTaskList);
      m_allTasks.swap(rhs.m_allTasks);
   }

   timer_queue(timer_queue const &) = delete;

   [[nodiscard]] explicit timer_queue(size_t const initialCapacity)
   {
      for (size_t taskIndex = 0; taskIndex < initialCapacity; ++taskIndex)
      {
         auto &task = m_allTasks.emplace_back();
         task.next = m_freeTaskList;
         m_freeTaskList = std::addressof(task);
      }
   }

   ~timer_queue()
   {
      assert(nullptr == m_priorityTaskList);
      [[maybe_unused]] size_t tasksCount = 0;
      while (nullptr != m_freeTaskList)
      {
         auto *task = m_freeTaskList;
         m_freeTaskList = task->next;
         task->next = nullptr;
         ++tasksCount;
      }
      assert(m_allTasks.size() == tasksCount);
   }

   timer_queue &operator = (timer_queue &&) = delete;
   timer_queue &operator = (timer_queue const &) = delete;

   [[nodiscard]] thread::connection *pop(std::chrono::system_clock::time_point const time)
   {
      if ((nullptr != m_priorityTaskList) && (time >= m_priorityTaskList->expirationTime))
      {
         auto *task = m_priorityTaskList;
         assert(nullptr != task->connection);
         assert((nullptr == task->next) || (task->expirationTime <= task->next->expirationTime));
         m_priorityTaskList = task->next;
         auto *connection = task->connection;
         task->next = m_freeTaskList;
         task->connection = nullptr;
         task->expirationTime = {};
         m_freeTaskList = task;
         return connection;
      }
      return nullptr;
   }

   void push(std::chrono::system_clock::time_point const expirationTime, thread::connection &connection)
   {
      timer_task *lastPriorityTask = nullptr;
      for (auto *priorityTask = m_priorityTaskList; nullptr != priorityTask; priorityTask = priorityTask->next)
      {
         if (expirationTime < priorityTask->expirationTime)
         {
            break;
         }
         lastPriorityTask = priorityTask;
      }
      auto &task = free_task();
      assert(nullptr == task.next);
      assert(std::chrono::system_clock::time_point{} == task.expirationTime);
      assert(nullptr == task.connection);
      task.expirationTime = expirationTime;
      task.connection = std::addressof(connection);
      if (nullptr == lastPriorityTask)
      {
         task.next = m_priorityTaskList;
         m_priorityTaskList = std::addressof(task);
      }
      else
      {
         task.next = lastPriorityTask->next;
         lastPriorityTask->next = std::addressof(task);
      }
   }

   void remove(thread::connection &connection)
   {
      timer_task *lastPriorityTask = nullptr;
      for (auto *priorityTask = m_priorityTaskList; nullptr != priorityTask; priorityTask = priorityTask->next)
      {
         if (priorityTask->connection == std::addressof(connection))
         {
            if (nullptr == lastPriorityTask)
            {
               assert(priorityTask == m_priorityTaskList);
               m_priorityTaskList = m_priorityTaskList->next;
            }
            else
            {
               lastPriorityTask->next = priorityTask->next;
            }
            priorityTask->next = m_freeTaskList;
            priorityTask->expirationTime = {};
            priorityTask->connection = nullptr;
            m_freeTaskList = priorityTask;
            break;
         }
         lastPriorityTask = priorityTask;
      }
   }

private:
   timer_task *m_priorityTaskList = nullptr;
   timer_task *m_freeTaskList = nullptr;
   std::deque<timer_task> m_allTasks = {};

   [[nodiscard]] timer_task &free_task()
   {
      if (nullptr == m_freeTaskList)
      {
         return m_allTasks.emplace_back();
      }
      auto *task = m_freeTaskList;
      m_freeTaskList = task->next;
      task->next = nullptr;
      return *task;
   }
};

namespace
{

template<typename return_code>
constexpr void suppress_nodiscard(return_code const) noexcept
{
   /// Suppress nodiscard warning
}

#if (defined NDEBUG)
#  define EXPECT_ERROR_CODE(errorCode, expression) suppress_nodiscard(expression)
#else
#  define EXPECT_ERROR_CODE(errorCode, expression) assert(errorCode == expression)
#endif

class [[nodiscard("discarded-value expression detected")]] spin_lock final
{
public:
   [[maybe_unused, nodiscard]] spin_lock() noexcept = default;
   spin_lock(spin_lock const &) = delete;
   spin_lock(spin_lock &&) = delete;

   spin_lock &operator = (spin_lock const &) = delete;
   spin_lock &operator = (spin_lock &&) = delete;

   [[maybe_unused]] void lock() noexcept
   {
      while (m_atomicFlag.test_and_set(std::memory_order_acquire))
      {
#if (defined(__cpp_lib_atomic_flag_test))
         while (m_atomicFlag.test(std::memory_order_relaxed))
#endif
            ;
      }
   }

   [[maybe_unused]] void unlock() noexcept
   {
      m_atomicFlag.clear(std::memory_order_release);
   }

private:
#if (202002L <= __cplusplus)
   std::atomic_flag m_atomicFlag;
#else
   std::atomic_flag m_atomicFlag = ATOMIC_FLAG_INIT;
#endif
};

void set_thread_affinity([[maybe_unused]] uint16_t const cpuId) noexcept
{
   if (thread_config::no_affinity == cpuId)
   {
      return;
   }
#if (defined(__linux__))
   cpu_set_t affinityMask;
   CPU_ZERO(&affinityMask);
   CPU_SET(cpuId, &affinityMask);
   EXPECT_ERROR_CODE(0, sched_setaffinity(0, sizeof(affinityMask), std::addressof(affinityMask)));
#elif (defined(WIN32))
   auto const affinityMask = (static_cast<DWORD_PTR>(1) << cpuId);
   [[maybe_unused]] auto const errorCode = SetThreadAffinityMask(GetCurrentThread(), affinityMask);
   assert(0 != errorCode);
#endif
}

class [[nodiscard("discarded-value expression detected")]] global_context
{
public:
   global_context()
   {
      curl_global_init(CURL_GLOBAL_ALL);
   }

   global_context(global_context &&) = delete;
   global_context(global_context const &) = delete;

   ~global_context()
   {
      curl_global_cleanup();
      libevent_global_shutdown();
   }

   global_context &operator = (global_context const &) = delete;
   global_context &operator = (global_context &&) = delete;
};

enum class connection_status : uint8_t
{
   initializing,
   ready,
   busy,
   inactive,
};

struct [[nodiscard("discarded-value expression detected")]] socket_event
{
   std::variant<event, socket_event *> eventOrNext;
};

struct [[nodiscard("discarded-value expression detected")]] event_context final
{
   event_base &eventBase;
   CURLM *multiHandle = nullptr;
   socket_event *freeSocketEvents = nullptr;
   thread::timer_queue timerQueue;
   std::deque<socket_event> allSocketEvents = {};
};

[[nodiscard]] socket_event *acquire_socket_event(event_context &eventContext)
{
   socket_event *socketEvent;
   if (nullptr != eventContext.freeSocketEvents) [[likely]]
   {
      socketEvent = eventContext.freeSocketEvents;
      eventContext.freeSocketEvents = std::get<socket_event *>(socketEvent->eventOrNext);
   }
   else
   {
      socketEvent = std::addressof(eventContext.allSocketEvents.emplace_back());
   }
   socketEvent->eventOrNext = event{.ev_base = nullptr};
   return socketEvent;
}

void release_socket_event(event_context &eventContext, socket_event &socketEvent)
{
   if (nullptr != std::get<event>(socketEvent.eventOrNext).ev_base)
   {
      EXPECT_ERROR_CODE(0, event_del(std::addressof(std::get<event>(socketEvent.eventOrNext))));
   }
   socketEvent.eventOrNext = eventContext.freeSocketEvents;
   eventContext.freeSocketEvents = std::addressof(socketEvent);
}

}

class [[nodiscard("discarded-value expression detected")]] thread::connection
{
public:
   [[nodiscard]] connection() noexcept = default;
   connection(connection &&) = delete;
   connection(connection const &) = delete;
   virtual ~connection() noexcept = default;

   connection &operator = (connection &&) = delete;
   connection &operator = (connection const &) = delete;

   virtual void register_handle(event_context &eventContext) = 0;
   virtual void send_message(event_context &eventContext) = 0;
   virtual void tick(event_context &eventContext) = 0;
   virtual void unregister_handle(event_context &eventContext) = 0;

   static int curl_socket_callback(CURL *handle, curl_socket_t const socket, int const socketAction, void *callbackUserdata, void *socketUserdata)
   {
      assert(nullptr != handle);
      assert(0 != socket);
      assert(nullptr != callbackUserdata);
      auto &eventContext = *static_cast<event_context *>(callbackUserdata);
      assert(nullptr != eventContext.multiHandle);
      auto *socketEvent = static_cast<socket_event *>(socketUserdata);
      switch (socketAction)
      {
      case CURL_POLL_IN:
      case CURL_POLL_OUT:
      case CURL_POLL_INOUT:
      {
         if (nullptr == socketEvent)
         {
            socketEvent = acquire_socket_event(eventContext);
            EXPECT_ERROR_CODE(CURLM_OK, curl_multi_assign(eventContext.multiHandle, socket, socketEvent));
            connection *self = nullptr;
            EXPECT_ERROR_CODE(CURLE_OK, curl_easy_getinfo(handle, CURLINFO_PRIVATE, std::addressof(self)));
            assert(nullptr != self);
            self->on_attach_socket(socket);
         }
         else
         {
            EXPECT_ERROR_CODE(0, event_del(std::addressof(std::get<event>(socketEvent->eventOrNext))));
         }
         short socketEventKind =
            ((socketAction & CURL_POLL_IN) ? EV_READ : 0) |
            ((socketAction & CURL_POLL_OUT) ? EV_WRITE : 0) |
            EV_PERSIST
         ;
         EXPECT_ERROR_CODE(
            0,
            event_assign(
               std::addressof(std::get<event>(socketEvent->eventOrNext)),
               std::addressof(eventContext.eventBase),
               socket,
               socketEventKind,
               &connection::event_socket_callback,
               std::addressof(eventContext)
            )
         );
         EXPECT_ERROR_CODE(0, event_add(std::addressof(std::get<event>(socketEvent->eventOrNext)), nullptr));
      }
      break;

      case CURL_POLL_REMOVE:
      {
         assert(nullptr != socketEvent);
         connection *self = nullptr;
         EXPECT_ERROR_CODE(CURLE_OK, curl_easy_getinfo(handle, CURLINFO_PRIVATE, std::addressof(self)));
         assert(nullptr != self);
         self->on_detach_socket(socket);
         EXPECT_ERROR_CODE(CURLM_OK, curl_multi_assign(eventContext.multiHandle, socket, nullptr));
         release_socket_event(eventContext, *socketEvent);
         socketEvent = nullptr;
      }
      break;

      default:
      {
#if (202202L <= __cpp_lib_unreachable)
         std::unreachable();
#else
         std::abort();
#endif
      }
      }
      return 0;
   }

   static void event_timer_callback(evutil_socket_t, short, void *userdata)
   {
      assert(nullptr != userdata);
      auto *eventContext = static_cast<event_context *>(userdata);
      assert(nullptr != eventContext->multiHandle);
      EXPECT_ERROR_CODE(CURLM_OK, curl_multi_socket_action(eventContext->multiHandle, CURL_SOCKET_TIMEOUT, 0, nullptr));
      process_data(*eventContext);
   }

private:
   virtual void on_attach_socket(curl_socket_t socket) = 0;
   virtual void on_detach_socket(curl_socket_t socket) = 0;
   virtual void on_recv_message(event_context &eventContext, CURLMsg const &message) = 0;

   static void event_socket_callback(evutil_socket_t socket, short const socketEventKind, void *userdata)
   {
      assert(nullptr != userdata);
      auto *eventContext = static_cast<event_context *>(userdata);
      assert(nullptr != eventContext->multiHandle);
      int socketAction =
         ((socketEventKind & EV_READ) ? CURL_CSELECT_IN : 0) |
         ((socketEventKind & EV_WRITE) ? CURL_CSELECT_OUT : 0)
      ;
      EXPECT_ERROR_CODE(CURLM_OK, curl_multi_socket_action(eventContext->multiHandle, socket, socketAction, nullptr));
      process_data(*eventContext);
   }

   static void process_data(event_context &eventContext)
   {
      assert(nullptr != eventContext.multiHandle);
      int messagesInQueue = 0;
      CURLMsg *message = nullptr;
      while (nullptr != (message = curl_multi_info_read(eventContext.multiHandle, std::addressof(messagesInQueue))))
      {
         assert(CURLMSG_DONE == message->msg);
         assert(nullptr != message->easy_handle);
         connection *self = nullptr;
         EXPECT_ERROR_CODE(CURLE_OK, curl_easy_getinfo(message->easy_handle, CURLINFO_PRIVATE, std::addressof(self)));
         assert(nullptr != self);
         self->on_recv_message(eventContext, *message);
      }
      assert(0 == messagesInQueue);
   }
};

class [[nodiscard("discarded-value expression detected")]] thread::worker final
{
private:
   enum class task_action : int8_t
   {
      remove = -1,
      send = 0,
      add = 1,
   };

   struct [[nodiscard("discarded-value expression detected")]] task final
   {
      std::shared_ptr<thread::connection> connection;
      task_action action;
   };

public:
   worker() = delete;
   worker(worker &&) = delete;
   worker(worker const &) = delete;

   [[nodiscard]] explicit worker(thread_config const threadConfig)
   {
      m_tasksQueue.reserve(threadConfig.initial_connections_capacity());
      m_websocketBuffer.resize(threadConfig.max_expected_inbound_message_size());
#if (201911L <= __cpp_lib_jthread)
      m_thread = std::make_unique<std::jthread>(
         [this] (auto const stopToken, thread_config const config)
         {
            thread_handler(stopToken, config);
         },
         threadConfig
      );
#else
      m_thread = std::make_unique<std::thread>(&worker::thread_handler, this, threadConfig);
#endif
      assert(nullptr != m_thread);
   }

   ~worker() noexcept
   {
      assert(nullptr != m_thread);
#if (201911L <= __cpp_lib_jthread)
      assert(false == m_thread->get_stop_token().stop_requested());
      m_thread->request_stop();
#else
      assert(false == m_stopToken.load(std::memory_order_acquire));
      m_stopToken.store(true, std::memory_order_release);
#endif
      m_thread->join();
      [[maybe_unused]] std::scoped_lock const guard(m_tasksQueueLock);
      assert(true == m_tasksQueue.empty());
   }

   worker &operator = (worker &&) = delete;
   worker &operator = (worker const &) = delete;

   void add_connection(std::shared_ptr<thread::connection> const &connection) noexcept
   {
      assert(nullptr != connection);
#if (201911L <= __cpp_lib_jthread)
      assert(false == m_thread->get_stop_token().stop_requested());
#else
      assert(false == m_stopToken.load(std::memory_order_relaxed));
#endif
      [[maybe_unused]] std::scoped_lock const guard(m_tasksQueueLock);
      assert(
         std::end(m_tasksQueue) == std::find_if(
            std::begin(m_tasksQueue),
            std::end(m_tasksQueue),
            [&connection] (auto const &task)
            {
               return (connection == task.connection) && (task_action::remove != task.action);
            }
         )
      );
      m_tasksQueue.emplace_back(task{.connection = connection, .action = task_action::add});
   }

   void enqueue_request(std::shared_ptr<thread::connection> const &connection) noexcept
   {
      assert(nullptr != connection);
#if (201911L <= __cpp_lib_jthread)
      assert(false == m_thread->get_stop_token().stop_requested());
#else
      assert(false == m_stopToken.load(std::memory_order_relaxed));
#endif
      [[maybe_unused]] std::scoped_lock const guard(m_tasksQueueLock);
      assert(
         std::end(m_tasksQueue) == std::find_if(
            std::begin(m_tasksQueue),
            std::end(m_tasksQueue),
            [&connection] (auto const &task)
            {
               return (connection == task.connection) && (task_action::send == task.action);
            }
         )
      );
      m_tasksQueue.emplace_back(task{.connection = connection, .action = task_action::send});
   }

   void remove_connection(std::shared_ptr<thread::connection> const &connection) noexcept
   {
      assert(nullptr != connection);
#if (201911L <= __cpp_lib_jthread)
      assert(false == m_thread->get_stop_token().stop_requested());
#else
      assert(false == m_stopToken.load(std::memory_order_relaxed));
#endif
      [[maybe_unused]] std::scoped_lock const guard(m_tasksQueueLock);
      assert(
         std::end(m_tasksQueue) == std::find_if(
            std::begin(m_tasksQueue),
            std::end(m_tasksQueue),
            [&connection] (auto const &task)
            {
               return (connection == task.connection) && (task_action::remove == task.action);
            }
         )
      );
      m_tasksQueue.emplace_back(task{.connection = connection, .action = task_action::remove});
   }

   std::pair<char *, size_t> websocket_buffer() noexcept
   {
      return {m_websocketBuffer.data(), m_websocketBuffer.size()};
   }

private:
   spin_lock m_tasksQueueLock = {};
   std::vector<task> m_tasksQueue = {};
#if (201911L <= __cpp_lib_jthread)
   std::unique_ptr<std::jthread> m_thread = {};
#else
   std::atomic_bool m_stopToken = false;
   std::unique_ptr<std::thread> m_thread = {};
#endif
   std::string m_websocketBuffer = {};

   void swap_task_queue(std::vector<task> &tasksQueue)
   {
      [[maybe_unused]] std::scoped_lock const guard(m_tasksQueueLock);
      tasksQueue.swap(m_tasksQueue);
   }

#if (201911L <= __cpp_lib_jthread)
   void thread_handler(std::stop_token stopToken, thread_config const &config)
#else
   void thread_handler(thread_config const &config)
#endif
   {
      set_thread_affinity(config.cpu_affinity());

      static global_context const globalContext = {};

      auto eventContext = init_event_context(config.initial_connections_capacity());
      event eventTimer = {};
      EXPECT_ERROR_CODE(0, evtimer_assign(std::addressof(eventTimer), std::addressof(eventContext.eventBase), &connection::event_timer_callback, std::addressof(eventContext)));
      EXPECT_ERROR_CODE(CURLM_OK, curl_multi_setopt(eventContext.multiHandle, CURLMOPT_SOCKETDATA, std::addressof(eventContext)));
      EXPECT_ERROR_CODE(CURLM_OK, curl_multi_setopt(eventContext.multiHandle, CURLMOPT_SOCKETFUNCTION, &connection::curl_socket_callback));
      EXPECT_ERROR_CODE(CURLM_OK, curl_multi_setopt(eventContext.multiHandle, CURLMOPT_TIMERDATA, std::addressof(eventTimer)));
      EXPECT_ERROR_CODE(CURLM_OK, curl_multi_setopt(eventContext.multiHandle, CURLMOPT_TIMERFUNCTION, &worker::curl_timer_callback));

      std::vector<task> tasksQueue;
      tasksQueue.reserve(config.initial_connections_capacity());
#if (201911L <= __cpp_lib_jthread)
      while (false == stopToken.stop_requested())
#else
      while (false == m_stopToken.load(std::memory_order_relaxed))
#endif
      {
         [[maybe_unused]] auto const eventBaseLoopRetCode = event_base_loop(std::addressof(eventContext.eventBase), EVLOOP_NONBLOCK);
         assert(0 <= eventBaseLoopRetCode);

         swap_task_queue(tasksQueue);
         for (auto &task : tasksQueue)
         {
            switch (task.action)
            {
            case task_action::add:
            {
               task.connection->register_handle(eventContext);
            }
            break;

            case task_action::send:
            {
               task.connection->send_message(eventContext);
            }
            break;

            case task_action::remove:
            {
               task.connection->unregister_handle(eventContext);
            }
            break;
            }
         }
         tasksQueue.clear();

         connection *timedoutConnection = nullptr;
         while (nullptr != (timedoutConnection = eventContext.timerQueue.pop(std::chrono::system_clock::now())))
         {
            timedoutConnection->tick(eventContext);
         }
      }

      EXPECT_ERROR_CODE(CURLM_OK, curl_multi_setopt(eventContext.multiHandle, CURLMOPT_SOCKETDATA, nullptr));
      EXPECT_ERROR_CODE(CURLM_OK, curl_multi_setopt(eventContext.multiHandle, CURLMOPT_SOCKETFUNCTION, nullptr));
      EXPECT_ERROR_CODE(CURLM_OK, curl_multi_setopt(eventContext.multiHandle, CURLMOPT_TIMERDATA, nullptr));
      EXPECT_ERROR_CODE(CURLM_OK, curl_multi_setopt(eventContext.multiHandle, CURLMOPT_TIMERFUNCTION, nullptr));
      evtimer_del(std::addressof(eventTimer));
      free_event_context(eventContext);
   }

   static void curl_timer_callback(CURLM const *, long const timeoutMs, void *userdata)
   {
      assert(nullptr != userdata);
      auto *eventTimeout = static_cast<event *>(userdata);
      EXPECT_ERROR_CODE(0, evtimer_del(eventTimeout));
      if (0 <= timeoutMs)
      {
         auto const timeout = timeval
         {
            .tv_sec = static_cast<decltype(timeval::tv_sec)>(timeoutMs / 1000),
            .tv_usec = static_cast<decltype(timeval::tv_usec)>((timeoutMs % 1000) * 1000)
         };
         EXPECT_ERROR_CODE(0, evtimer_add(eventTimeout, std::addressof(timeout)));
      }
   }

   [[nodiscard]] static event_base &init_event_base()
   {
      auto *eventConfig = event_config_new();
      assert(nullptr != eventConfig);
      EXPECT_ERROR_CODE(0, event_config_set_flag(eventConfig, EVENT_BASE_FLAG_NOLOCK));
      auto *eventBase = event_base_new_with_config(eventConfig);
      assert(nullptr != eventBase);
      event_config_free(eventConfig);
      eventConfig = nullptr;
      return *eventBase;
   }

   [[nodiscard]] static event_context init_event_context(size_t const initialSocketEventsCapacity)
   {
      event_context eventContext
      {
         .eventBase = init_event_base(),
         .multiHandle = curl_multi_init(),
         .freeSocketEvents = nullptr,
         .timerQueue = timer_queue{initialSocketEventsCapacity},
         .allSocketEvents = {},
      };
      assert(nullptr != eventContext.multiHandle);
      for (size_t socketEventsCount = 0; socketEventsCount < initialSocketEventsCapacity; ++socketEventsCount)
      {
         auto &socketEvent = eventContext.allSocketEvents.emplace_back();
         socketEvent.eventOrNext = eventContext.freeSocketEvents;
         eventContext.freeSocketEvents = std::addressof(socketEvent);
      }
      return eventContext;
   }

   static void free_event_context(event_context &eventContext)
   {
#if (not defined(NDEBUG))
      size_t socketEventsCount = 0;
#endif
      while (nullptr != eventContext.freeSocketEvents)
      {
         auto *socketEvent = eventContext.freeSocketEvents;
         eventContext.freeSocketEvents = std::get<socket_event *>(socketEvent->eventOrNext);
#if (not defined(NDEBUG))
         ++socketEventsCount;
#endif
      }
      assert(eventContext.allSocketEvents.size() == socketEventsCount);
      EXPECT_ERROR_CODE(CURLM_OK, curl_multi_cleanup(eventContext.multiHandle));
      eventContext.multiHandle = nullptr;
      event_base_free(std::addressof(eventContext.eventBase));
   }
};

namespace
{

std::string_view trim(std::string_view const value) noexcept
{
   size_t trimLeftIndex = value.find_first_not_of(" \t\r\n");
   if (std::string_view::npos == trimLeftIndex)
   {
      return {};
   }
   size_t trimRightIndex = value.find_last_not_of(" \t\r\n");
   return value.substr(trimLeftIndex, trimRightIndex - trimLeftIndex + 1);
}

std::pair<std::string_view, std::string_view> split_http_header(std::string_view const header) noexcept
{
   auto const delimiterIndex = header.find(':');
   if (std::string_view::npos == delimiterIndex)
   {
      return {std::string_view{}, std::string_view{}};
   }
   return {trim(header.substr(0, delimiterIndex)), trim(header.substr(delimiterIndex + 1))};
}

}

class [[nodiscard("discarded-value expression detected")]] curl_config final
{
public:
   curl_config() = delete;
   curl_config(curl_config &&) = delete;
   curl_config(curl_config const &) = delete;

   curl_config(
      std::string_view const &scheme,
      peer_address const &address,
      std::optional<peer_address> const &realAddress,
      net_interface const &netInterface,
      std::chrono::seconds const keepAliveDelay,
      std::optional<ssl_certificate> const &sslCertificate
   ) :
      m_keepAliveDelay(keepAliveDelay)
   {
      assert(false == scheme.empty());
      assert(false == address.host.empty());
      assert(std::chrono::seconds::zero() < keepAliveDelay);
      m_url.assign(scheme).append("://").append(address.host);
      if (0 != address.port)
      {
         m_url.append(":").append(std::to_string(address.port));
      }
      if (true == realAddress.has_value())
      {
         m_connectToStr.assign(address.host).append(":");
         if (0 != address.port)
         {
            m_connectToStr.append(std::to_string(address.port));
         }
         m_connectToStr.append(":").append(realAddress.value().host).append(":");
         if (0 != realAddress.value().port)
         {
            m_connectToStr.append(std::to_string(realAddress.value().port));
         }
         m_connectTo.data = m_connectToStr.data();
      }
      if (true == netInterface.name.has_value())
      {
         if (true == netInterface.host.has_value())
         {
            m_interface.assign("ifhost!").append(netInterface.name.value()).append("!").append(netInterface.host.value());
         }
         else
         {
            m_interface.assign("if!").append(netInterface.name.value());
         }
      }
      else if (true == netInterface.host.has_value())
      {
         m_interface.assign("host!").append(netInterface.host.value());
      }
      if (true == sslCertificate.has_value())
      {
         assert((false == sslCertificate.value().authorityInfo.empty()) || (false == sslCertificate.value().certificate.empty()));
         m_sslCertificateAuthority.assign(sslCertificate.value().authorityInfo);
         m_sslCertificate.assign(sslCertificate.value().certificate);
         m_sslCertificateType = sslCertificate.value().type;
      }
   }

   curl_config &operator = (curl_config &&) = delete;
   curl_config &operator = (curl_config const &) = delete;

   template<typename report_error_callback>
   [[nodiscard]] bool apply(CURL *handle, report_error_callback const &reportError)
   {
      return
      (
         (true == setup_address(handle, reportError)) &&
         (true == setup_keep_alive(handle, reportError)) &&
         (true == setup_tls(handle, reportError))
      );
   }

private:
   std::string m_url = {};
   curl_slist m_connectTo = {.data = nullptr, .next = nullptr};
   std::string m_connectToStr = {};
   std::string m_interface = {};
   std::chrono::seconds m_keepAliveDelay;
   std::string m_sslCertificateAuthority = {};
   std::string m_sslCertificate = {};
   ssl_certificate_type m_sslCertificateType = ssl_certificate_type::pem;

   template<typename report_error_callback>
   [[nodiscard]] bool setup_address(CURL *handle, report_error_callback const &reportError) const
   {
      assert(nullptr != handle);
      assert(false == m_url.empty());
      return
      (
         (connection_status::initializing == reportError(curl_easy_setopt(handle, CURLOPT_URL, m_url.c_str()))) &&
         (
            (nullptr == m_connectTo.data) ||
            (connection_status::initializing == reportError(curl_easy_setopt(handle, CURLOPT_CONNECT_TO, std::addressof(m_connectTo))))
         ) &&
         (
            (true == m_interface.empty()) ||
            (connection_status::initializing == reportError(curl_easy_setopt(handle, CURLOPT_INTERFACE, m_interface.c_str())))
         )
      );
   }

   template<typename report_error_callback>
   [[nodiscard]] bool setup_keep_alive(CURL *handle, report_error_callback const &reportError) const
   {
      return
      (
         (connection_status::initializing == reportError(curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L))) &&
         (connection_status::initializing == reportError(curl_easy_setopt(handle, CURLOPT_TCP_KEEPCNT, 1L))) &&
         (connection_status::initializing == reportError(curl_easy_setopt(handle, CURLOPT_TCP_KEEPIDLE, static_cast<long>(m_keepAliveDelay.count())))) &&
         (connection_status::initializing == reportError(curl_easy_setopt(handle, CURLOPT_TCP_KEEPINTVL, 1L)))
      );
   }

   template<typename report_error_callback>
   [[nodiscard]] bool setup_tls(CURL *handle, report_error_callback const &reportError)
   {
      assert(nullptr != handle);
      auto const sslCertificateAuthorityBlob = curl_blob
      {
         .data = m_sslCertificateAuthority.data(),
         .len = m_sslCertificateAuthority.size(),
         .flags = CURL_BLOB_NOCOPY,
      };
      auto const sslCertificateBlob = curl_blob
      {
         .data = m_sslCertificate.data(),
         .len = m_sslCertificate.size(),
         .flags = CURL_BLOB_NOCOPY
      };
      return
      (
#if (defined(WEBTHREAD_TLSv1_3))
         (connection_status::initializing == reportError(curl_easy_setopt(handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_MAX_TLSv1_3))) &&
#else
         (connection_status::initializing == reportError(curl_easy_setopt(handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_MAX_TLSv1_2))) &&
#endif
         (
            (true == m_sslCertificateAuthority.empty()) ||
            (connection_status::initializing == reportError(curl_easy_setopt(handle, CURLOPT_CAINFO_BLOB, std::addressof(sslCertificateAuthorityBlob))))
         ) &&
         (
            (true == m_sslCertificate.empty()) ||
            (
               (connection_status::initializing == reportError(curl_easy_setopt(handle, CURLOPT_SSLCERTTYPE, convert_ssl_certificate_type(m_sslCertificateType)))) &&
               (connection_status::initializing == reportError(curl_easy_setopt(handle, CURLOPT_SSLCERT_BLOB, std::addressof(sslCertificateBlob))))
            )
         )
      );
   }

   [[nodiscard]] static char const *convert_ssl_certificate_type(ssl_certificate_type const sslCertificatType) noexcept
   {
      switch (sslCertificatType)
      {
#if (201907L <= __cpp_using_enum)
      using enum ssl_certificate_type;
      case der: return "DER";
      case p12: return "P12";
      case pem: return "PEM";
#else
      case ssl_certificate_type::der: return "DER";
      case ssl_certificate_type::p12: return "P12";
      case ssl_certificate_type::pem: return "PEM";
#endif
      default: return nullptr;
      }
   }
};

class [[nodiscard("discarded-value expression detected")]] thread::rest_connection final : public thread::connection
{
private:
   struct [[nodiscard("discarded-value expression detected")]] request final
   {
      rest_method method = rest_method::get;
      std::string_view customMethod = {};
      std::string_view path = {};
      std::string_view query = {};
      curl_slist *headers = nullptr;
      std::string_view body = {};
      size_t bodyOffset = 0;
      std::chrono::milliseconds timeout = default_request_timeout;
   };

public:
   rest_connection() = delete;
   rest_connection(rest_connection &&) = delete;
   rest_connection(rest_connection const &) = delete;

   rest_connection(
      std::shared_ptr<rest_client_listener> const &listener,
      std::unique_ptr<curl_config> config
   ) :
      connection(),
      m_listener(listener),
      m_config(std::move(config))
   {
      /// context: client thread
      assert(nullptr != m_listener);
      assert(nullptr != m_config);
      assert(nullptr != m_request);
   }

   ~rest_connection() override
   {
      assert(nullptr == m_handle);
      assert(nullptr != m_request);
      assert(rest_method::get == m_request->method);
      assert(true == m_request->customMethod.empty());
      assert(true == m_request->path.empty());
      assert(true == m_request->query.empty());
      assert(nullptr == m_request->headers);
      assert(true == m_request->body.empty());
      assert(0 == m_request->bodyOffset);
#if (not defined(NDEBUG))
      size_t nodesCount = 0;
      while (nullptr != m_freeHead)
      {
         ++nodesCount;
         auto *nextFreeHead = m_freeHead->next;
         assert(nullptr == m_freeHead->data);
         m_freeHead->next = nullptr;
         m_freeHead = nextFreeHead;
      }
      assert(m_allNodes.size() == nodesCount);
#endif
   }

   rest_connection &operator = (rest_connection &&) = delete;
   rest_connection &operator = (rest_connection const &) = delete;

   void add_request_header(std::string_view const header)
   {
      /// context: client thread
      assert(false == header.empty());
      assert(nullptr != m_request);
      auto *headerNode = pop_free_node();
      assert(nullptr != headerNode);
      assert(nullptr == headerNode->data);
      assert(nullptr == headerNode->next);
      headerNode->data = store_header(header);
      headerNode->next = m_request->headers;
      m_request->headers = headerNode;
   }

   void add_request_header(std::string_view const headerName, std::string_view const headerValue)
   {
      /// context: client thread
      assert(false == headerName.empty());
      assert(false == headerValue.empty());
      assert(nullptr != m_request);
      auto *headerNode = pop_free_node();
      assert(nullptr != headerNode);
      assert(nullptr == headerNode->data);
      assert(nullptr == headerNode->next);
      headerNode->data = store_header(headerName, headerValue);
      headerNode->next = m_request->headers;
      m_request->headers = headerNode;
   }

   void register_handle(event_context &) override
   {
      /// context: worker thread
      assert(nullptr == m_handle);
      assert(nullptr != m_config);
      assert(connection_status::initializing == m_status);
      m_handle = curl_easy_init();
      assert(nullptr != m_handle);
      if (
         (true == m_config->apply(m_handle, [this] (auto const errorCode) { return report_error(errorCode); })) &&
         (true == setup_callbacks())
      )
      {
         m_status = connection_status::ready;
      }
   }

   void send_message(event_context &eventContext) override
   {
      /// context: worker thread
      assert(nullptr != eventContext.multiHandle);
      assert(nullptr != m_request);
      assert(connection_status::ready == m_status);
      if (true == setup_request())
      {
         m_status = connection_status::busy;
         report_error(curl_multi_add_handle(eventContext.multiHandle, m_handle));
      }
   }

   void set_request_body(std::string_view const body, std::string_view const contentType)
   {
      /// context: client thread
      assert(false == body.empty());
      assert(false == contentType.empty());
      assert(nullptr != m_request);
      assert(rest_method::get != m_request->method);
      assert(true == m_request->body.empty());
      m_request->body = body;
      add_request_header("Content-Type", contentType);
   }

   void set_request_method(rest_method const method, std::string_view const customMethod) noexcept
   {
      /// context: client thread
      assert((rest_method::custom != method) || (false == customMethod.empty()));
      assert(nullptr != m_request);
      assert(rest_method::get == m_request->method);
      assert(true == m_request->customMethod.empty());
      assert(true == m_request->path.empty());
      assert(true == m_request->query.empty());
      assert(nullptr == m_request->headers);
      assert(true == m_request->body.empty());
      assert(0 == m_request->bodyOffset);
      m_request->method = method;
      m_request->customMethod = customMethod;
   }

   void set_request_path(std::string_view const path) noexcept
   {
      /// context: client thread
      assert(false == path.empty());
      assert(nullptr != m_request);
      assert(true == m_request->path.empty());
      m_request->path = path;
   }

   void set_request_query(std::string_view const query) noexcept
   {
      /// context: client thread
      assert(false == query.empty());
      assert(nullptr != m_request);
      assert(true == m_request->query.empty());
      m_request->query = query;
   }

   void set_request_timeout(std::chrono::milliseconds const timeout) noexcept
   {
      /// context: client thread
      assert(std::chrono::milliseconds::zero() != timeout);
      assert(nullptr != m_request);
      assert(default_request_timeout == m_request->timeout);
      m_request->timeout = timeout;
   }

   [[noreturn]] void tick(event_context &) override
   {
      /// context: worker thread
      assert(false && "Should never be called");
#if (202202L <= __cpp_lib_unreachable)
      std::unreachable();
#else
      std::abort();
#endif
   }

   void unregister_handle(event_context &) override
   {
      /// context: worker thread
      assert(connection_status::busy != m_status);
      if (nullptr != m_handle)
      {
         deactivate();
      }
   }

private:
   CURL *m_handle = nullptr;
   std::shared_ptr<rest_client_listener> m_listener;
   std::unique_ptr<curl_config> m_config;
   std::string m_requestTarget = {};
   std::unique_ptr<request> m_request = std::make_unique<request>();
   curl_slist *m_freeHead = nullptr;
   std::deque<curl_slist> m_allNodes = {};
   std::deque<std::string> m_allValues = {};
   size_t m_lastValueIndex = 0;
   connection_status m_status = connection_status::initializing;

   void deactivate()
   {
      /// context: worker thread
      reset_request();
      curl_easy_cleanup(m_handle);
      m_handle = nullptr;
      m_status = connection_status::inactive;
   }

   void on_recv_message(event_context &eventContext, CURLMsg const &message) override
   {
      /// context: worker thread
      assert(nullptr != eventContext.multiHandle);
      assert(nullptr != message.easy_handle);
      assert(message.easy_handle == m_handle);
      report_error(curl_multi_remove_handle(eventContext.multiHandle, m_handle));
      assert(connection_status::busy == m_status);
      if (connection_status::busy == report_error(message.data.result))
      {
         auto responseCode = 0L;
         EXPECT_ERROR_CODE(connection_status::busy, report_error(curl_easy_getinfo(m_handle, CURLINFO_RESPONSE_CODE, &responseCode)));
         reset_request();
         m_status = connection_status::ready;
         m_listener->on_response_complete(static_cast<intptr_t>(responseCode));
      }
   }

   void on_attach_socket(curl_socket_t const) override
   {
      /// context: worker thread
   }

   void on_detach_socket(curl_socket_t const) override
   {
      /// context: worker thread
   }

   [[nodiscard]] curl_slist *pop_free_node()
   {
      /// context: client thread
      if (nullptr == m_freeHead)
      {
         return std::addressof(m_allNodes.emplace_back(curl_slist{.data = nullptr, .next = nullptr}));
      }
      auto *freeNode = m_freeHead;
      m_freeHead = m_freeHead->next;
      freeNode->next = nullptr;
      return freeNode;
   }

   void push_free_nodes(curl_slist *head) noexcept
   {
      /// context: worker thread
      while (nullptr != head)
      {
         auto *node = head;
#if (not defined(NDEBUG))
         node->data = nullptr;
#endif
         head = head->next;
         node->next = m_freeHead;
         m_freeHead = node;
      }
   }

   [[nodiscard]] connection_status report_error(CURLcode const errorCode)
   {
      /// context: worker thread
      assert(connection_status::inactive != m_status);
      if (CURLE_OK == errorCode) [[likely]]
      {
         return m_status;
      }
      deactivate();
      m_listener->on_error(static_cast<intptr_t>(errorCode), curl_easy_strerror(errorCode));
      return m_status;
   }

   void report_error(CURLMcode const errorCode)
   {
      /// context: worker thread
      assert(connection_status::inactive != m_status);
      if (CURLM_OK == errorCode) [[likely]]
      {
         return;
      }
      deactivate();
      m_listener->on_error(static_cast<intptr_t>(errorCode), curl_multi_strerror(errorCode));
   }

   void reset_request()
   {
      /// context: worker thread
      assert(nullptr != m_handle);
      assert(nullptr != m_request);
      if (rest_method::custom == m_request->method)
      {
         EXPECT_ERROR_CODE(m_status, report_error(curl_easy_setopt(m_handle, CURLOPT_CUSTOMREQUEST, nullptr)));
      }
      EXPECT_ERROR_CODE(m_status, report_error(curl_easy_setopt(m_handle, CURLOPT_REQUEST_TARGET, nullptr)));
      m_requestTarget.clear();
      EXPECT_ERROR_CODE(m_status, report_error(curl_easy_setopt(m_handle, CURLOPT_HTTPHEADER, nullptr)));
      push_free_nodes(m_request->headers);
      *m_request = request{};
      m_lastValueIndex = 0;
   }

   [[nodiscard]] bool setup_callbacks()
   {
      /// context: worker thread
      assert(nullptr != m_handle);
      assert(connection_status::initializing == m_status);
      return
      (
         (connection_status::initializing == report_error(curl_easy_setopt(m_handle, CURLOPT_HEADERDATA, this))) &&
         (connection_status::initializing == report_error(curl_easy_setopt(m_handle, CURLOPT_HEADERFUNCTION, &header_callback))) &&
         (connection_status::initializing == report_error(curl_easy_setopt(m_handle, CURLOPT_PRIVATE, this))) &&
         (connection_status::initializing == report_error(curl_easy_setopt(m_handle, CURLOPT_READDATA, this))) &&
         (connection_status::initializing == report_error(curl_easy_setopt(m_handle, CURLOPT_READFUNCTION, &read_callback))) &&
         (connection_status::initializing == report_error(curl_easy_setopt(m_handle, CURLOPT_WRITEDATA, this))) &&
         (connection_status::initializing == report_error(curl_easy_setopt(m_handle, CURLOPT_WRITEFUNCTION, &write_callback)))
      );
   }

   [[nodiscard]] bool setup_request()
   {
      /// context: worker thread
      assert(nullptr != m_handle);
      assert(nullptr != m_request);
      assert(true == m_requestTarget.empty());
      assert(connection_status::ready == m_status);
      if ((true == m_request->path.empty()) || ('/' != m_request->path.at(0)))
      {
         m_requestTarget.assign("/");
      }
      m_requestTarget.append(m_request->path);
      if (false == m_request->query.empty())
      {
         if ('?' != m_request->query.at(0))
         {
            m_requestTarget.append("?");
         }
         m_requestTarget.append(m_request->query);
      }
      return
      (
         (connection_status::ready == report_error(curl_easy_setopt(m_handle, CURLOPT_REQUEST_TARGET, m_requestTarget.c_str()))) &&
         (connection_status::ready == report_error(curl_easy_setopt(m_handle, CURLOPT_HTTPHEADER, m_request->headers))) &&
         (connection_status::ready == report_error(curl_easy_setopt(m_handle, CURLOPT_TIMEOUT_MS, static_cast<long>(m_request->timeout.count()))))
      ) && setup_request_method();
   }

   [[nodiscard]] bool setup_request_method()
   {
      /// context: worker thread
      assert(nullptr != m_handle);
      assert(nullptr != m_request);
      assert(connection_status::ready == m_status);
      switch (m_request->method)
      {
      case rest_method::custom: return
      (
         (connection_status::ready == report_error(curl_easy_setopt(m_handle, CURLOPT_CUSTOMREQUEST, m_request->customMethod.data()))) &&
         (connection_status::ready == report_error(curl_easy_setopt(m_handle, CURLOPT_POST, 1L))) &&
         (connection_status::ready == report_error(curl_easy_setopt(m_handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(m_request->body.size()))))
      );

      case rest_method::get: return (connection_status::ready == report_error(curl_easy_setopt(m_handle, CURLOPT_HTTPGET, 1L)));

      case rest_method::post: return
      (
         (connection_status::ready == report_error(curl_easy_setopt(m_handle, CURLOPT_POST, 1L))) &&
         (connection_status::ready == report_error(curl_easy_setopt(m_handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(m_request->body.size()))))
      );

      case rest_method::put: return
      (
         (connection_status::ready == report_error(curl_easy_setopt(m_handle, CURLOPT_INFILESIZE, static_cast<long>(m_request->body.size())))) &&
         (connection_status::ready == report_error(curl_easy_setopt(m_handle, CURLOPT_UPLOAD, 1L)))
      );

      default:
      {
#if (202202L <= __cpp_lib_unreachable)
         std::unreachable();
#else
         std::abort();
#endif
      }
      }
   }

   char *store_header(std::string_view const header)
   {
      /// context: client thread
      assert(false == header.empty());
      if (m_allValues.size() == m_lastValueIndex)
      {
         ++m_lastValueIndex;
         return m_allValues.emplace_back(header).data();
      }
      return m_allValues[m_lastValueIndex++].assign(header).data();
   }

   char *store_header(std::string_view const headerName, std::string_view const headerValue)
   {
      /// context: client thread
      assert(false == headerName.empty());
      auto &header = (m_allValues.size() == m_lastValueIndex) ? m_allValues.emplace_back() : m_allValues[m_lastValueIndex];
      ++m_lastValueIndex;
      header.reserve(headerName.size() + headerValue.size() + 2);
      return header.assign(headerName).append(":").append(headerValue).data();
   }

   static size_t header_callback(char const *buffer, size_t const size, size_t const nitems, void *userdata)
   {
      /// context: worker thread
      assert(nullptr != userdata);
      auto *self = static_cast<rest_connection *>(userdata);
      assert(connection_status::busy == self->m_status);
      auto const bytes = size * nitems;
      if (
         auto const [headerName, headerValue] = split_http_header(std::string_view{buffer, bytes});
         false == headerName.empty()
      )
      {
         self->m_listener->on_response_header(headerName, headerValue);
      }
      return bytes;
   }

   static size_t read_callback(char *buffer, size_t const size, size_t const nitems, void *userdata)
   {
      /// context: worker thread
      assert(nullptr != userdata);
      auto *self = static_cast<rest_connection *>(userdata);
      assert(nullptr != self->m_request);
      assert(connection_status::busy == self->m_status);
      auto &request = *self->m_request;
      assert(request.bodyOffset <= request.body.size());
      auto const bytesToSend = request.body.size() - request.bodyOffset;
      auto const writtenBytes = std::min<size_t>(size * nitems, bytesToSend);
      std::memcpy(buffer, request.body.data() + request.bodyOffset, writtenBytes);
      request.bodyOffset += writtenBytes;
      return writtenBytes;
   }

   static size_t write_callback(char const *buffer, size_t const size, size_t const nmemb, void *userdata)
   {
      /// context: worker thread
      assert(nullptr != userdata);
      auto *self = static_cast<rest_connection *>(userdata);
      assert(connection_status::busy == self->m_status);
      auto const bytes = size * nmemb;
      self->m_listener->on_response_body(std::string_view{buffer, bytes});
      return bytes;
   }
};

thread::rest_request::rest_request(rest_request &&) noexcept = default;

thread::rest_request::rest_request(
   std::shared_ptr<worker> const &worker,
   std::shared_ptr<rest_connection> const &connection,
   rest_method const method,
   std::string_view const customMethod
) noexcept :
   m_connection(connection),
   m_worker(worker)
{
   assert(nullptr != m_connection);
   assert(nullptr != m_worker);
   m_connection->set_request_method(method, customMethod);
}

thread::rest_request::~rest_request() noexcept
{
   assert(nullptr == m_connection);
   assert(nullptr == m_worker);
}

void thread::rest_request::perform() noexcept
{
   assert(nullptr != m_connection);
   assert(nullptr != m_worker);
   m_worker->enqueue_request(m_connection);
   m_connection.reset();
   m_worker.reset();
}

thread::rest_request &thread::rest_request::with_body(std::string_view const body, std::string_view const contentType)
{
   assert(nullptr != m_connection);
   assert(nullptr != m_worker);
   m_connection->set_request_body(body, contentType);
   return *this;
}

thread::rest_request &thread::rest_request::with_header(std::string_view const header)
{
   assert(nullptr != m_connection);
   assert(nullptr != m_worker);
   m_connection->add_request_header(header);
   return *this;
}

thread::rest_request &thread::rest_request::with_header(std::string_view const headerName, std::string_view const headerValue)
{
   assert(nullptr != m_connection);
   assert(nullptr != m_worker);
   m_connection->add_request_header(headerName, headerValue);
   return *this;
}

thread::rest_request &thread::rest_request::with_path(std::string_view const path) noexcept
{
   assert(nullptr != m_connection);
   assert(nullptr != m_worker);
   m_connection->set_request_path(path);
   return *this;
}

thread::rest_request &thread::rest_request::with_query(std::string_view const query) noexcept
{
   assert(nullptr != m_connection);
   assert(nullptr != m_worker);
   m_connection->set_request_query(query);
   return *this;
}

thread::rest_request &thread::rest_request::with_timeout(std::chrono::milliseconds const timeout) noexcept
{
   assert(nullptr != m_connection);
   assert(nullptr != m_worker);
   m_connection->set_request_timeout(timeout);
   return *this;
}

thread::rest_client::rest_client() noexcept = default;

thread::rest_client::rest_client(rest_client &&rhs) noexcept = default;

thread::rest_client::rest_client(thread const &thread, std::shared_ptr<rest_client_listener> const &listener, http_config const &httpConfig) :
   m_worker(thread.m_worker),
   m_connection(
      std::make_shared<rest_connection>(
         listener,
         std::make_unique<curl_config>(
            "http",
            httpConfig.peer_address(),
            std::nullopt,
            httpConfig.net_interface(),
            httpConfig.keep_alive_delay(),
            std::nullopt
         )
      )
   )
{
   m_worker->add_connection(m_connection);
}

thread::rest_client::rest_client(thread const &thread, std::shared_ptr<rest_client_listener> const &listener, https_config const &httpsConfig) :
   m_worker(thread.m_worker),
   m_connection(
      std::make_shared<rest_connection>(
         listener,
         std::make_unique<curl_config>(
            "https",
            httpsConfig.peer_address(),
            httpsConfig.peer_real_address(),
            httpsConfig.net_interface(),
            httpsConfig.keep_alive_delay(),
            httpsConfig.ssl_certificate()
         )
      )
   )
{
   m_worker->add_connection(m_connection);
}

thread::rest_client::~rest_client() noexcept
{
   if (nullptr != m_connection)
   {
      assert(nullptr != m_worker);
      m_worker->remove_connection(m_connection);
   }
}

thread::rest_client &thread::rest_client::operator = (rest_client &&rhs) noexcept = default;

thread::rest_request thread::rest_client::custom_request(std::string_view const method) const noexcept
{
   assert(false == method.empty());
   assert(nullptr != m_worker);
   assert(nullptr != m_connection);
   return thread::rest_request{m_worker, m_connection, rest_method::custom, method};
}

thread::rest_request thread::rest_client::get_request() const noexcept
{
   assert(nullptr != m_worker);
   assert(nullptr != m_connection);
   return thread::rest_request{m_worker, m_connection, rest_method::get, {}};
}

thread::rest_request thread::rest_client::post_request() const noexcept
{
   assert(nullptr != m_worker);
   assert(nullptr != m_connection);
   return thread::rest_request{m_worker, m_connection, rest_method::post, {}};
}

thread::rest_request thread::rest_client::upload_request() const noexcept
{
   assert(nullptr != m_worker);
   assert(nullptr != m_connection);
   return thread::rest_request{m_worker, m_connection, rest_method::put, {}};
}

class [[nodiscard("discarded-value expression detected")]] thread::websocket_connection final : public thread::connection
{
public:
   websocket_connection() = delete;
   websocket_connection(websocket_connection &&) = delete;
   websocket_connection(websocket_connection const &) = delete;

   websocket_connection(
      std::shared_ptr<websocket_listener> const &listener,
      std::unique_ptr<curl_config> config,
      std::string_view const urlPath,
      std::chrono::milliseconds const connectTimeout,
      std::chrono::milliseconds const tickTimeout,
      std::pair<char *, size_t> inboundMessageBuffer
   ) :
      connection(),
      m_listener(listener),
      m_config(std::move(config)),
      m_tickTimeout(tickTimeout),
      m_connectTimeout(connectTimeout),
      m_inboundMessageBuffer(inboundMessageBuffer)
   {
      /// context: client thread
      assert(nullptr != m_listener);
      assert(nullptr != m_config);
      assert(std::chrono::milliseconds::zero() <= m_tickTimeout);
      assert(std::chrono::milliseconds::zero() < m_connectTimeout);
      if ((true == urlPath.empty()) || ('/' != urlPath.at(0)))
      {
         m_urlPath.assign("/");
      }
      m_urlPath.append(urlPath);
   }

   ~websocket_connection() override
   {
      assert(nullptr == m_multiHandle);
      assert(nullptr == m_handle);
      assert(nullptr == m_socketEvent);
      assert(CURL_SOCKET_BAD == m_resolverSocket);
      assert(CURL_SOCKET_BAD == m_dataSocket);
      assert(connection_status::inactive == m_status);
   }

   websocket_connection &operator = (websocket_connection &&) = delete;
   websocket_connection &operator = (websocket_connection const &) = delete;

   void register_handle(event_context &eventContext) override
   {
      /// context: worker thread
      assert(nullptr != eventContext.multiHandle);
      assert(nullptr == m_multiHandle);
      assert(nullptr == m_handle);
      assert(nullptr != m_config);
      assert(connection_status::ready != m_status);
      assert(connection_status::busy != m_status);
      assert(false == m_outboundMessageReady);
      m_multiHandle = eventContext.multiHandle;
      m_handle = curl_easy_init();
      m_status = connection_status::initializing;
      assert(nullptr != m_handle);
      if (
         (true == m_config->apply(m_handle, [this] (auto const errorCode) { return report_error(errorCode); })) &&
         (connection_status::initializing == report_error(curl_easy_setopt(m_handle, CURLOPT_CONNECT_ONLY, 2L))) &&
         (connection_status::initializing == report_error(curl_easy_setopt(m_handle, CURLOPT_PRIVATE, this))) &&
         (connection_status::initializing == report_error(curl_easy_setopt(m_handle, CURLOPT_REQUEST_TARGET, m_urlPath.c_str()))) &&
         (connection_status::initializing == report_error(curl_easy_setopt(m_handle, CURLOPT_TIMEOUT_MS, static_cast<long>(m_connectTimeout.count()))))
      )
      {
         report_error(curl_multi_add_handle(m_multiHandle, m_handle));
      }
   }

   void send_message(event_context &eventContext) override
   {
      /// context: worker thread
      assert(nullptr != eventContext.multiHandle);
      assert((nullptr == m_multiHandle) || (m_multiHandle == eventContext.multiHandle));
      assert((nullptr == m_multiHandle) == (nullptr == m_handle));
      assert(connection_status::busy != m_status);
      if (connection_status::ready == m_status) [[likely]]
      {
         assert(nullptr != m_handle);
         assert(false == m_outboundMessage.empty());
         event_assign_callback(eventContext.eventBase, EV_WRITE);
      }
      else if (connection_status::initializing == m_status)
      {
         m_outboundMessageReady = true;
      }
   }

   void set_outbound_message(std::string_view const outboundMessage)
   {
      /// context: client thread
      assert(false == outboundMessage.empty());
      assert(true == m_outboundMessage.empty());
      assert(false == m_outboundMessageReady);
      m_outboundMessage.assign(outboundMessage);
   }

   void tick(event_context &eventContext) override
   {
      /// context: worker thread
      assert(nullptr != eventContext.multiHandle);
      assert(std::chrono::milliseconds::zero() < m_tickTimeout);
      if (connection_status::inactive != m_status)
      {
         assert(nullptr != m_multiHandle);
         assert(m_multiHandle == eventContext.multiHandle);
         assert(nullptr != m_handle);
         assert(nullptr != m_socketEvent);
         assert(connection_status::initializing != m_status);
         m_listener->on_tick();
         eventContext.timerQueue.push(std::chrono::system_clock::now() + m_tickTimeout, *this);
      }
   }

   void unregister_handle(event_context &eventContext) override
   {
      /// context: worker thread
      assert(nullptr != eventContext.multiHandle);
      assert((nullptr == m_multiHandle) || (m_multiHandle == eventContext.multiHandle));
      assert((nullptr == m_multiHandle) == (nullptr == m_handle));
      assert((nullptr != m_handle) || (connection_status::ready != m_status));
      if (nullptr != m_socketEvent)
      {
         release_socket_event(eventContext, *m_socketEvent);
         m_socketEvent = nullptr;
      }
      if (nullptr != m_handle)
      {
         deactivate();
      }
      eventContext.timerQueue.remove(*this);
   }

private:
   CURLM *m_multiHandle = nullptr;
   CURL *m_handle = nullptr;
   std::shared_ptr<websocket_listener> m_listener;
   std::unique_ptr<curl_config> m_config;
   std::string m_urlPath = {};
   std::chrono::milliseconds m_tickTimeout;
   std::chrono::milliseconds m_connectTimeout;
   std::string m_outboundMessage = {};
   std::pair<char *, size_t> m_inboundMessageBuffer;
   socket_event *m_socketEvent = nullptr;
   curl_socket_t m_resolverSocket = CURL_SOCKET_BAD;
   curl_socket_t m_dataSocket = CURL_SOCKET_BAD;
   connection_status m_status = connection_status::initializing;
   bool m_outboundMessageReady = false;

   void deactivate()
   {
      /// context: worker thread
      assert(nullptr != m_multiHandle);
      assert(nullptr != m_handle);
      if ((nullptr != m_socketEvent) && (nullptr != std::get<event>(m_socketEvent->eventOrNext).ev_base))
      {
         assert(CURL_SOCKET_BAD != m_dataSocket);
         EXPECT_ERROR_CODE(0, event_del(std::addressof(std::get<event>(m_socketEvent->eventOrNext))));
      }
      EXPECT_ERROR_CODE(CURLM_OK, curl_multi_remove_handle(m_multiHandle, m_handle));
      m_multiHandle = nullptr;
      curl_easy_cleanup(m_handle);
      m_handle = nullptr;
      m_outboundMessage.clear();
      m_resolverSocket = CURL_SOCKET_BAD;
      m_dataSocket = CURL_SOCKET_BAD;
      m_status = connection_status::inactive;
      m_outboundMessageReady = false;
   }

   void handle_read_event()
   {
      assert(nullptr != m_handle);
      assert(nullptr != m_socketEvent);
      assert(connection_status::initializing != m_status);
      assert(connection_status::inactive != m_status);
      for (CURLcode errorCode = CURLE_OK; CURLE_OK == errorCode; )
      {
         size_t recvBytes = 0;
         curl_ws_frame const *wsMeta = nullptr;
         errorCode = curl_ws_recv(
            m_handle,
            m_inboundMessageBuffer.first,
            m_inboundMessageBuffer.second,
            std::addressof(recvBytes),
            std::addressof(wsMeta)
         );
         if (connection_status::inactive != report_error(errorCode)) [[likely]]
         {
            if (0 < recvBytes) [[likely]]
            {
               assert(CURLE_AGAIN != errorCode);
               assert(wsMeta->len == recvBytes);
               assert(0 <= wsMeta->bytesleft);
               auto messageType = (CURLWS_TEXT == (CURLWS_TEXT & wsMeta->flags))
                  ? websocket_message_type::text
                  : (CURLWS_BINARY == (CURLWS_BINARY & wsMeta->flags))
                     ? websocket_message_type::binary
                     : (CURLWS_PING == (CURLWS_PING & wsMeta->flags))
                        ? websocket_message_type::ping
                        : (CURLWS_PONG == (CURLWS_PONG & wsMeta->flags))
                           ? websocket_message_type::pong
                           : (CURLWS_CLOSE == (CURLWS_CLOSE & wsMeta->flags))
                              ? websocket_message_type::close
                              : websocket_message_type::unknown
               ;
               assert(websocket_message_type::unknown != messageType);
               m_listener->on_message_recv(
                  std::string_view{m_inboundMessageBuffer.first, recvBytes},
                  messageType,
                  static_cast<size_t>(wsMeta->bytesleft)
               );
               if (m_inboundMessageBuffer.second == recvBytes)
               {
                  continue;
               }
            }
            else
            {
               assert(CURLE_AGAIN == errorCode);
            }
            event_assign_callback(
               *std::get<event>(m_socketEvent->eventOrNext).ev_base,
               (connection_status::busy == m_status) ? EV_WRITE : EV_READ
            );
         }
         break;
      }
   }

   void handle_write_event()
   {
      assert(nullptr != m_handle);
      assert(nullptr != m_socketEvent);
      assert(connection_status::initializing != m_status);
      if (connection_status::ready == m_status)
      {
         size_t sentBytes = 0;
         auto const errorCode = curl_ws_send(
            m_handle,
            m_outboundMessage.c_str(),
            m_outboundMessage.size(),
            std::addressof(sentBytes),
            0,
            CURLWS_TEXT
         );
         assert(CURLE_AGAIN != errorCode);
         if (connection_status::ready == report_error(errorCode))
         {
            m_status = connection_status::busy;
            assert(m_outboundMessage.size() == sentBytes);
            event_assign_callback(*std::get<event>(m_socketEvent->eventOrNext).ev_base, EV_WRITE);
         }
      }
      else
      {
         assert(connection_status::busy == m_status);
         m_outboundMessage.clear();
         m_status = connection_status::ready;
         m_listener->on_message_sent();
         event_assign_callback(*std::get<event>(m_socketEvent->eventOrNext).ev_base, 0);
      }
   }

   void on_attach_socket(curl_socket_t const socket) override
   {
      assert(CURL_SOCKET_BAD != socket);
      if (CURL_SOCKET_BAD == m_resolverSocket)
      {
         m_resolverSocket = socket;
      }
      assert((socket == m_resolverSocket) || (m_resolverSocket == m_dataSocket));
      m_dataSocket = socket;
   }

   void on_detach_socket([[maybe_unused]] curl_socket_t const socket) override
   {
      assert(CURL_SOCKET_BAD != socket);
      assert((socket == m_resolverSocket) || (socket == m_dataSocket));
   }

   void on_recv_message(event_context &eventContext, CURLMsg const &message) override
   {
      /// context: worker thread
      assert(nullptr != eventContext.multiHandle);
      assert(nullptr != message.easy_handle);
      assert(message.easy_handle == m_handle);
      assert(connection_status::inactive != m_status);
      if (connection_status::initializing == report_error(message.data.result))
      {
         assert(nullptr == m_socketEvent);
         assert(CURL_SOCKET_BAD != m_dataSocket);
         m_socketEvent = acquire_socket_event(eventContext);
         assert(nullptr != m_socketEvent);
         m_status = connection_status::ready;
         if (std::chrono::milliseconds::zero() < m_tickTimeout)
         {
            eventContext.timerQueue.push(std::chrono::system_clock::now() + m_tickTimeout, *this);
         }
         if (true == m_outboundMessageReady)
         {
            m_outboundMessageReady = false;
            event_assign_callback(eventContext.eventBase, EV_WRITE);
         }
         else
         {
            event_assign_callback(eventContext.eventBase, 0);
         }
      }
      else
      {
         assert(connection_status::inactive == m_status);
      }
   }

   [[nodiscard]] connection_status report_error(CURLcode const errorCode)
   {
      /// context: worker thread
      assert(connection_status::inactive != m_status);
      if ((CURLE_OK == errorCode) || (CURLE_AGAIN == errorCode)) [[likely]]
      {
         return m_status;
      }
      deactivate();
      m_listener->on_error(errorCode, curl_easy_strerror(errorCode));
      return m_status;
   }

   void report_error(CURLMcode const errorCode)
   {
      /// context: worker thread
      assert(connection_status::inactive != m_status);
      if (CURLM_OK == errorCode) [[likely]]
      {
         return;
      }
      deactivate();
      m_listener->on_error(errorCode, curl_multi_strerror(errorCode));
   }

   void event_assign_callback(event_base &eventBase, short const additionalSocketEventKind)
   {
      assert(nullptr != m_socketEvent);
      assert(CURL_SOCKET_BAD != m_dataSocket);
      assert(connection_status::inactive != m_status);
      if (nullptr != std::get<event>(m_socketEvent->eventOrNext).ev_base) [[likely]]
      {
         EXPECT_ERROR_CODE(0, event_del(std::addressof(std::get<event>(m_socketEvent->eventOrNext))));
      }
      EXPECT_ERROR_CODE(
         0,
         event_assign(
            std::addressof(std::get<event>(m_socketEvent->eventOrNext)),
            std::addressof(eventBase),
            m_dataSocket,
            EV_READ | additionalSocketEventKind,
            &websocket_connection::event_socket_callback,
            this
         )
      );
      EXPECT_ERROR_CODE(0, event_add(std::addressof(std::get<event>(m_socketEvent->eventOrNext)), nullptr));
   }

   static void event_socket_callback([[maybe_unused]] evutil_socket_t const socket, short const socketEventKind, void *userdata)
   {
      assert(nullptr != userdata);
      auto &self = *static_cast<websocket_connection *>(userdata);
      assert(socket == static_cast<evutil_socket_t>(self.m_dataSocket));
      if (EV_READ == (socketEventKind & EV_READ))
      {
         self.handle_read_event();
      }
      if ((EV_WRITE == (socketEventKind & EV_WRITE)) && (connection_status::inactive != self.m_status))
      {
         self.handle_write_event();
      }
   }
};

thread::websocket::websocket() noexcept = default;

thread::websocket::websocket(websocket &&rhs) noexcept = default;

thread::websocket::websocket(thread const &thread, std::shared_ptr<websocket_listener> const &listener, ws_config const &wsConfig) :
   m_worker(thread.m_worker),
   m_connection(
      std::make_shared<websocket_connection>(
         listener,
         std::make_unique<curl_config>(
            "ws",
            wsConfig.peer_address(),
            std::nullopt,
            wsConfig.net_interface(),
            wsConfig.keep_alive_delay(),
            std::nullopt
         ),
         wsConfig.url_path(),
         wsConfig.connect_timeout(),
         wsConfig.tick_timeout(),
         thread.m_worker->websocket_buffer()
      )
   )
{
   m_worker->add_connection(m_connection);
}

thread::websocket::websocket(thread const &thread, std::shared_ptr<websocket_listener> const &listener, wss_config const &wssConfig) :
   m_worker(thread.m_worker),
   m_connection(
      std::make_shared<websocket_connection>(
         listener,
         std::make_unique<curl_config>(
            "wss",
            wssConfig.peer_address(),
            wssConfig.peer_real_address(),
            wssConfig.net_interface(),
            wssConfig.keep_alive_delay(),
            wssConfig.ssl_certificate()
         ),
         wssConfig.url_path(),
         wssConfig.connect_timeout(),
         wssConfig.tick_timeout(),
         thread.m_worker->websocket_buffer()
      )
   )
{
   m_worker->add_connection(m_connection);
}

thread::websocket::~websocket() noexcept
{
   if (nullptr != m_connection)
   {
      assert(nullptr != m_worker);
      m_worker->remove_connection(m_connection);
   }
}

thread::websocket &thread::websocket::operator = (websocket &&rhs) noexcept = default;

void thread::websocket::reconnect()
{
   assert(nullptr != m_worker);
   assert(nullptr != m_connection);
   m_worker->remove_connection(m_connection);
   m_worker->add_connection(m_connection);
}

void thread::websocket::write(std::string_view message)
{
   assert(nullptr != m_worker);
   assert(nullptr != m_connection);
   m_connection->set_outbound_message(message);
   m_worker->enqueue_request(m_connection);
}

thread::thread(thread &&rhs) noexcept :
   m_worker(std::forward<decltype(rhs.m_worker)>(rhs.m_worker))
{
   assert(nullptr != m_worker);
}

thread::thread(thread const &rhs) noexcept :
   m_worker(rhs.m_worker)
{
   assert(nullptr != m_worker);
}

thread::thread(thread_config const config) :
   m_worker(std::make_shared<worker>(config))
{
   assert(nullptr != m_worker);
}

thread::~thread()
{
   m_worker.reset();
}

}
