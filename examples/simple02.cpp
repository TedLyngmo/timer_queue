#include "lyn/timer_queue.hpp"

#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

using queue_t = lyn::mq::timer_queue<void()>;
using queue_reg_t = lyn::mq::timer_queue_registrator<queue_t>;

void event_handler(queue_reg_t reg) {
    auto& queue = reg.queue();
    queue_reg_t::event_type ev;

    while(queue.wait_pop(ev)) {
        ev();
    }
    std::cout << "queue shutdown\n";
}

int main() {
    queue_t queue;
    std::thread th = std::thread(event_handler, std::ref(queue));

    for(int i = 0; i < 10; ++i) {
        queue.emplace_do_in(std::chrono::milliseconds(i * 100), [i] { std::cout << i << '\n'; });
    }

    std::this_thread::sleep_for(1s);

    // if queue goes out of scope after the thread, you need to explicitly shutdown:
    queue.shutdown();
    th.join();
}
