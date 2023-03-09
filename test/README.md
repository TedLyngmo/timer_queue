#### What's what
All `src/ut_*.cpp` files will be compiled into two executables.

* `test-g++` - Compiled with `-fsanitize=address,undefined,leak`
* `test-clang++` - Compiled with `-stdlib=libc++` and `-fsanitize=thread`

Both are executed and expected to run clean.

#### Building and running the tests
```
make -j
```
If it doesn't end with `OK` either compilation or execution failed. Clean compilation is a part of the test.

#### Adding a test

Copy `ut_skel.cpp` (shown below) and hack away. You return `EXIT_SUCCESS` or `EXIT_FAILURE` to indicate if your test succeeded or failed.
```c++
#include "lyn/timer_queue.hpp"

#include "include/jthread.hpp" // A C++17 version of an auto-joining thread. 

#include <iostream>
#include <thread>

namespace { // put all in an anonymous namespace

// test specific classes / functions

} // namespace

namespace ut_skel { // the namespace must have the name of the file without ".cpp"
int main() {
    return EXIT_SUCCESS; // or EXIT_FAILURE
}
} // namespace ut_skel
```
#### Rant
I detest violating (presumably) good code to make it testable and adding polymorphism to classes that don't actually need it in production for the sake of making them testable is abhorrent.

Perhaps I'll look into a proper framework that doesn't require any of that, but for now, the tests are here written in a home-cooked way.

