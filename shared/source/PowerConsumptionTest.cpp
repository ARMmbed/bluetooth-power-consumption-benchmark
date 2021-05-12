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

#include <cassert>
#include <cctype>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <functional>

#include "bt_test_state.h"
#include "PowerConsumptionTest.h"

PowerConsumptionTest::PowerConsumptionTest(BluetoothPlatform &platform) : _platform(platform)
{}

PowerConsumptionTest::~PowerConsumptionTest()
{
    // Do nothing.
}

void PowerConsumptionTest::run()
{
    _platform.setEventHandler(this);
    _platform.init([this]() {nextState();});
    _platform.runEventLoop();
}

void PowerConsumptionTest::nextState()
{
    updateState(bt_test_state_t::START);
    _platform.printf(
        "Enter one of the following commands:\r\n"
        " * a - Advertise\r\n"
        " * s - Scan \r\n"
        " * p - Toggle periodic adv/scan flag (currently %s)\r\n"
        " * m - Set/unset peer MAC address to connect by MAC instead of name\n",
        _is_periodic ? "ON" : "OFF"
    );
    while (true) {
        _platform.printf("Enter command: ");
        int c = _platform.getchar();
        _platform.putchar(c);
        switch (tolower(c)) {
            case 'a': advertise();                              return;
            case 's': scan();                                   return;
            case 'p': togglePeriodic();                         return;
            case 'm': readTargetMac();                          return;
            case '\n':                                          break;
            default:  _platform.printf("\r\nInvalid choice. "); break;
        }
    }
}

void PowerConsumptionTest::advertise()
{
    if (_is_periodic) {
        _platform.startPeriodicAdvertising();
    } else {
        _platform.startAdvertising();
    }
}

void PowerConsumptionTest::scan()
{
    if (_is_periodic) {
        _platform.startScanForPeriodicAdvertising();
    } else {
        _platform.startScan();
    }
}

void PowerConsumptionTest::togglePeriodic()
{
    _is_periodic = !_is_periodic;
    _platform.printf("\r\nPeriodic mode toggled %s\r\n", _is_periodic ? "ON" : "OFF");
    _platform.call([this]() { nextState(); });
}

void PowerConsumptionTest::readTargetMac()
{
    char buffer[MAC_ADDRESS_LENGTH + 1];
    size_t length = 0;
    memset(buffer, 0, sizeof(buffer));

    _platform.printf(
        "\r\n * Set target MAC by inputting 6 hex bytes (12 digits) with optional : separators"
        "\r\n * Unset target MAC and use name to match by pressing ENTER with no input"
        "\r\nTarget MAC: "
    );
    int xdigits = 0;
    do {
        // Break on '\n', append hex digit, ignore other chars.
        int c = tolower(_platform.getchar());
        if (c == '\n') {
            break;
        } else if (isxdigit(c)) {
            buffer[length] = c;
            length++;
            xdigits++;
            _platform.putchar(c);
        }

        // Insert a colon when needed.
        if (xdigits == 2 && length < MAC_ADDRESS_LENGTH) {
            _platform.putchar(':');
            xdigits = 0;
        }
    } while (length < MAC_ADDRESS_LENGTH);

    if (length == 0) {
        _platform.printf("Will look for peer with name \"%s\"\r\n", _platform.deviceName());
    } else if (length == MAC_ADDRESS_LENGTH) {
        _platform.printf("\r\nWill look for peer with MAC \"%s\"\r\n", buffer);
        _target_mac_len = length;
        memcpy(_target_mac, buffer, length);
    } else {
        _platform.printf("\r\nInvalid MAC \"%s\"\r\n", buffer);
    }

    _platform.call([this]() { nextState(); });
}

void PowerConsumptionTest::updateState(bt_test_state_t state)
{
    if (state != _state && state != bt_test_state_t::START) {
        print_bt_test_state(state, [this](const char *s) { _platform.printf("\r\n#%s\r\n", s); });
    }

    _state = state;
}

