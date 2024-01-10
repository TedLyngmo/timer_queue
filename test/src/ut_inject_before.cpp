#include "lyn/timer_queue.hpp"

#include "include/jthread.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>

using namespace lyn::mq;
using queue_t = timer_queue<void()>;

namespace { // put all in an anonymous namespace

// classes / functions

void bgt(timer_queue_registrator<queue_t> reg, bool short_delay) {
    auto& queue = reg.queue();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if(short_delay) {
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        //std::cout << "injecting first event to fire in queue\n";
        queue.emplace_do_in(std::chrono::milliseconds(500), []{std::cout << "one\n"; });
    } else {
        //std::cout << "putting last event to fire in queue\n";
        queue.emplace_do_in(std::chrono::milliseconds(2900), [&queue]{std::cout << "two\n"; queue.shutdown(); });
    }
}

} // namespace

namespace ut_inject_before { // the namespace have the name of the file without ".cpp"
int main() {
    std::chrono::steady_clock::time_point start;
    std::array<std::chrono::steady_clock::time_point, 2> tps;
    {
        queue_t tq;
        std::array<jthread, 2> ths;
        std::size_t tpn = 0;

        bool short_delay = false;
        for(auto& th : ths) {
            th = jthread(bgt, std::ref(tq), short_delay);
            short_delay = true;
        }
        start = std::chrono::steady_clock::now();

        queue_t::event_type ev;
        while(tq.wait_pop(ev)) {
            ev();
            tps[tpn++] = std::chrono::steady_clock::now();
        }
    }
    // the events should have fired at 1s and 3s +- 500µs so we divide by 10 and compare with 100 and 300 ms.
    auto first = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::round<std::chrono::milliseconds>(tps[0] - start) / 10);
    auto second = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::round<std::chrono::milliseconds>(tps[1] - start) / 10);
    std::cout << first.count() << "ms\n" << second.count() << "ms\n";
    return !(first.count() == 100 && second.count() == 300);
}
} // namespace ut_inject_before
