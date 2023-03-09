#include "lyn/timer_queue.hpp"

#include "include/jthread.hpp"

#include <iostream>
#include <thread>

namespace {

struct Test {
    virtual ~Test() = default;
    virtual bool Run() = 0;
};

// poor mans std::jthread for C++17
struct jthread : public std::thread {
    using std::thread::thread;

    jthread() = default;

    jthread(jthread&&) = default;
    jthread& operator=(jthread&&) noexcept = default;

    ~jthread() {
        if(joinable()) join();
    }
};

struct ScheduleTest : Test {
    using queue_t = lyn::mq::timer_queue<void()>;
    using queue_registrator_t = lyn::mq::timer_queue_registrator<queue_t>;

    static void pump(queue_registrator_t reg) {
        auto& q = reg.queue();
        queue_registrator_t::event_type eve;

        while(q.wait_pop(eve)) {
            eve();
        }
    }

    bool Run() override {
        jthread pumper;
        queue_t q;

        pumper = jthread(&ScheduleTest::pump, queue_registrator_t(q));

        auto T_0 = std::chrono::steady_clock::now();
        {
            std::vector<queue_t::schedule_at_type> sat;

            sat.emplace_back(T_0 + std::chrono::milliseconds(000), [] { std::cout << "1\n"; });
            sat.emplace_back(T_0 + std::chrono::milliseconds(100), [] { std::cout << "2\n"; });
            sat.emplace_back(T_0 + std::chrono::milliseconds(200), [] { std::cout << "3\n"; });
            sat.emplace_back(T_0 + std::chrono::milliseconds(300), [] { std::cout << "4\n"; });
            sat.emplace_back(T_0 + std::chrono::milliseconds(400), [] { std::cout << "5\n"; });

            q.emplace_schedule(sat.begin(), sat.end());
        }
        {
            std::vector<queue_t::schedule_in_type> sat;

            sat.emplace_back(std::chrono::milliseconds(000), [] { std::cout << "A\n"; });
            sat.emplace_back(std::chrono::milliseconds(100), [] { std::cout << "B\n"; });
            sat.emplace_back(std::chrono::milliseconds(200), [] { std::cout << "C\n"; });
            sat.emplace_back(std::chrono::milliseconds(300), [] { std::cout << "D\n"; });
            sat.emplace_back(std::chrono::milliseconds(400), [] { std::cout << "E\n"; });

            q.emplace_schedule(T_0, sat.begin(), sat.end());
        }
        {
            std::vector<queue_t::event_type> sat;
            sat.emplace_back([] { std::cout << " I1\n"; });
            sat.emplace_back([] { std::cout << " I2\n"; });
            sat.emplace_back([] { std::cout << " I3\n"; });
            sat.emplace_back([] { std::cout << " I4\n"; });
            sat.emplace_back([] { std::cout << " I5\n"; });

            q.emplace_schedule(sat.begin(), sat.end());
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
        return true;
    }
};

} // namespace

namespace ut_schedule {
int main() {
    return !ScheduleTest().Run();
}
} // namespace ut_schedule
