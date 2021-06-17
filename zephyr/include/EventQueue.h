/* mbed Microcontroller Library
 * Copyright (c) 2006-2021 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef EVENTQUEUE_H
#define EVENTQUEUE_H

#include <stdint.h>
#include <time.h>

#include <zephyr.h>

/// Event queue with a similar interface to mbed EventQueue.
/// Callbacks are scheduled in the order (1) that they become ready, and (2) that they arrive. Meaning that if two
/// callbacks become ready at the same time, the one which was scheduled first runs first.
struct EventQueue {
    typedef void( *callback_t)(void *);

    ~EventQueue();

    /// Add an event to be dispatched ASAP.
    void call(callback_t fn, void *arg);

    /// Schedule callback to be called after at least `millis` ms has passed.
    void call_in(uint32_t millis, callback_t fn, void *arg);

    /// Dispatch events continuously.
    void dispatch_forever();

private:
    struct Event {
        callback_t fn;
        void *arg;
        uint32_t millis;
        Event *next;

        Event(callback_t fn_, void *arg_, uint32_t millis_);

        uint32_t elapsed();

        bool ready();

        void call();
    private:
        int64_t _start_time;
    };

    Event *_head;
    Event *_tail;

    // Append a new Event.
    void append(callback_t fn, void *arg, uint32_t millis);

    // Remove and deallocate the node after prev. If `prev == node` then the head node is removed.
    void removeNode(Event *prev, Event *node);
};

#endif // ! EVENTQUEUE_H
