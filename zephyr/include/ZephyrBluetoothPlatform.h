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

#ifndef ZEPHYRBLUETOOTHPLATFORM_H
#define ZEPHYRBLUETOOTHPLATFORM_H

#include <stddef.h>
#include <stdint.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>

#include <BluetoothPlatform.h>
#include <EventQueue.h>

/// Implementation of BluetoothPlatform for Zephyr.
struct ZephyrBluetoothPlatform : BluetoothPlatform {
    /// Gets the instance. This class is a singleton to support interop with Zephyr (namely that callbacks don't accept
    /// a user-defined argument that could contain a `this` pointer).
    static ZephyrBluetoothPlatform &instance();

    ZephyrBluetoothPlatform(const ZephyrBluetoothPlatform &) = delete;

    int init() override;

    void runEventLoop() override;

    void getLocalAddress(uint8_t buf[6]) override;

    void call(BluetoothPlatform::callback_t fn, void *arg) override;

    void callIn(uint32_t millis, BluetoothPlatform::callback_t fn, void *arg) override;

    void printError(intmax_t error, const char *msg) override;

    void printf(const char *fmt, ...) override;

    int getchar() override;

    void putchar(int c) override;

    const char *deviceName() const override;

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

private:
    // Zephyr stuff.
    bt_le_ext_adv *_adv_set;
    bt_conn *_conn;
    bt_le_per_adv_sync *_sync;
    bt_conn_cb conn_callbacks = {
        .connected = &connectedCallback,
        .disconnected = &disconnectedCallback,
    };
    bt_le_scan_cb scan_callbacks = {
        .recv = &scanCallback
    };
    bt_le_per_adv_sync_cb sync_callbacks = {
        .synced = &syncedCallback,
        .term = &syncLostCallback
    };

    // Event queue.
    EventQueue _event_queue;

    // Flags.
    bool _is_scanner;
    bool _is_periodic;
    bool _is_scanning_or_advertising;
    bool _is_connecting_or_syncing;
    k_mutex _scan_sync_mutex;

    ZephyrBluetoothPlatform() = default;

    int startPeriodicAdvertising_Error(int error, const char* func);
    void cleanUpExtendedAdvertising();

    void endAdvertising();
    void endScan();

    static ZephyrBluetoothPlatform _instance;

    // Internal callbacks.
    static void endAdvertisingCallback(void *ignored);
    static void endScanCallback(void *ignored);

    // Zephyr callbacks.
    static void scanCallback(const bt_le_scan_recv_info *info, net_buf_simple *buf);
    static void connectedCallback(bt_conn *conn, uint8_t err);
    static void disconnectedCallback(bt_conn *conn, uint8_t reason);
    static void syncedCallback(bt_le_per_adv_sync *sync, bt_le_per_adv_sync_synced_info *info);
    static void syncLostCallback(bt_le_per_adv_sync *sync, const bt_le_per_adv_sync_term_info *info);
};

#endif // ! ZEPHYRBLUETOOTHPLATFORM_H