bool PowerConsumptionTest::isPeriodic() const
{
    return _is_periodic;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BluetoothPlatform::EventHandler overrides
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void PowerConsumptionTest::onAdvertisingStart(const BluetoothPlatform::AdvertisingStartEvent &event)
{
    updateState(bt_test_state_t::ADVERTISE);
    if (event.isPeriodic) {
        _platform.printf(
            "Periodic advertising for %" PRIu32 " started with interval %" PRIu32 "ms\r\n",
            event.durationMs,
            event.periodicIntervalMs
        );
    } else {
        _platform.printf("Advertising started for %" PRIu32 "ms\r\n", event.durationMs);
    }
}

void PowerConsumptionTest::onScanStart(const BluetoothPlatform::ScanStartEvent &event)
{
    updateState(bt_test_state_t::SCAN);
    printf("Scanning started for %" PRIu32 "ms\r\n", event.scanDurationMs);
}

void PowerConsumptionTest::onAdvertisingReport(const BluetoothPlatform::AdvertisingReportEvent &event)
{
    bool match = false;

    // Format MAC as a string.
    const uint8_t* mac_raw = event.peerAddressData;
    assert(event.peerAddressSize == MAC_ADDRESS_LENGTH/2);
    char mac[MAC_ADDRESS_LENGTH + 1];
    sprintf(mac, "%02x%02x%02x%02x%02x%02x", mac_raw[5], mac_raw[4], mac_raw[3], mac_raw[2], mac_raw[1], mac_raw[0]);

    // Match by MAC or by name.
    if (_target_mac_len > 0 && memcmp(mac, _target_mac, _target_mac_len) == 0) {
        _platform.printf("Peer matched by MAC\r\n");
        match = true;
    } else if (_target_mac_len == 0 && strcmp(_platform.deviceName(), event.localName) == 0) {
        _platform.printf("Peer matched by name\r\n");
        match = true;
    }

    if (!match) {
        return;
    }

    // Connect or sync to the peer.
    if (event.isPeriodic) {
        printf(
            "Syncing with peer \"%s\" (%s) with SID %d and periodic interval %" PRIu32 "ms\r\n",
            event.localName,
            mac,
            event.sid,
            event.periodicIntervalMs
        );

        _platform.syncToPeriodicAdvertising(
            event.sid,
            event.peerAddressType,
            event.peerAddressData,
            5000
        );
    } else {
        printf("Connecting to peer \"%s\" (%s)\r\n", event.localName, mac);
        _platform.establishConnection(
            event.peerAddressType,
            event.peerAddressData
        );
    }
}

void PowerConsumptionTest::onAdvertisingTimeout()
{
    updateState(bt_test_state_t::START);
    _platform.printf("Advertising timed out\r\n");
    _platform.call([this]() {nextState();});
}

void PowerConsumptionTest::onScanTimeout()
{
    updateState(bt_test_state_t::START);
    _platform.printf("Scanning timed out\r\n");
    _platform.call([this]() {nextState();});
}

void PowerConsumptionTest::onConnection(const BluetoothPlatform::ConnectEvent &event)
{
    if (event.error) {
        _platform.printError(event.error, "Connection failed");
        return;
    }

    _platform.printf("Connected to peer as ");
    if (event.role == BluetoothPlatform::connection_role_t::main) {
        _platform.printf("main\r\n");
        updateState(bt_test_state_t::CONNECT_MAIN);
        // Trigger disconnect after timeout when connected as main.
        _platform.callIn(
            10000,
            [this, event]()
            {
                _platform.printf("Triggering disconnect...\r\n");
                _platform.disconnect(event.connectionHandle);
                nextState();
            }
        );
    } else {
        _platform.printf("peripheral\r\n");
        updateState(bt_test_state_t::CONNECT_PERIPHERAL);
    }
}

void PowerConsumptionTest::onDisconnect()
{
    _platform.printf("Disconnected\r\n");
    _platform.call([this]() {nextState();});
}

void PowerConsumptionTest::onPeriodicSync(const BluetoothPlatform::PeriodicSyncEvent &event)
{
    if (event.error) {
        _platform.printError(event.error, "Sync with periodic advertising failed");
    } else {
       _platform.printf("Synced with periodic advertising\r\n");
    }

    _platform.callIn(
        10000,
        [this, event]()
        {
            _platform.printf("Ending sync...\r\n");
            _platform.stopSync(event.syncHandle);
            nextState();
        }
    );
}

void PowerConsumptionTest::onSyncLoss()
{
    _platform.printf("Periodic sync lost\r\n");
    _platform.call([this]() {nextState();});
}
