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

// NOLINTNEXTLINE(llvm-header-guard)
#ifndef LYN_MQ_TIMER_QUEUE_HPP_DE38A168_BE4F_11ED_AE3D_90B11C0C0FF8
#define LYN_MQ_TIMER_QUEUE_HPP_DE38A168_BE4F_11ED_AE3D_90B11C0C0FF8

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <type_traits>
#include <utility>

namespace lyn::mq {
namespace detail {
    // Make sure no promise goes unfulfilled without having to catch exceptions.
    // This should really be a nested class inside timer_queue but older gcc's (at least 7.4) says deduction guides need
    // to be at namespace scope.
    template<class RR, class B>
    struct prom_ctx {
        prom_ctx(B bad) : bad_val(bad) {}
        prom_ctx(const prom_ctx&) = delete;
        prom_ctx(prom_ctx&&) = delete;
        prom_ctx& operator=(const prom_ctx&) = delete;
        prom_ctx& operator=(prom_ctx&&) = delete;

        ~prom_ctx() {
            if(has_bad_val) {
                prom.set_value(std::move(bad_val));
            }
        }

        std::future<RR> get_future() { return prom.get_future(); }

        void set_value(const RR& value) {
            prom.set_value(value);
            has_bad_val = false;
        }

        void set_value(RR&& value) {
            prom.set_value(std::move(value));
            has_bad_val = false;
        }

        // bad_val should be a `std::optional<B>` - but gcc 7.4 does not like std::optional<std::nullopt_t>
        std::promise<RR> prom{};
        B bad_val;
        bool has_bad_val = true;
    };
    template<class RR, class B>
    prom_ctx(std::promise<RR>, B) -> prom_ctx<RR, B>;
} // namespace detail

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
        explicit TimedEvent(const time_point& tpnt, EvArgs&&... args) :
            StartTime{tpnt}, m_event{std::forward<EvArgs>(args)...} {}

        bool operator<(const TimedEvent& rhs) const { return rhs.StartTime < StartTime; }
        time_point StartTime;
        event_type m_event;
    };

