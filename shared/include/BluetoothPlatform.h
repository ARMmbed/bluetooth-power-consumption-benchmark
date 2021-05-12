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

#ifndef BLUETOOTHPLATFORM_H
#define BLUETOOTHPLATFORM_H

#include <cstddef>
#include <cstdint>
#include <functional>

struct BluetoothPlatform {
    /// Connection role, main or peripheral.
    enum class connection_role_t {
        peripheral,
        main
    };

    /// Handle type. May be casted to the platform's handle type and dereferenced if the platform uses value typed
    /// handles.
    using handle_t = void*;

    /// Event raised when advertising starts.
    struct AdvertisingStartEvent {
        AdvertisingStartEvent(uint32_t durationMs_, bool isPeriodic_, uint32_t periodicIntervalMs);

        /// The duration of advertising in ms.
        uint32_t durationMs;

        /// Indicates whether periodic advertising is present.
        bool isPeriodic;

        /// The periodic advertising interval in ms.
        uint32_t periodicIntervalMs;
    };

    /// Event raised when advertising report received.
    struct AdvertisingReportEvent {
        AdvertisingReportEvent(
            int32_t sid_,
            uint8_t peerAddressType_,
            const uint8_t *peerAddressData_,
            size_t peerAddressSize_,
            const char *localName_,
            bool isPeriodic_,
            uint32_t periodicIntervalMs_
        );

        /// The SID.
        uint8_t sid;

        /// The peer address type (platform defined, e.g. ble::peer_address_type_t on mbed).
        uint8_t peerAddressType;

        /// The peer address data (bytes).
        const uint8_t *peerAddressData;

        /// The peer address size.
        size_t peerAddressSize;

        /// The local device name.
        const char *localName;

        /// Indicates whether periodic advertising is present.
        bool isPeriodic;

        /// The periodic advertising interval in ms.
        uint32_t periodicIntervalMs;
    };

    struct ScanStartEvent {
        ScanStartEvent(uint32_t scanDurationMs_);

        uint32_t scanDurationMs;
    };

    /// Event raised when connected.
    struct ConnectEvent {
        ConnectEvent(
            uint8_t peerAddressType_,
            const uint8_t *peerAddressData_,
            size_t peerAddressSize_,
            intmax_t error_,
            connection_role_t role_,
            handle_t connectionHandle_
        );

        /// The peer address type (platform defined, e.g. ble::peer_address_type_t on mbed).
        uint8_t peerAddressType;

        /// The peer address data (bytes).
        const uint8_t *peerAddressData;

        /// The peer address size.
        size_t peerAddressSize;

        /// The platform-defined error code.
        intmax_t error;

        /// The connection role.
        connection_role_t role;

        /// The paltform-defined connection handle.
        handle_t connectionHandle;
    };

    /// Event raised when synced with periodic advertising.
    struct PeriodicSyncEvent  {
        PeriodicSyncEvent(
            int32_t sid_,
            uint8_t peerAddressType_,
            const uint8_t *peerAddressData_,
            size_t peerAddressSize_,
            intmax_t error_,
            connection_role_t role_,
            handle_t syncHandle_
        );

        /// The SID.
        uint8_t sid;

        /// The peer address type (platform defined, e.g. ble::peer_address_type_t on mbed).
        uint8_t peerAddressType;

        /// The peer address data (bytes).
        const uint8_t *peerAddressData;

        /// The peer address size.
        size_t peerAddressSize;

        /// The platform-defined error code.
        intmax_t error;

        /// The connection role.
        connection_role_t role;

        /// The platform-defined sync handle.
        handle_t syncHandle;
    };

    /// Interface for event handlers.
    struct EventHandler {
        /// Called when advertising starts.
        virtual void onAdvertisingStart(const AdvertisingStartEvent &event) {}

        /// Called when scanning starts.
        virtual void onScanStart(const ScanStartEvent &event) {}

        /// Called when an advertising report is available.
        virtual void onAdvertisingReport(const AdvertisingReportEvent &event) {}

        /// Called when advertising ends due to timeout.
        virtual void onAdvertisingTimeout() {}

        /// Called when scan ends due to timeout.
        virtual void onScanTimeout() {}

        /// Called when connection is established.
        virtual void onConnection(const ConnectEvent &event) {}

        /// Called upon disconnect.
        virtual void onDisconnect() {}

        /// Called when periodic sync is established.
        virtual void onPeriodicSync(const PeriodicSyncEvent &event) {}

        /// Called upon loss of periodic sync.
        virtual void onSyncLoss() {}
    };

    virtual ~BluetoothPlatform() {}

    /// Gets the event handler or nullptr if not present.
    EventHandler *getEventHandler();

    /// Sets the event handler. Passing nullptr unsets.
    void setEventHandler(EventHandler *eh);

    /// Perform any needed initialisation, then call callback.
    virtual int init(const std::function<void()>& callback) = 0;

    /// Run the event loop (does not return).
    virtual void runEventLoop() = 0;

    /// Call a function e.g. using an event queue to avoid stack overflow.
    virtual void call(const std::function<void()>& fn) { fn(); }

    /// Call a function after interval elapses.
    virtual void callIn(uint32_t millis, const std::function<void()> &fn) = 0;

    /// Print a platform-defined error code.
    virtual void printError(intmax_t error, const char* msg) = 0;

    /// Behaves like cstdio printf().
    virtual void printf(const char* fmt, ...) = 0;

    /// Behaves like cstdio getchar().
    virtual int getchar() = 0;

    /// Behaves like cstdio putchar().
    virtual void putchar(int c) = 0;

    /// Gets the local device name.
    virtual const char *deviceName() const = 0;

    /// Indicates whether extended advertising is supported (feature test).
    virtual bool isPeriodicAdvertisingAvailable() = 0;

    /// Initiates advertising.
    virtual int startAdvertising() = 0;

    /// Initiates periodic advertising.
    virtual int startPeriodicAdvertising() = 0;

    /// Initiates scanning.
    virtual int startScan() = 0;

    /// Initiates scanning for periodic advertising.
    virtual int startScanForPeriodicAdvertising() = 0;

    /// Establish a connection with the given peer.
    virtual int establishConnection(uint8_t peerAddressType, const uint8_t* peerAddress) = 0;

    /// Sync to peer's periodic advertising.
    virtual int syncToPeriodicAdvertising(
        int32_t sid,
        uint8_t peerAddressType,
        const uint8_t* peerAddress,
        uint32_t syncTimeoutMs
    ) = 0;

    /// Trigger disconnection.
    virtual int disconnect(handle_t connection_handle) = 0;

    /// Stop periodic sync.
    virtual int stopSync(handle_t sync_handle) = 0;

protected:
    BluetoothPlatform() {}

private:
    static EventHandler _default_handler;
    EventHandler *_event_handler;
};

#endif // ! BLUETOOTHPLATFORM_H
