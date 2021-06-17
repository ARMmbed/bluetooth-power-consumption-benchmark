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

#include <stddef.h>
#include <stdint.h>

#include <BluetoothPlatform.h>

BluetoothPlatform::EventHandler BluetoothPlatform::_default_handler;

BluetoothPlatform::EventHandler *BluetoothPlatform::getEventHandler()
{
    return _event_handler;
}

void BluetoothPlatform::setEventHandler(BluetoothPlatform::EventHandler *eh)
{
    if (eh == nullptr) {
        _event_handler = &_default_handler;
    } else {
        _event_handler = eh;
    }
}

BluetoothPlatform::AdvertisingStartEvent::AdvertisingStartEvent(
    uint32_t durationMs_,
    bool isPeriodic_,
    uint32_t periodicIntervalMs_
)
: durationMs(durationMs_)
, isPeriodic(isPeriodic_)
, periodicIntervalMs(periodicIntervalMs_)
{}

BluetoothPlatform::AdvertisingReportEvent::AdvertisingReportEvent(
    int32_t sid_,
    uint8_t peerAddressType_,
    const uint8_t *peerAddressData_,
    size_t peerAddressSize_,
    const char *localName_,
    bool isPeriodic_,
    uint32_t periodicIntervalMs_
)
: sid(sid_)
, peerAddressType(peerAddressType_)
, peerAddressData(peerAddressData_)
, peerAddressSize(peerAddressSize_)
, localName(localName_)
, isPeriodic(isPeriodic_)
, periodicIntervalMs(periodicIntervalMs_)
{}

BluetoothPlatform::ScanStartEvent::ScanStartEvent(uint32_t scanDurationMs_)
: scanDurationMs(scanDurationMs_)
{}

BluetoothPlatform::PeriodicSyncEvent::PeriodicSyncEvent(
    int32_t sid_,
    uint8_t peerAddressType_,
    const uint8_t *peerAddressData_,
    size_t peerAddressSize_,
    intmax_t error_,
    connection_role_t role_,
    handle_t syncHandle_
)
: sid(sid_)
, peerAddressType(peerAddressType_)
, peerAddressData(peerAddressData_)
, peerAddressSize(peerAddressSize_)
, error(error_)
, role(role_)
, syncHandle(syncHandle_)
{}

BluetoothPlatform::ConnectEvent::ConnectEvent(
    uint8_t peerAddressType_,
    const uint8_t *peerAddressData_,
    size_t peerAddressSize_,
    intmax_t error_,
    BluetoothPlatform::connection_role_t role_,
    BluetoothPlatform::handle_t connectionHandle_
)
: peerAddressType(peerAddressType_)
, peerAddressData(peerAddressData_)
, peerAddressSize(peerAddressSize_)
, error(error_)
, role(role_)
, connectionHandle(connectionHandle_)
{}


BluetoothPlatform::ConnectEvent::ConnectEvent(intmax_t error_) : error(error_)
{}
