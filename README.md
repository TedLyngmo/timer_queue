# timer\_queue
A header-only C++17 priority queue based on `Clock::time_point`s

## Classes

Classes defined in header [`lyn/timer_queue.hpp`](include/lyn/timer_queue.hpp).

### `lyn::mq::timer_queue`

---
```
template<
    class R, class... Args
    class Clock = std::chrono::steady_clock,
    class TimePoint = std::chrono::time_point<Clock>
> class timer_queue; /* undefined */

template<class R, class... Args, class Clock, class TimePoint>
class timer_queue<R(Args...), Clock, TimePoint>;
```

---
A timer queue provides constant time lookup of the first event to timeout, at the expense of logarithmic insertion and extraction.

#### Template parameters

|Parameter|Description|
|-:|:-|
|         **R** | The return type of events to store in the queue.                              |
|   **Args...** | The arguments to pass to events stored in the queue.                          |
|     **Clock** | The clock type used to keep time. `std::chrono::steady_clock` by default.     |
| **TimePoint** | `std::chrono::time_point<Clock>`                                              |

|Member types| Definitions |
|-:|:-|
| `event_type`       | The type you extract in the event loop |
| `clock_type`       | Clock                                  |
| `duration`         | Clock::duration                        |
| `time_point`       | TimePoint                              |
| `event_container`       | _unspecified_ - Has a member function `bool pop(event_type& ev)` - see `wait_pop_all` |
| `schedule_at_type` | `std::pair<time_point, event_type>` |
| `schedule_in_type` | `std::pair<duration, event_type>` |


---

| ![](./svg/spacer.svg)<br>Public member functions | |
|-|-|
|`timer_queue()` | Constructs the `timer_queue` with zero delay for events added using `emplace_do` |
|`explicit timer_queue(duration now_delay)` | Constructs the `timer_queue` with `now_delay` delay for events added using `emplace_do` |
|`timer_queue(const timer_queue&) = delete`||
|`timer_queue(timer_queue&&) = delete`||
|`timer_queue& operator=(const timer_queue&) = delete`||
|`timer_queue& operator=(timer_queue&&) = delete`||
|`~timer_queue()` | Destroys the `timer_queue` after waiting for all registered users to have unregistered |

| ![](./svg/spacer.svg)<br>Functions to add single events | |
|-|-|
|`void emplace_do(event_type ev)`| Add an event that is due after `now_delay`. This is also affected by `set_delay_until` (see below). |
|`void emplace_do_urgently(event_type ev)`| Add an event, placing the event last among those added with `emplace_do_urgently`, but before all other events in queue |
|`void emplace_do_at(time_point tp, event_type ev)` | Add an event that is due at the specified `time_point` |
|`void emplace_do_in(duration dur, event_type)` | Add an event that is due after the specified `duration` |

| ![](./svg/spacer.svg)<br>Functions to add events in bulk | |
|-|-|
|`template<class Iter>`<br>`void emplace_schedule(Iter first, Iter last)` | Place a number of events in queue that are due after `now_delay`. This overload only participates in overload resolution if `std::iterator_traits<Iter>::value_type` is `event_type`. This overload is also affected by `set_delay_until` (see below). |
|`template<class Iter>`<br>`void emplace_schedule(Iter first, Iter last)` | Place a number of events in queue. This overload only participates in overload resolution if `std::iterator_traits<Iter>::value_type` is `schedule_at_type`.|
|`template<class Iter>`<br>`void emplace_schedule(Iter first, Iter last)` | Place a number of events in queue in relation to `clock_type::now()`. This overload only participates in overload resolution if `std::iterator_traits<Iter>::value_type` is `schedule_in_type`.|
|`template<class Iter>`<br>`void emplace_schedule(time_point T0, Iter first, Iter last)` | Place a number of events in queue in relation to `T0`. This overload only participates in overload resolution if `std::iterator_traits<Iter>::value_type` is `schedule_in_type`.|

| ![](./svg/spacer.svg)<br>Functions to perform synchronized tasks | |
|-|-|
|`template<class Re, class Func>`<br>`Re synchronize(Func&& func)`|Execute `func`, that should return `Re`, in the task queue and wait for the execution to complete. This overload only participates in overload resolution if `R` is `void`.|
|`template<class Re, class Func>`<br>`Re synchronize(Func&& func, R&& event_loop_return_value = R{})`|Execute `func`, that should return `Re`, in the task queue and wait for the execution to complete. The value returned by the event when executed in the event loop is stored in `event_loop_return_value`. This overload only participates in overload resolution if `R` is not `void`.|


