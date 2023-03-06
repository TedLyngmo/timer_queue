/*
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.
In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
For more information, please refer to <https://unlicense.org>
*/

// Original: https://github.com/TedLyngmo/timer_queue

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <iterator>
#include <mutex>
#include <queue>
#include <type_traits>
#include <utility>

namespace lyn {
namespace mq {

    template<class, class Clock = std::chrono::steady_clock, class TimePoint = typename Clock::time_point>
    class timer_queue;

    template<class R, class... Args, class Clock, class TimePoint>
    class timer_queue<R(Args...), Clock, TimePoint> {
    public:
        using event_type = std::function<R(Args...)>;
        using clock_type = Clock;
        using duration = typename Clock::duration;
        using time_point = TimePoint;
        using schedule_at_type = std::pair<time_point, event_type>;
        using schedule_in_type = std::pair<duration, event_type>;

    private:
        struct TimedEvent {
            template<class... EvArgs>
            TimedEvent(const time_point& tp, EvArgs&&... args) :
                StartTime{tp}, m_event{std::forward<EvArgs>(args)...} {}

            bool operator<(const TimedEvent& rhs) const { return rhs.StartTime < StartTime; }
            time_point StartTime;
            event_type m_event;
        };

    public:
        struct queue_type : std::priority_queue<TimedEvent> {
            using std::priority_queue<TimedEvent>::pop;

            bool pop(event_type& ev) {
                if(this->empty()) return false;
                ev = std::move(this->top().m_event); // extract event
                this->pop();
                return true;
            }
        };

        explicit timer_queue(duration now_delay) : m_now_delay(std::move(now_delay)) {}

        timer_queue() : timer_queue(std::chrono::nanoseconds(0)) {}
        timer_queue(const timer_queue&) = delete;
        timer_queue(timer_queue&&) = delete;
        timer_queue& operator=(const timer_queue&) = delete;
        timer_queue& operator=(timer_queue&&) = delete;
        ~timer_queue() {
            shutdown();
            std::unique_lock<std::mutex> lock(m_mutex);
            while(m_users) m_cv.wait(lock);
        }

        timer_queue& reg() {
            std::lock_guard<std::mutex> lock(m_mutex);
            ++m_users;
            return *this;
        }

        void unreg() {
            std::lock_guard<std::mutex> lock(m_mutex);
            --m_users;
            m_cv.notify_all();
        }

        void shutdown() {
            m_shutdown = true;
            m_cv.notify_all();
        }

        void clear() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue = queue_type{};
        }

        void restart() { m_shutdown = false; }

        bool is_open() const { return !m_shutdown; }
        bool operator!() const { return m_shutdown; }
        explicit operator bool() const { return !m_shutdown; }

        // Re is here the promised return value by added functor
        // R is what's returned in the event loop
        // Two overloads for void and non-void returns in the event loop
        template<class Re, class Func, class LoopR = R,
                 std::enable_if_t<std::is_same_v<LoopR, void> && std::is_same_v<LoopR, R>, int> = 0>
        Re synchronize(Func&& func) {
            std::promise<Re> p;
            std::future<Re> f = p.get_future();

            if constexpr(std::is_same_v<Re, void>) {
                emplace_do_urgently([&p, func = std::forward<Func>(func)](Args&&... args) {
                    func(std::forward<Args>(args)...);
                    p.set_value();
                });

                f.wait();
            } else {
                emplace_do_urgently([&p, func = std::forward<Func>(func)](Args&&... args) {
                    p.set_value(func(std::forward<Args>(args)...));
                });

                f.wait();
                return f.get();
            }
        }

        template<class Re, class Func, class LoopR = R,
                 std::enable_if_t<!std::is_same_v<LoopR, void> && std::is_same_v<LoopR, R>, int> = 0>
        Re synchronize(Func&& func, LoopR&& event_loop_return_value = R{}) {
            std::promise<Re> p;
            std::future<Re> f = p.get_future();

            if constexpr(std::is_same_v<Re, void>) {
                emplace_do_urgently([&p, elrv = std::forward<LoopR>(event_loop_return_value),
                                     func = std::forward<Func>(func)](Args&&... args) -> R {
                    func(std::forward<Args>(args)...);
                    p.set_value();
                    return std::move(elrv);
                });

                f.wait();
            } else {
                emplace_do_urgently([&p, elrv = std::forward<LoopR>(event_loop_return_value),
                                     func = std::forward<Func>(func)](Args&&... args) -> R {
                    p.set_value(func(std::forward<Args>(args)...));
                    return std::move(elrv);
                });

                f.wait();
                return f.get();
            }
        }

        template<class... EvArgs>
        void emplace_do_at(time_point tp, EvArgs&&... args) {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_queue.emplace(tp, std::forward<EvArgs>(args)...);
            m_seq += std::chrono::nanoseconds(1); // used by emplace_do_urgently
            m_cv.notify_all();
        }

        template<class... EvArgs>
        void emplace_do_in(duration dur, EvArgs&&... args) {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_queue.emplace(clock_type::now() + dur, std::forward<EvArgs>(args)...);
            m_cv.notify_all();
        }

        template<class... EvArgs>
        void emplace_do(EvArgs&&... args) {
            emplace_do_in(m_now_delay, std::forward<EvArgs>(args)...);
        }

        template<class... EvArgs>
        void emplace_do_urgently(EvArgs&&... args) {
            emplace_do_at(m_seq, std::forward<EvArgs>(args)...);
        }

