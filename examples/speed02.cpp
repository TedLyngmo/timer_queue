#include "lyn/timer_queue.hpp"

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

using queue_t = lyn::mq::timer_queue<void()>;
using queue_reg_t = lyn::mq::timer_queue_registrator<queue_t>;

using namespace std::chrono_literals;

void producer(queue_reg_t reg, std::atomic<int>& events_processed) {
    auto& queue = reg.queue();

    while(queue) {
        queue.emplace_do([&events_processed]{ ++events_processed; });
    }
}

void consumer(queue_reg_t reg) {
    auto& queue = reg.queue();
    queue_reg_t::event_type eve;
    queue_reg_t::event_container evec;

    while(queue.wait_pop_all(evec)) {
        // lock free event processing
        while(evec.pop(eve)) {
            eve();
        }
    }
}
int main() {
    std::atomic<int> events_processed = 0;

    std::thread th_cons;
    std::thread th_prod1;
    //std::thread th_prod2;

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now(), end;
    {
        queue_t queue;

        th_cons = std::thread(consumer, queue_reg_t(queue));
        start = std::chrono::steady_clock::now();
        th_prod1 = std::thread(producer, queue_reg_t(queue), std::ref(events_processed));
        //th_prod2 = std::thread(producer, queue_reg_t(queue), std::ref(events_processed));
        auto tick = start + 1s;

        for(int i = 1; i < 30; ++i, tick += 1s) {
            std::this_thread::sleep_until(tick);

            std::cout << '\r' << std::setw(15) << events_processed / i << " events/s" << std::flush;
        }
        std::cout << "\nEvents in queue: " << queue.size() << '\n';
    }
    end = std::chrono::steady_clock::now();

    //th_prod2.join();
    th_prod1.join();
    th_cons.join();
}
