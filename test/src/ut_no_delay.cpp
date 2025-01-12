#include "lyn/timer_queue.hpp"

#include "include/jthread.hpp"

#include <iostream>
#include <thread>

namespace { // put all in an anonymous namespace

// classes / functions

} // namespace

namespace ut_no_delay { // the namespace have the name of the file without ".cpp"
int main() {
    lyn::mq::timer_queue<void(),std::chrono::steady_clock, std::chrono::steady_clock::time_point, false> queue;
    return EXIT_SUCCESS; // or EXIT_FAILURE
}
} // namespace ut_skel
