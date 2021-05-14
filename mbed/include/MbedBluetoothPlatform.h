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

#ifndef MBEDBLUETOOTHPLATFORM_H
#define MBEDBLUETOOTHPLATFORM_H

#include <cctype>
#include <chrono>
#include <inttypes.h>

#include "ble/BLE.h"
#include <events/mbed_events.h>
#include "pretty_printer.h"

#include "bt_test_state.h"
#include <BluetoothPlatform.h>
#include <config.h>
struct MbedBluetoothPlatform : BluetoothPlatform, protected ble::Gap::EventHandler {
    MbedBluetoothPlatform(ble::BLE &ble, events::EventQueue &eq);

    ~MbedBluetoothPlatform();

    const char *deviceName() const override;

    int init() override;

    void runEventLoop() override;

    void getLocalAddress(uint8_t buf[6]) override;

    void call(BluetoothPlatform::callback_t fn, void* arg) override;

    void callIn(uint32_t millis, BluetoothPlatform::callback_t fn, void* arg) override;

    void printError(intmax_t error, const char *msg) override;

    void printf(const char *fmt, ...) override;

    int getchar() override;

    void putchar(int c) override;

    bool isPeriodicAdvertisingAvailable() override;

    int startAdvertising() override;

    int startPeriodicAdvertising() override;

    int startScan() override;

    int startScanForPeriodicAdvertising() override;

    int establishConnection(uint8_t peerAddressType, const uint8_t *peerAddress) override;

    int syncToPeriodicAdvertising(
        int32_t sid,
        uint8_t peerAddressType,
        const uint8_t *peerAddress,
        uint32_t syncTimeoutMs
    ) override;

    int disconnect(handle_t connection_handle) override;

    int stopSync(handle_t sync_handle) override;
protected:
    void onAdvertisingStart(const ble::AdvertisingStartEvent &event) override;

    void onAdvertisingEnd(const ble::AdvertisingEndEvent &event) override;

    void onAdvertisingReport(const ble::AdvertisingReportEvent &event) override;

    void onScanTimeout(const ble::ScanTimeoutEvent&) override;

    void onConnectionComplete(const ble::ConnectionCompleteEvent &event) override;

    void onDisconnectionComplete(const ble::DisconnectionCompleteEvent &event) override;

    void onPeriodicAdvertisingSyncEstablished(const ble::PeriodicAdvertisingSyncEstablishedEvent &event) override;

    void onPeriodicAdvertisingSyncLoss(const ble::PeriodicAdvertisingSyncLoss &event) override;

private:
    static constexpr uint16_t MAX_ADVERTISING_PAYLOAD_SIZE = 50;

    ble::scan_duration_t _scan_time = ble::scan_duration_t(CONFIG_SCAN_TIME);
    ble::adv_duration_t _advertise_time = ble::adv_duration_t(CONFIG_ADVERTISE_TIME);
    events::EventQueue::duration _connect_time = events::EventQueue::duration(CONFIG_CONNECT_TIME);
    ble::periodic_interval_t _periodic_interval = ble::adv_duration_t(CONFIG_PERIODIC_INTERVAL);

    BLE &_ble;
    events::EventQueue &_event_queue;

    uint8_t _adv_buffer[MAX_ADVERTISING_PAYLOAD_SIZE];
    ble::AdvertisingDataBuilder _adv_data_builder;

    ble::advertising_handle_t _adv_handle = ble::INVALID_ADVERTISING_HANDLE;

    bool _is_periodic = false;
    bool _is_scanner = false;
    bool _is_connecting_or_syncing = false;

    void scheduleEvents(BLE::OnEventsToProcessCallbackContext *context);
    void onInitComplete(BLE::InitializationCompleteCallbackContext *event);
    int commonStartAdvertising();
    int commonStartScan();
};

#endif // ! MBEDBLUETOOTHPLATFORM_H
