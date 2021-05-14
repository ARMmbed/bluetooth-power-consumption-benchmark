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

#include <inttypes.h>

#include <zephyr.h>

static size_t total_newed = 0;

static void out_of_memory(size_t count)
{
    printk(
        "OOM: Allocation of size %" PRIu64 " failed (%" PRIu64 " alloc'd in total)\n",
        static_cast<uint64_t>(count),
        static_cast<uint64_t>(total_newed + count)
    );
    while (true) {}
}

void* operator new(size_t count)
{
    auto p = k_malloc(count);
    if (!p) {
        out_of_memory(count);
    }

    total_newed += count;
    return p;
}

void* operator new[](size_t count)
{
    auto p = k_malloc(count);
    if (!p) {
        out_of_memory(count);
    }

    total_newed += count;
    return p;
}

void operator delete(void* ptr)
{
    k_free(ptr);
}

void operator delete[](void* ptr)
{
    k_free(ptr);
}
