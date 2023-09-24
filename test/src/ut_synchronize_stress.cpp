#include "lyn/timer_queue.hpp"

#include "include/jthread.hpp"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>

using namespace lyn::mq;
using queue_t = timer_queue<void()>;

namespace { // put all in an anonymous namespace

std::uintmax_t count{};
constexpr std::uintmax_t iterations = 10000;
constexpr std::size_t threads = 10;

// classes / functions

void bgt(timer_queue_registrator<queue_t> reg) {
    auto& queue = reg.queue();

    for(std::uintmax_t i = 0; i < iterations; ++i) {
        auto res = queue.synchronize<void>([] { ++count; });
        if(not res) break;
    }
}

} // namespace

namespace ut_synchronize_stress { // the namespace have the name of the file without ".cpp"
int main() {
    {
        queue_t tq;
        jthread ths[threads];
        for(auto& th : ths) {
            th = jthread(bgt, std::ref(tq));
        }

        queue_t::event_type ev;

        while(tq.wait_pop(ev)) {
            ev();
            if(count == threads * iterations) tq.shutdown();
        }
    }
    std::cout << count << '\n';

    return !(count == threads * iterations);
}
} // namespace ut_synchronize_stress
