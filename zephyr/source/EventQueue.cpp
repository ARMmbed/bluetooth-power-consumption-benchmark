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

#include <assert.h>
#include <inttypes.h>
#include <time.h>

#include <zephyr.h>

#include <EventQueue.h>

EventQueue::Event::Event(callback_t fn_, void *arg_, uint32_t millis_)
: fn(fn_)
, arg(arg_)
, millis(millis_)
, _start_time(k_uptime_get())
{}

uint32_t EventQueue::Event::elapsed()
{
    // For some reason, k_uptime_delta updates its argument, so when you call it in a loop, all deltas are zero.
    // This works around it to get the expected behaviour.
    auto tmp = _start_time;
    return static_cast<uint32_t>(k_uptime_delta(&tmp));
}

bool EventQueue::Event::ready()
{
    return elapsed() >= millis;
}

void EventQueue::Event::call()
{
    fn(arg);
}

EventQueue::~EventQueue()
{
    if (_head == nullptr) {
        assert(_tail == nullptr);
        return;
    }

    assert(_tail != nullptr);
    while (_head->next) {
        auto prev = _head;
        _head = _head->next;
        delete prev;
    }
}

void EventQueue::call(callback_t fn, void *arg)
{
    append(fn, arg, 0);
}

void EventQueue::call_in(uint32_t millis, callback_t fn, void *arg)
{
    append(fn, arg, millis);
}

void EventQueue::dispatch_forever()
{
    while (true) {
        auto prev = _head;
        auto node = _head;
        while (node) {
            if (node->ready()) {
                node->call();
                removeNode(prev, node);
            }

            prev = node;
            node = node->next;
        }
    }
}

void EventQueue::append(callback_t fn, void *arg, uint32_t millis)
{
    if (_head == nullptr) {
        assert(_tail == nullptr);
        _head = new Event(fn, arg, millis);
        _tail = _head;
    } else {
        assert(_tail != nullptr);
        _tail->next = new Event(fn, arg, millis);
        _tail = _tail->next;
    }
}

void EventQueue::removeNode(Event *prev, Event *node)
{
    if (prev == node) {
        assert(prev == _head);
        _head = _head->next;
    } else {
        assert(prev->next == node);
        prev->next = node->next;
    }

    delete node;
}
