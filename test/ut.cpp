#include "lyn/timer_queue.hpp"

#include <iostream>
#include <thread>

struct Test {
    virtual ~Test() = default;
    virtual bool Run() = 0;
};

struct jthread : std::thread {
    using std::thread::thread;
    using std::thread::operator=;
    inline ~jthread() {
        if(joinable()) join();
    }
};


struct ScheduleTest : Test {
    using queue_type = lyn::mq::timer_queue<void()>;

    void pump(lyn::mq::timer_queue_registrator<queue_type> reg) {
        auto& q = reg.queue();
        queue_type::event_type ev;
        
        while(q.wait_pop(ev)) {
            ev();
        }
    }

    bool Run() override {
        jthread pumper;
        queue_type q;

        pumper = std::thread(&ScheduleTest::pump, this, std::ref(q));

        auto T0 = std::chrono::steady_clock::now();
        {
            std::vector<queue_type::schedule_at_type> sa;

            sa.emplace_back(T0 + std::chrono::milliseconds(000), []{std::cout << "1\n"; });
            sa.emplace_back(T0 + std::chrono::milliseconds(100), []{std::cout << "2\n"; });
            sa.emplace_back(T0 + std::chrono::milliseconds(200), []{std::cout << "3\n"; });
            sa.emplace_back(T0 + std::chrono::milliseconds(300), []{std::cout << "4\n"; });
            sa.emplace_back(T0 + std::chrono::milliseconds(400), []{std::cout << "5\n"; });
        
            q.emplace_schedule(sa.begin(), sa.end());
        }
        {
            std::vector<queue_type::schedule_in_type> sa;

            sa.emplace_back(std::chrono::milliseconds(000), []{std::cout << "A\n"; });
            sa.emplace_back(std::chrono::milliseconds(100), []{std::cout << "B\n"; });
            sa.emplace_back(std::chrono::milliseconds(200), []{std::cout << "C\n"; });
            sa.emplace_back(std::chrono::milliseconds(300), []{std::cout << "D\n"; });
            sa.emplace_back(std::chrono::milliseconds(400), []{std::cout << "E\n"; });
        
            q.emplace_schedule(T0, sa.begin(), sa.end());
        }
        {
            std::vector<queue_type::event_type> sa;
            sa.emplace_back([]{std::cout << " I1\n"; });
            sa.emplace_back([]{std::cout << " I2\n"; });
            sa.emplace_back([]{std::cout << " I3\n"; });
            sa.emplace_back([]{std::cout << " I4\n"; });
            sa.emplace_back([]{std::cout << " I5\n"; });

            q.emplace_schedule(sa.begin(), sa.end());
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
        return true;
    }
};

int main() {
    return !(ScheduleTest().Run());
}