        // Add a bunch of events using iterators. Events will be processed in the order they are added.
        template<class Iter>
        std::enable_if_t<std::is_same_v<event_type, typename std::iterator_traits<Iter>::value_type>> //
        emplace_schedule(Iter first, Iter last) {
            auto T0 = clock_type::now() + m_now_delay;
            std::chrono::nanoseconds event_order{0};

            std::lock_guard<std::mutex> lock(m_mutex);
            for(; first != last; ++first) {
                m_queue.emplace(T0 + event_order, *first);
                event_order += std::chrono::nanoseconds(1);
            }
            m_cv.notify_all();
        }

        // Add a bunch of events with pre-calculated time_points using iterators.
        template<class Iter>
        std::enable_if_t<std::is_same_v<schedule_at_type, typename std::iterator_traits<Iter>::value_type>>
        emplace_schedule(Iter first, Iter last) {
            std::lock_guard<std::mutex> lock(m_mutex);
            for(; first != last; ++first) {
                auto&& [tp, ev] = *first;
                m_queue.emplace(tp, ev);
            }
            m_cv.notify_all();
        }

        // Add a bunch of events with durations in relation to the supplied time_point T0 using iterators.
        template<class Iter>
        std::enable_if_t<std::is_same_v<schedule_in_type, typename std::iterator_traits<Iter>::value_type>>
        emplace_schedule(time_point T0, Iter first, Iter last) {
            std::vector<schedule_at_type> atev;
            if constexpr(std::is_base_of_v<std::random_access_iterator_tag,
                                           typename std::iterator_traits<Iter>::iterator_category>) {
                atev.reserve(static_cast<typename decltype(atev)::size_type>(std::distance(first, last)));
            }
            std::transform(first, last, std::back_inserter(atev), [&T0](auto&& de) {
                return schedule_at_type{T0 + de.first, de.second};
            });
            emplace_schedule(std::move_iterator(atev.begin()), std::move_iterator(atev.end()));
        }

        // Add a bunch of events with durations in relation to clock_type::now() using iterators.
        template<class Iter>
        std::enable_if_t<std::is_same_v<schedule_in_type, typename std::iterator_traits<Iter>::value_type>>
        emplace_schedule(Iter first, Iter last) {
            emplace_schedule(clock_type::now(), first, last);
        }

        bool wait_pop(event_type& ev) {
            std::unique_lock<std::mutex> lock(m_mutex);

            while((m_queue.empty() || clock_type::now() < m_queue.top().StartTime) && not m_shutdown) {
                if(m_queue.empty()) {
                    m_cv.wait(lock);
                } else {
                    auto st = m_queue.top().StartTime;
                    m_cv.wait_until(lock, st);
                }
            }
            if(m_shutdown) return false; // time to quit

            ev = std::move(m_queue.top().m_event); // extract event
            m_queue.pop();

            return true;
        }

        bool wait_pop_all(queue_type& in_out) {
            in_out = queue_type{}; // make sure it's empty
            std::unique_lock<std::mutex> lock(m_mutex);

            while((m_queue.empty() || clock_type::now() < m_queue.top().StartTime) && not m_shutdown) {
                if(m_queue.empty()) {
                    m_cv.wait(lock);
                } else {
                    auto st = m_queue.top().StartTime;
                    m_cv.wait_until(lock, st);
                }
            }
            if(m_shutdown) return false;

            auto now = clock_type::now();
            while(!m_queue.empty() && now >= m_queue.top().StartTime) {
                in_out.emplace(std::move(m_queue.top()));
                m_queue.pop();
            }

            return true;
        }

    protected:
        // These methods violate the timing aspect and extracts queued events including those that expires in the
        // future. One possible use-case is when writing tests that don't care about the timing.
        bool wait_pop_future(event_type& ev) {
            std::unique_lock<std::mutex> lock(m_mutex);

            while(m_queue.empty() && not m_shutdown) {
                m_cv.wait(lock);
            }
            if(m_shutdown) return false; // time to quit

            ev = std::move(m_queue.top().m_event); // extract event
            m_queue.pop();

            return true;
        }

        bool wait_pop_all_future(queue_type& in_out) {
            in_out = queue_type{}; // make sure it's empty
            std::unique_lock<std::mutex> lock(m_mutex);

            while(m_queue.empty() && not m_shutdown) {
                m_cv.wait(lock);
            }
            if(m_shutdown) return false;

            std::swap(in_out, m_queue);

            return true;
        }

        duration get_now_delay() const { return m_now_delay; }
        time_point get_seq() const { return m_seq; }

    private:
        queue_type m_queue{};
        mutable std::mutex m_mutex{};
        std::condition_variable m_cv{};
        std::atomic<bool> m_shutdown{};
        duration m_now_delay;
        // m_seq is used for to make sure events are executed in the order they are put in the queue which can be used
        // to extend the queue with adding a bunch of elements at the same time and to guarantee them to be extracted
        // together in the exact order they were put in.
        time_point m_seq{};
        unsigned m_users{};
    };

    template<class queue_type>
    class timer_queue_registrator {
    public:
        using event_type = typename queue_type::event_type;
        timer_queue_registrator() = delete;
        timer_queue_registrator(queue_type& tq) : m_tq(&static_cast<queue_type&>(tq.reg())) {}
        timer_queue_registrator(std::reference_wrapper<queue_type> tq) : timer_queue_registrator(tq.get()) {}

        timer_queue_registrator(const timer_queue_registrator&) = delete;
        timer_queue_registrator(timer_queue_registrator&& other) noexcept : m_tq{std::exchange(other.m_tq, nullptr)} {}
        timer_queue_registrator& operator=(const timer_queue_registrator&) = delete;
        timer_queue_registrator& operator=(timer_queue_registrator&& other) noexcept {
            std::swap(m_tq, other.m_tq);
            return *this;
        }
        ~timer_queue_registrator() {
            if(m_tq) m_tq->unreg();
        }

        queue_type& queue() { return *m_tq; }

    private:
        queue_type* m_tq;
    };

} // namespace mq
} // namespace lyn
