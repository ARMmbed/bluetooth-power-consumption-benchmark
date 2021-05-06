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

#ifndef TEST_BASE_H
#define TEST_BASE_H 1

#include <cstddef>
#include <functional>

#include "bt_test_state.h"

class PowerConsumptionTestBase {
public:
    static constexpr size_t MAC_ADDRESS_LENGTH = 2*6; // Six 2-digit bytes.

    virtual ~PowerConsumptionTestBase();
protected:
    /// Gets the local device name.
    virtual const char* device_name() const = 0;

    /// Initiates advertising.
    virtual void advertise() = 0;

    /// Initiates scanning.
    virtual void scan() = 0;

    /// Called when connected as peripheral.
    virtual void connect_peripheral() = 0;

    /// Called when connected as master.
    virtual void connect_master() = 0;

    /// Call a function e.g. using an event queue to avoid stack overflow.
    virtual void call(const std::function<void()>& fn) = 0;

    /// Enter next state according to operator input.
    void next_state();

    /// Handles the `p` command to toggle the period flag.
    void toggle_periodic();

    /// Handles the `m` command to set/unset target MAC address.
    void read_target_mac();

    /// Called when state transitions.
    void update_state(bt_test_state_t state);

    /// Set is_connecting_or_syncing flag.
    void is_connecting_or_syncing(bool next);

    /// Get is_connecting_or_syncing flag.
    bool is_connecting_or_syncing() const;
    
    /// Get is_periodic flag.
    bool is_periodic() const;
    
    /// Get is_scanner flag.
    bool is_scanner() const;

    /// Get target MAC or NULL.
    const char* target_mac() const;

    /// Get length of target MAC.
    size_t target_mac_len() const;

    /// Indicates whether the raw binary data matches the target MAC address _target_mac when formatted as string.
    bool is_matching_mac_address(const uint8_t* data);

    /// Indicates whether the formatted string matches the target MAC address _target_mac.
    bool is_matching_mac_address(const char* buffer);
private:
    char _target_mac[MAC_ADDRESS_LENGTH + 1];
    size_t _target_mac_len = 0;
    bt_test_state_t _state;
    bool _is_connecting_or_syncing = false;
    bool _is_periodic = false;
    bool _is_scanner = false;
};

#endif // ! TEST_BASE_H