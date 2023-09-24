#include "lyn/timer_queue.hpp"

#include "include/jthread.hpp"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

//#define DEBUG

using namespace lyn::mq;
using queue_t = timer_queue<void()>;

namespace { // put all in an anonymous namespace

#ifdef DEBUG
std::mutex mtx;
template<class... Args>
void print(Args&&... args) {
    std::ostringstream os;
    os.copyfmt(std::cout);
    (..., (os << ' ' << args));
    os << '\n';
    std::lock_guard lock(mtx);
    std::cout << os.str();
}
#else
template<class... Args>
void print(Args&&...) {
}
#endif

std::uintmax_t count{};
constexpr std::uintmax_t iterations = 10000;
constexpr std::uintmax_t throwiters = 10000;
constexpr std::size_t threads = 10;

// classes / functions

void bgt(timer_queue_registrator<queue_t> reg) {
    auto& queue = reg.queue();

    for(std::uintmax_t i = 0; queue && i < iterations; ++i) {
        print(std::this_thread::get_id(), "Adding:", queue.size());
        auto res = queue.synchronize<void>([] {
            if(++count % throwiters == 0) throw std::runtime_error("time to die");
        });
        print(std::this_thread::get_id(), "Result:", res);
    }
}

} // namespace

namespace ut_synchronize_exceptions { // the namespace have the name of the file without ".cpp"
int main() {
    std::cout << std::boolalpha;
    {
        queue_t tq;
        jthread ths[threads];
        for(auto& th : ths) {
            th = jthread(bgt, std::ref(tq));
        }

        queue_t::event_type ev;

        try {
            while(tq.wait_pop(ev)) {
                ev();
                if(count == threads * iterations) tq.shutdown();
            }
        } catch(const std::exception& ex) {
            print("Exception:", ex.what());
        }
        tq.shutdown();
        print("Clearing");
        tq.clear();
        print("Waiting for clients...");
    }
    print(count);

    return !(count == throwiters);
}
} // namespace ut_synchronize_exceptions
