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
#ifndef BT_STATE_H
#define BT_STATE_H 1

#include <functional>

#define BT_STATE_LIST(F)    \
    F(START)                \
    F(SCAN)                 \
    F(ADVERTISE)            \
    F(CONNECT_PERIPHERAL) \
    F(CONNECT_MAIN)

enum class bt_test_state_t {
#define BT_STATE_DEFINE_ENUM(NAME) NAME,
    BT_STATE_LIST(BT_STATE_DEFINE_ENUM)
#undef BT_STATE_DEFINE_ENUM
};

inline void print_bt_test_state(bt_test_state_t state, const std::function<void(const char *)> &print)
{
    switch (state) {
#define BT_STATE_SWITCH_CASE(NAME) case bt_test_state_t::NAME: print(#NAME); break;
        BT_STATE_LIST(BT_STATE_SWITCH_CASE)
#undef BT_STATE_SWITCH_CASE
    }
}

#undef BT_STATE_LIST

#endif // ! BT_STATE_H
