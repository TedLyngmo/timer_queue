/*
 * PURPOSE:
 * The purpose of this test is to inherit from a `timer_queue` and to
 * provide an interface for events that is different from those
 * processed by the actual `timer_queue`.
 * The user of the "SpecialQueue" provides "void()" events, but the
 * queue itself processes "bool()" events.
 * Currently the test is that it actually compiles without warnings and
 * it doesn't perform any actual actions.
*/
#include "lyn/timer_queue.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace {
namespace detail {
    class SpecialTest {
    public:
        class Token {
        public:
            using flag_type = std::atomic<bool>;

            Token() : m_canceled{std::make_shared<flag_type>()} {}

            Token(const Token&) = default;
            Token(Token&&) noexcept = default;
            Token& operator=(const Token&) = default;
            Token& operator=(Token&&) noexcept = default;
            ~Token() = default;

            void cancel() { *m_canceled = true; }

            [[nodiscard]] bool is_canceled() const { return *m_canceled; }

        private:
            std::shared_ptr<flag_type> m_canceled;
        };

        template<class F>
        SpecialTest(Token ctt, F&& func) : token(std::move(ctt)), task{std::forward<F>(func)} {}

        bool operator()() {
            if(not token.is_canceled()) {
                task();
                return true;
            }
            return false;
        }

    private:
        Token token;
        std::function<void()> task;
    };

    using base_queue = lyn::mq::timer_queue<bool()>;
} // namespace detail

class SpecialQueue : public detail::base_queue {
public:
    using token_type = detail::SpecialTest::Token;
    using in_event = std::function<void()>;
    using schedule_at_type = std::pair<time_point, in_event>;
    using schedule_in_type = std::pair<duration, in_event>;

    using detail::base_queue::timer_queue;
    using detail::base_queue::operator=;

private:
    template<class Event>
    auto make_void_event_into_bool(Event&& eve) {
        return [eve = std::forward<Event>(eve)] {
            eve();
            return true;
        };
    }

public:
    using detail::base_queue::emplace_do_in;

    // wrappers for making `void()` events into `bool()` events
    template<class Event, std::enable_if_t<std::is_invocable_r_v<void, Event>, int> = 0>
    void emplace_do_in(duration dur, Event&& eve) {
        timer_queue::emplace_do_in(dur, make_void_event_into_bool(std::forward<Event>(eve)));
    }

    template<class Event, std::enable_if_t<std::is_invocable_r_v<void, Event>, int> = 0>
    void emplace_do_at(time_point tp, Event&& eve) {
        timer_queue::emplace_do_at(tp, make_void_event_into_bool(std::forward<Event>(eve)));
    }

    template<class Event, std::enable_if_t<std::is_invocable_r_v<void, Event>, int> = 0>
    void emplace_do(Event&& eve) {
        timer_queue::emplace_do(make_void_event_into_bool(std::forward<Event>(eve)));
    }

    template<class Event, std::enable_if_t<std::is_invocable_r_v<void, Event>, int> = 0>
    void emplace_do_urgently(Event&& eve) {
        timer_queue::emplace_do_urgently(make_void_event_into_bool(std::forward<Event>(eve)));
    }

    // Add a bunch of events with durations in relation to the supplied time_point T_0 using iterators.
    template<class Iter>
    std::enable_if_t<std::is_same_v<schedule_in_type, typename std::iterator_traits<Iter>::value_type>>
    emplace_schedule(time_point T_0, Iter first, Iter last) {
        std::vector<detail::base_queue::schedule_at_type> ateve;

        if constexpr(std::is_base_of_v<std::random_access_iterator_tag,
                                       typename std::iterator_traits<Iter>::iterator_category>) {
            ateve.reserve(static_cast<typename decltype(ateve)::size_type>(std::distance(first, last)));
        }

        // transform `void` functors into `bool` functors
        std::transform(first, last, std::back_inserter(ateve), [&T_0](auto de) {
            return detail::base_queue::schedule_at_type{T_0 + de.first, [func = std::move(de.second)] {
                                                            func();
                                                            return true;
                                                        }};
        });

        timer_queue::emplace_schedule(std::move_iterator(ateve.begin()), std::move_iterator(ateve.end()));
    }

    [[nodiscard]] token_type emplace_do_special_in(duration dur, in_event eve) {
        token_type token;
        timer_queue::emplace_do_in(dur, detail::SpecialTest{token, std::move(eve)});
        return token;
    }

    [[nodiscard]] token_type emplace_do_special_at(time_point tp, in_event eve) {
        token_type token;
        timer_queue::emplace_do_at(tp, detail::SpecialTest{token, std::move(eve)});
        return token;
    }
};

using SpecialQueueRegistrator = lyn::mq::timer_queue_registrator<SpecialQueue>;

void bgt(SpecialQueueRegistrator reg) {
    [[maybe_unused]] auto& queue = reg.queue();
    queue.emplace_do_in(std::chrono::milliseconds(100), []{ return true; }); // use base class `emplace_do_in`
}
} // namespace

namespace ut_inheritance {
int main() {
    SpecialQueue taq;
    auto thr = std::thread(bgt, SpecialQueueRegistrator(taq));

    thr.join();
    return 0;
}
} // namespace ut_inheritance