public:
    struct event_container : std::priority_queue<TimedEvent> {
        using std::priority_queue<TimedEvent>::pop;

        bool pop(event_type& eve) {
            if(this->empty()) return false;
            eve = std::move(this->top().m_event); // extract event
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
        const std::lock_guard<std::mutex> lock(m_mutex);
        ++m_users;
        return *this;
    }

    void unreg() {
        const std::lock_guard<std::mutex> lock(m_mutex);
        --m_users;
        m_cv.notify_all();
    }

    void shutdown() {
        m_shutdown = true;
        m_cv.notify_all();
    }

    void restart() { m_shutdown = false; }

    std::size_t size() const {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    void clear() {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_queue = event_container{};
    }

    void set_delay_until(time_point tpnt) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        // don't let it move backwards in time
        if(m_delay_until < tpnt) m_delay_until = std::move(tpnt);
    }

    bool is_open() const { return !m_shutdown; }
    bool operator!() const { return m_shutdown; }
    explicit operator bool() const { return !m_shutdown; }

public:
    // Re is here the promised return value by added functor
    // R is what's returned in the event loop
    // Two overloads for void and non-void returns in the event loop
    template<class Re, class Func, class LoopR = R,
             std::enable_if_t<std::is_same_v<LoopR, void> && std::is_same_v<LoopR, R>, int> = 0>
    [[nodiscard]] auto synchronize(Func&& func) {
        if constexpr(std::is_same_v<Re, void>) { // will return bool, true == 0K
            auto prom = std::make_shared<detail::prom_ctx<bool, bool>>(false);
            std::future<bool> fut = prom->get_future();

            emplace_do_urgently([prom = std::move(prom), func = std::forward<Func>(func)](Args&&... args) {
                func(std::forward<Args>(args)...);
                prom->set_value(true);
            });

            return fut.get(); // true if there was no exception, false if there was
        } else {              // will return std::optional<Re>, non-empty optional == OK
            auto prom = std::make_shared<detail::prom_ctx<std::optional<Re>, std::nullopt_t>>(std::nullopt);
            std::future<std::optional<Re>> fut = prom->get_future();

            emplace_do_urgently([prom = std::move(prom), func = std::forward<Func>(func)](Args&&... args) {
                prom->set_value(func(std::forward<Args>(args)...));
            });

            return fut.get(); // the optional<Re> has a value if there was no exception
        }
    }

    template<class Re, class Func, class LoopR = R,
             std::enable_if_t<!std::is_same_v<LoopR, void> && std::is_same_v<LoopR, R>, int> = 0>
    [[nodiscard]] auto synchronize(Func&& func, LoopR&& event_loop_return_value = R{}) {
        if constexpr(std::is_same_v<Re, void>) { // will return bool, true == 0K
            auto prom = std::make_shared<detail::prom_ctx<bool, bool>>(false);
            std::future<bool> fut = prom->get_future();

            emplace_do_urgently([prom = std::move(prom), elrv = std::forward<LoopR>(event_loop_return_value),
                                 func = std::forward<Func>(func)](Args&&... args) -> R {
                func(std::forward<Args>(args)...);
                prom->set_value(true);
                return std::move(elrv);
            });

            return fut.get(); // true if there was no exception, false if there was
        } else {              // will return std::optional<Re>, non-empty optional == OK
            auto prom = std::make_shared<detail::prom_ctx<std::optional<Re>, std::nullopt_t>>(std::nullopt);
            std::future<std::optional<Re>> fut = prom->get_future();

            emplace_do_urgently([prom = std::move(prom), elrv = std::forward<LoopR>(event_loop_return_value),
                                 func = std::forward<Func>(func)](Args&&... args) -> R {
                prom->set_value(func(std::forward<Args>(args)...));
                return std::move(elrv);
            });

            return fut.get(); // the optional<Re> has a value if there was no exception
        }
    }

    template<class... EvArgs>
    bool emplace_do_at(time_point tpnt, EvArgs&&... args) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        bool added = not m_shutdown;
        if(added) m_queue.emplace(tpnt, std::forward<EvArgs>(args)...);
        m_cv.notify_all();
        return added;
    }

    template<class... EvArgs>
    bool emplace_do_in(duration dur, EvArgs&&... args) {
        auto attime = clock_type::now() + dur;
        const std::lock_guard<std::mutex> lock(m_mutex);
        bool added = not m_shutdown;
        if(added) m_queue.emplace(attime, std::forward<EvArgs>(args)...);
        m_cv.notify_all();
        return added;
    }

    template<class... EvArgs>
    bool emplace_do(EvArgs&&... args) {
        auto now = clock_type::now();
        const std::lock_guard<std::mutex> lock(m_mutex);
        bool added = not m_shutdown;
        if(added) {
            if(now < m_delay_until) {
                now = m_delay_until;
                m_delay_until += std::chrono::nanoseconds(1);
            } else {
                now += m_now_delay;
            }
            m_queue.emplace(now, std::forward<EvArgs>(args)...);
        }
        m_cv.notify_all();
        return added;
    }

    template<class... EvArgs>
    bool emplace_do_urgently(EvArgs&&... args) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        bool added = not m_shutdown;
        if(added) {
            m_queue.emplace(m_seq, std::forward<EvArgs>(args)...);
            m_seq += std::chrono::nanoseconds(1);
        }
        m_cv.notify_all();
        return added;
    }

    // Add a bunch of events using iterators. Events will be processed in the order they are added.
    template<class Iter>
    std::enable_if_t<std::is_same_v<event_type, typename std::iterator_traits<Iter>::value_type>, bool> //
    emplace_schedule(Iter first, Iter last) {
        auto now = clock_type::now();
        const std::lock_guard<std::mutex> lock(m_mutex);
        bool added = not m_shutdown;
        if(added) {
            if(now < m_delay_until) {
                for(; first != last; ++first) {
                    m_queue.emplace(m_delay_until, *first);
                    m_delay_until += std::chrono::nanoseconds(1);
                }
            } else {
                now += m_now_delay;
                for(; first != last; ++first) {
                    m_queue.emplace(now, *first);
                    now += std::chrono::nanoseconds(1);
                }
            }
        }
        m_cv.notify_all();
        return added;
    }

    // Add a bunch of events with pre-calculated time_points using iterators.
    template<class Iter>
    std::enable_if_t<std::is_same_v<schedule_at_type, typename std::iterator_traits<Iter>::value_type>, bool>
    emplace_schedule(Iter first, Iter last) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        bool added = not m_shutdown;
        if(added) {
            for(; first != last; ++first) {
                auto&& [tpnt, eve] = *first;
                m_queue.emplace(tpnt, eve);
            }
        }
        m_cv.notify_all();
        return added;
    }

    // Add a bunch of events with durations in relation to the supplied time_point T_0 using iterators.
    template<class Iter>
    std::enable_if_t<std::is_same_v<schedule_in_type, typename std::iterator_traits<Iter>::value_type>, bool>
    emplace_schedule(time_point T_0, Iter first, Iter last) {
        std::vector<schedule_at_type> ateve;
        if constexpr(std::is_base_of_v<std::random_access_iterator_tag,
                                       typename std::iterator_traits<Iter>::iterator_category>) {
            ateve.reserve(static_cast<typename decltype(ateve)::size_type>(std::distance(first, last)));
        }
        std::transform(first, last, std::back_inserter(ateve), [&T_0](auto&& dureve) {
            return schedule_at_type{T_0 + dureve.first, dureve.second};
        });
        return emplace_schedule(std::move_iterator(ateve.begin()), std::move_iterator(ateve.end()));
    }

    // Add a bunch of events with durations in relation to clock_type::now() using iterators.
    template<class Iter>
    std::enable_if_t<std::is_same_v<schedule_in_type, typename std::iterator_traits<Iter>::value_type>, bool>
    emplace_schedule(Iter first, Iter last) {
        return emplace_schedule(clock_type::now(), first, last);
    }

    bool wait_pop(event_type& eve) {
        std::unique_lock<std::mutex> lock(m_mutex);

        while((m_queue.empty() || clock_type::now() < m_queue.top().StartTime) && not m_shutdown) {
            if(m_queue.empty()) {
                m_cv.wait(lock);
            } else {
                auto sta = m_queue.top().StartTime;
                m_cv.wait_until(lock, sta);
            }
        }
        if(m_shutdown) return false; // time to quit

        eve = std::move(m_queue.top().m_event); // extract event
        m_queue.pop();

        return true;
    }

    bool wait_pop_all(event_container& in_out) {
        in_out = event_container{}; // make sure it's empty
        std::unique_lock<std::mutex> lock(m_mutex);

        while((m_queue.empty() || clock_type::now() < m_queue.top().StartTime) && not m_shutdown) {
            if(m_queue.empty()) {
                m_cv.wait(lock);
            } else {
                auto sta = m_queue.top().StartTime;
                m_cv.wait_until(lock, sta);
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
    bool wait_pop_future(event_type& eve) {
        std::unique_lock<std::mutex> lock(m_mutex);

        while(m_queue.empty() && not m_shutdown) {
            m_cv.wait(lock);
        }
        if(m_shutdown) return false; // time to quit

        eve = std::move(m_queue.top().m_event); // extract event
        m_queue.pop();

        return true;
    }

    bool wait_pop_all_future(event_container& in_out) {
        in_out = event_container{}; // make sure it's empty
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
    event_container m_queue{};
    mutable std::mutex m_mutex{};
    std::condition_variable m_cv{};
    std::atomic<bool> m_shutdown{};
    duration m_now_delay;
    time_point m_delay_until{};
    // m_seq is used for to make sure events are executed in the order they are put in the queue which can be used
    // to extend the queue with adding a bunch of elements at the same time and to guarantee them to be extracted
    // together in the exact order they were put in.
    time_point m_seq{};
    unsigned m_users{};
};

template<class QT>
class timer_queue_registrator {
public:
    using timer_queue = QT;
    using event_type = typename timer_queue::event_type;
    using event_container = typename timer_queue::event_container;

    timer_queue_registrator() = delete;
    // cast return from tq.reg() to timer_queue& in case of inheritance
    timer_queue_registrator(timer_queue& timq) : m_tq(&static_cast<timer_queue&>(timq.reg())) {}
    timer_queue_registrator(std::reference_wrapper<timer_queue> timq_rw) : timer_queue_registrator(timq_rw.get()) {}

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

    timer_queue& queue() { return *m_tq; }

private:
    timer_queue* m_tq;
};

} // namespace lyn::mq

#endif
