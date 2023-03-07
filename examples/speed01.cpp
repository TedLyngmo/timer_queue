#include "lyn/timer_queue.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

using queue_t = lyn::mq::timer_queue<void()>;
using queue_reg_t = lyn::mq::timer_queue_registrator<queue_t>;

int events_processed = 0;

void event_handler(queue_reg_t reg) {
    auto& queue = reg.queue();
    queue_reg_t::event_type ev;

    while(queue.wait_pop(ev)) {
        ev();
    }
}

auto speed(std::thread& th) {
    queue_t queue;
    th = std::thread(event_handler, queue_reg_t(queue));

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now(), end;
    for(int i = 0; i < 10000000; ++i) {
        queue.emplace_do_urgently([] { ++events_processed; });
    }
    queue.shutdown();
    end = std::chrono::steady_clock::now();
    return end - start;
}

int main() {
    std::thread th;

    auto elapsed = std::chrono::duration<double>(speed(th));

    std::cout << std::fixed << std::setprecision(2) << "Processed " << events_processed << " events with a speed of "
              << events_processed / elapsed.count() << " events/second.\n";

    th.join();
}
