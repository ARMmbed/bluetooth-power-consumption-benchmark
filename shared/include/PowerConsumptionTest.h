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
#include "BluetoothPlatform.h"

struct PowerConsumptionTest : protected BluetoothPlatform::EventHandler {
    static constexpr size_t MAC_ADDRESS_LENGTH = 2*6; // Six 2-digit bytes.

    PowerConsumptionTest(BluetoothPlatform &platform);

    PowerConsumptionTest(const PowerConsumptionTest &) = delete;

    ~PowerConsumptionTest();

    void run();

protected:
    void onAdvertisingStart(const BluetoothPlatform::AdvertisingStartEvent &event) override;
    void onAdvertisingReport(const BluetoothPlatform::AdvertisingReportEvent &event) override;
    void onAdvertisingTimeout() override;

    void onScanStart(const BluetoothPlatform::ScanStartEvent &event) override;
    void onScanTimeout() override;

    void onConnection(const BluetoothPlatform::ConnectEvent &event) override;
    void onDisconnect() override;

    void onPeriodicSync(const BluetoothPlatform::PeriodicSyncEvent &event) override;
    void onSyncLoss() override;

private:
    /// Enter next state according to operator input.
    void nextState();

    /// Start advertising.
    void advertise();

    /// Start scanning.
    void scan();

    /// Handles the `p` command to toggle the period flag.
    void togglePeriodic();

    /// Handles the `m` command to set/unset target MAC address.
    void readTargetMac();

    /// Called when state transitions.
    void updateState(bt_test_state_t state);

    /// Get is_periodic flag.
    bool isPeriodic() const;
private:
    BluetoothPlatform &_platform;
    char _target_mac[MAC_ADDRESS_LENGTH + 1];
    size_t _target_mac_len = 0;
    bt_test_state_t _state;
    bool _is_periodic = false;
};

#endif // ! TEST_BASE_H