| ![](./svg/spacer.svg)<br>Functions to extract events | |
|-|-|
|`bool wait_pop(event_type& ev)` | Wait until an event is due and populate `ev`. Returns `true` if an event was successfully extracted or `false` if the queue was shutdown. |
|`bool wait_pop_all(event_container& in_out)` | Wait until an event is due and extracts all events that are due. Returns `true` unless the queue was shutdown in which case it returns `false`. Use `in_out.pop(event_type&)` to extract events in a lock-free way. If there is only one thread processing events put in the queue and adding events to the queue is only done using `emplace_do` or `emplace_schedule` (where `std::iterator_traits<Iter>::value_type` is `event_type`), using `wait_pop_all` may be more efficient than extracting one event at a time with `wait_pop`. |

| ![](./svg/spacer.svg)<br>Misc. rarely used | |
|-|-|
|`void set_delay_until(time_point tp)`|Delay all events added with `emplace_do` and `emplace_schedule` (where `std::iterator_traits<Iter>::value_type` is `event_type`) until the supplied `time_point`. Events added while `tp` has not yet occured will be processed in the order they were added, only delayed until after `tp`.|
| `void shutdown()` | Shutdown the queue, leaving unprocessed events in the queue |
| `void clear()` | Removes unprocessed events from the queue |
| `void restart()` | Restarts the queue with unprocessed events intact |
| `std::size_t size() const` | Returns the number of events in queue |
| `bool operator!() const` | Returns `true` if `shutdown()` has been called, `false` otherwise |
| `bool is_open() const` | Returns `true` if `shutdown()` has _not_ been called, `false`otherwise |
| `explicit operator bool() const` | Returns the same as `is_open()` |

| ![](./svg/spacer.svg)<br>Queue registration | Usually only used by `lyn::mq::timer_queue_registrator` |
|-|-|
| `timer_queue& reg()` | Registers a user (usually a thread) of the `timer_queue`. |
| `void unreg()` | Unregisters a user (usually a thread) of the `timer_queue`. |

---

### `lyn::mq::timer_queue_registrator`

```
template<class QT>
class timer_queue_registrator;
```
A `timer_queue_registrator` is a RAII wrapper used to register a user (usually a thread) to a `timer_queue` and to unregister from the `timer_queue` when the `timer_queue_registrator` is destroyed.

#### Template parameters

|Parameter|Description|
|-:|:-|
|         **QT** | The specific `timer_queue` type|

|Member types| Definitions |
|-:|:-|
| `timer_queue`       | QT |
| `event_type`       | `timer_queue::event_type` |
| `event_container`       | `timer_queue::event_container` |

| ![](./svg/spacer.svg)<br>Public member functions | |
|-|-|
|`timer_queue_registrator() = delete` | |
|`timer_queue_registrator(queue_type& tq)`|Constructs the `timer_queue_registrator` and registers to the supplied `timer_queue`|
|`timer_queue_registrator(std::reference_wrapper<queue_type> tqrw)`|Constructs the `timer_queue_registrator` and registers to the wrapped `timer_queue`|
|`timer_queue_registrator(const timer_queue_registrator&) = delete`||
|`timer_queue_registrator(timer_queue_registrator&& other) noexcept`|A `timer_queue_registrator` is move-constructible|
|`timer_queue_registrator& operator=(const timer_queue_registrator&) = delete`||
|`timer_queue_registrator& operator=(timer_queue_registrator&& other) noexcept`|A `timer_queue_registrator` is move-assignable|
|`~timer_queue_registrator()`|Unregisters from the `timer_queue` supplied at construction|

| ![](./svg/spacer.svg)<br>Observers ||
|-|-|
|`queue_type& queue()`| Returns a reference to the `timer_queue` supplied at construction|

---

## Performance
#### One thread adding events and one thread processing events

|Compiler |OS        |CPU  |events / second|
|:-------:|----------|-----|--------------:|
|g++12    |Fedora 37 |Intel(R) Xeon(R) CPU E5-2430 0 @ 2.20GHz| 700,000 - 1,400,000|
|clang++15|Fedora 37 |Intel(R) Xeon(R) CPU E5-2430 0 @ 2.20GHz| 1,200,000 - 1,500,000|
|g++12    |Ubuntu 22.04 WSL<br>Windows 11|Intel(R) Core(TM) i9-7920X CPU @ 2.90GHz|1,350,000 - 1,600,000|
|clang++15|Ubuntu 22.04 WSL<br>Windows 11|Intel(R) Core(TM) i9-7920X CPU @ 2.90GHz|1,400,000 - 1,500,000|
|VS 17.5.0<br>MSVC 19.35|Windows 11|Intel(R) Core(TM) i9-7920X CPU @ 2.90GHz|2,300,000 - 2,700,000|
