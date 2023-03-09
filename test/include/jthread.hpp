#pragma once

#include <thread>

// poor mans std::jthread for C++17
struct jthread : public std::thread {
    using std::thread::thread;

    //jthread() = default;

    jthread(jthread&&) = default;
    jthread& operator=(jthread&&) noexcept = default;

    ~jthread() {
        if(joinable()) join();
    }
};
