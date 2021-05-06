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

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>

#include "bt_test_state.h"
#include "test_base.h"

PowerConsumptionTestBase::~PowerConsumptionTestBase()
{
    // Do nothing.
}

void PowerConsumptionTestBase::next_state()
{
    update_state(bt_test_state_t::START);
    printf(
        "Enter one of the following commands:\r\n"
        " * a - Advertise\r\n"
        " * s - Scan \r\n"
        " * p - Toggle periodic adv/scan flag (currently %s)\r\n"
        " * m - Set/unset peer MAC address to connect by MAC instead of name\n",
        _is_periodic ? "ON" : "OFF" 
    );

    while (true) {
        printf("Enter command: ");
        fflush(stdout);
        int c = getchar();
        putchar(c);
        switch (tolower(c)) {
            case 'a': advertise();                    return;
            case 's': scan();                         return;
            case 'p': toggle_periodic();              return;
            case 'm': read_target_mac();              return;
            case '\n':                                break;
            default:  printf("\r\nInvalid choice. "); break;
        }
    }
}

void PowerConsumptionTestBase::toggle_periodic()
{
    _is_periodic = !_is_periodic;
    printf("\r\nPeriodic mode toggled %s\r\n", _is_periodic ? "ON" : "OFF");
    call([this]() { next_state(); });
}

void PowerConsumptionTestBase::read_target_mac()
{
    char buffer[MAC_ADDRESS_LENGTH + 1];
    size_t length = 0;
    memset(buffer, 0, sizeof(buffer));

    printf(
        "\r\n* Set target MAC by inputting 6 hex bytes (12 digits) with optional : separators"
        "\r\n* Unset target MAC and use name to match by pressing ENTER with no input"
        "\r\nTarget MAC: "
    );
    fflush(stdout);
    int xdigits = 0;
    do {
        int c = tolower(getchar());
        // Break on '\n', append hex digit, ignore other chars.
        if (c == '\n') {
            break;
        } else if (isxdigit(c)) {
            buffer[length] = c;
            length++;
            xdigits++;
            putchar(c);
        }

        // Insert a colon when needed.
        if (xdigits == 2 && length < MAC_ADDRESS_LENGTH) {
            putchar(':');
            xdigits = 0;
        }
    } while (length < MAC_ADDRESS_LENGTH);

    if (length == 0) {
        printf("Will look for peer with name \"%s\"\r\n", device_name());
    } else if (length == MAC_ADDRESS_LENGTH) {
        printf("\r\nWill look for peer with MAC \"%s\"\r\n", buffer);
        _target_mac_len = length;
        memcpy(_target_mac, buffer, length);
    } else {
        printf("\r\nInvalid MAC \"%s\"\r\n", buffer);
    }

    call([this]() { next_state(); });
}

void PowerConsumptionTestBase::update_state(bt_test_state_t state)
{
    if (state != _state && state != bt_test_state_t::START) {
        print_bt_test_state(state, [](const char* s) { printf("\r\n#%s\r\n", s); });
    }

    _state = state;
    _is_scanner = (state == bt_test_state_t::SCAN) || (state == bt_test_state_t::CONNECT_MASTER);
    _is_connecting_or_syncing = (state == bt_test_state_t::CONNECT_PERIPHERAL) || (state == bt_test_state_t::CONNECT_MASTER);
}

void PowerConsumptionTestBase::is_connecting_or_syncing(bool next)
{
    _is_connecting_or_syncing = next;
}

bool PowerConsumptionTestBase::is_connecting_or_syncing() const
{
    return _is_connecting_or_syncing;
}

bool PowerConsumptionTestBase::is_periodic() const
{
    return _is_periodic;
}

bool PowerConsumptionTestBase::is_scanner() const
{
    return _is_scanner;
}

const char* PowerConsumptionTestBase::target_mac() const
{
    return _target_mac;
}

size_t PowerConsumptionTestBase::target_mac_len() const
{
    return _target_mac_len;
}

bool PowerConsumptionTestBase::is_matching_mac_address(const uint8_t* data)
{
    char buffer[MAC_ADDRESS_LENGTH + 1];
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%02x%02x%02x%02x%02x%02x", data[5], data[4], data[3], data[2], data[1], data[0]);
    return is_matching_mac_address(buffer);
}

bool PowerConsumptionTestBase::is_matching_mac_address(const char* buffer)
{
    return strncmp(buffer, _target_mac, MAC_ADDRESS_LENGTH) == 0;
}
