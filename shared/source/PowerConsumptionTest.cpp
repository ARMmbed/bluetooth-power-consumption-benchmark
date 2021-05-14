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
#include <ctype.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <bt_test_state.h>
#include <config.h>
#include <PowerConsumptionTest.h>

PowerConsumptionTest::PowerConsumptionTest(BluetoothPlatform &platform) : _platform(platform)
{}

PowerConsumptionTest::~PowerConsumptionTest()
{
    // Do nothing.
}

void PowerConsumptionTest::callNextState(void* arg)
{
    reinterpret_cast<PowerConsumptionTest*>(arg)->nextState();
}

void PowerConsumptionTest::run()
{
    _platform.init(this);
    _platform.runEventLoop();
}

void PowerConsumptionTest::nextState()
{
    updateState(bt_test_state_t::START);
    _platform.printf(
        "Enter one of the following commands:\n"
        " * a - Advertise\n"
        " * s - Scan\n"
        " * p - Toggle periodic adv/scan flag (currently %s)\n"
        " * m - Set/unset peer MAC address to connect by MAC instead of name\n",
        _is_periodic ? "ON" : "OFF"
    );
    while (true) {
        _platform.printf("Enter command: ");
        int c = _platform.getchar();
        _platform.putchar(c);
        switch (tolower(c)) {
            case 'a': advertise();      return;
            case 's': scan();           return;
            case 'p': togglePeriodic(); return;
            case 'm': readTargetMac();  return;
            default:
                if (isprint(c)) {
                    _platform.printf("Invalid choice \'%c\'. ", c);
                }
                break;
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
#if CONFIG_USE_PER_ADV_SYNC
    _is_periodic = !_is_periodic;
    _platform.printf("\nPeriodic mode toggled %s\n", _is_periodic ? "ON" : "OFF");
#else
    _platform.printf("\nProgram was not compiled with support for periodic sync\n");
#endif

    _platform.call(&callNextState, this);
}

void PowerConsumptionTest::readTargetMac()
{
    char buffer[MAC_ADDRESS_LENGTH + 1];
    size_t length = 0;
    memset(buffer, 0, sizeof(buffer));

    _platform.printf(
        "\n * Set target MAC by inputting 6 hex bytes (12 digits) with optional : separators"
        "\n * Unset target MAC and use name to match by pressing ENTER with no input"
        "\nTarget MAC: "
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
        _platform.printf("Will look for peer with name \"%s\"\n", _platform.deviceName());
    } else if (length == MAC_ADDRESS_LENGTH) {
        _platform.printf("\nWill look for peer with MAC \"%s\"\n", buffer);
        _target_mac_len = length;
        memcpy(_target_mac, buffer, length);
    } else {
        _platform.printf("\nInvalid MAC \"%s\"\n", buffer);
    }

    _platform.call(&callNextState, this);
}

void PowerConsumptionTest::callPrintf(void* arg, const char* s)
{
    reinterpret_cast<PowerConsumptionTest*>(arg)->_platform.printf(s);
}

void PowerConsumptionTest::updateState(bt_test_state_t state)
{
    if (state != _state) {
        _platform.printf("\n#");
        print_bt_test_state(state, &callPrintf, this);
        _platform.printf("\n");
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
void PowerConsumptionTest::onInitComplete()
{
    uint8_t mac[6];
    _platform.getLocalAddress(mac);
    _platform.printf(
        "#DEV - %s - %02x:%02x:%02x:%02x:%02x:%02x\n",
        _platform.deviceName(),
        mac[5],
        mac[4],
        mac[3],
        mac[2],
        mac[1],
        mac[0]
    );
    _platform.call(&callNextState, this);
}

void PowerConsumptionTest::onAdvertisingStart(const BluetoothPlatform::AdvertisingStartEvent &event)
{
    updateState(bt_test_state_t::ADVERTISE);
    if (event.isPeriodic) {
        _platform.printf(
            "Periodic advertising for %" PRIu32 " ms started with interval %" PRIu32 "ms\n",
            event.durationMs,
            event.periodicIntervalMs
        );
    } else {
        _platform.printf("Advertising started for %" PRIu32 "ms\n", event.durationMs);
    }
}

void PowerConsumptionTest::onScanStart(const BluetoothPlatform::ScanStartEvent &event)
{
    updateState(bt_test_state_t::SCAN);
    printf("Scanning started for %" PRIu32 "ms\n", event.scanDurationMs);
}

void PowerConsumptionTest::onAdvertisingReport(const BluetoothPlatform::AdvertisingReportEvent &event)
{
    // Format MAC as a string.
    const uint8_t *mac_raw = event.peerAddressData;
    assert(event.peerAddressSize == MAC_ADDRESS_LENGTH/2);
    char mac[MAC_ADDRESS_LENGTH + 1];
    sprintf(mac, "%02x%02x%02x%02x%02x%02x", mac_raw[5], mac_raw[4], mac_raw[3], mac_raw[2], mac_raw[1], mac_raw[0]);

    // Log the discovered peer if configured to do so.
#if CONFIG_LIST_SCAN_DEVS
    const char *name = event.localName[0] == 0 ? "(unknown name)" : event.localName;
    _platform.printf("Discovered \"%s\" (%s)\n", name, mac);
#endif

    // Match by MAC or by name.
    if (_target_mac_len > 0 && memcmp(mac, _target_mac, _target_mac_len) == 0) {
        _platform.printf("Peer matched by MAC\n");
    } else if (_target_mac_len == 0 && strcmp(_platform.deviceName(), event.localName) == 0) {
        _platform.printf("Peer matched by name\n");
    } else {
        return;
    }

    // Connect or sync to the peer.
    if (event.isPeriodic) {
        printf(
            "Syncing with peer \"%s\" (%s) with SID %d and periodic interval %" PRIu32 " ms\n",
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
        printf("Connecting to peer \"%s\" (%s)\n", event.localName, mac);
        _platform.establishConnection(
            event.peerAddressType,
            event.peerAddressData
        );
    }
}

void PowerConsumptionTest::onAdvertisingTimeout()
{
    updateState(bt_test_state_t::START);
    _platform.printf("Advertising timed out\n");
    _platform.call(&callNextState, this);
}

void PowerConsumptionTest::onScanTimeout()
{
    updateState(bt_test_state_t::START);
    _platform.printf("Scanning timed out\n");
    _platform.call(&callNextState, this);
}

struct DisconnectContext {
    PowerConsumptionTest* this_ptr;
    BluetoothPlatform::handle_t handle;

    DisconnectContext(PowerConsumptionTest* this_ptr_, BluetoothPlatform::handle_t handle_)
    : this_ptr(this_ptr_)
    , handle(handle_)
    {}
};

void PowerConsumptionTest::triggerDisconnect(void* arg)
{
    auto ctx = reinterpret_cast<DisconnectContext*>(arg);
    ctx->this_ptr->_platform.printf("Triggering disconnect...\n");
    ctx->this_ptr->_platform.disconnect(ctx->handle);
    ctx->this_ptr->nextState();
    delete ctx;
}

void PowerConsumptionTest::triggerDesync(void* arg)
{
    auto ctx = reinterpret_cast<DisconnectContext*>(arg);
    ctx->this_ptr->_platform.printf("Stopping sync...\n");
    ctx->this_ptr->_platform.stopSync(ctx->handle);
    ctx->this_ptr->nextState();
    delete ctx;
}

void PowerConsumptionTest::onConnection(const BluetoothPlatform::ConnectEvent &event)
{
    if (event.error) {
        _platform.printError(event.error, "Connection failed");
        return;
    }

    _platform.printf("Connected to peer as ");
    if (event.role == BluetoothPlatform::connection_role_t::main) {
        _platform.printf("main\n");
        updateState(bt_test_state_t::CONNECT_MAIN);
        // Trigger disconnect after timeout when connected as main.
        auto ctx = new DisconnectContext(this, event.connectionHandle); // Deleted by handler.
        _platform.callIn(CONFIG_CONNECT_TIME, &triggerDisconnect, ctx);
    } else {
        // Wait for disconnect when peripheral.
        _platform.printf("peripheral\n");
        updateState(bt_test_state_t::CONNECT_PERIPHERAL);
    }
}

void PowerConsumptionTest::onDisconnect()
{
    _platform.printf("Disconnected\n");
    _platform.call(&callNextState, this);
}

void PowerConsumptionTest::onPeriodicSync(const BluetoothPlatform::PeriodicSyncEvent &event)
{
    if (event.error) {
        _platform.printError(event.error, "Sync with periodic advertising failed");
    } else {
       _platform.printf("Synced with periodic advertising\n");
    }

    auto ctx = new DisconnectContext(this, event.syncHandle); // Deleted by handler.
    _platform.callIn(CONFIG_CONNECT_TIME, &triggerDesync, ctx);
}

void PowerConsumptionTest::onSyncLoss()
{
    _platform.printf("Periodic sync lost\n");
    _platform.call(&callNextState, this);
}
