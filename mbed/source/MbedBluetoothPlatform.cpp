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
#include <cstdarg>
#include <limits>

#include <ble/BLE.h>

#include <BluetoothPlatform.h>
#include <MbedBluetoothPlatform.h>

using ble::BLE;

MbedBluetoothPlatform::MbedBluetoothPlatform(BLE &ble, events::EventQueue &eq)
: _ble(ble)
, _event_queue(eq)
, _adv_data_builder(_adv_buffer)
{}

MbedBluetoothPlatform::~MbedBluetoothPlatform()
{
    if (_ble.hasInitialized()) {
        _ble.shutdown();
    }
}

void MbedBluetoothPlatform::onInitComplete(BLE::InitializationCompleteCallbackContext *event)
{
    if (event->error) {
        printError(event->error, "Error during the initialisation");
        return;
    }

    getEventHandler()->onInitComplete();
}

int MbedBluetoothPlatform::commonStartAdvertising()
{
    _is_scanner = false;
    _is_connecting_or_syncing = false;

    auto error = _ble.gap().startAdvertising(_adv_handle, _advertise_time);
    if (error) {
        printError(error, "Gap::startAdvertising() failed");
    }

    return error;
}

int MbedBluetoothPlatform::commonStartScan()
{
    _is_scanner = true;
    _is_connecting_or_syncing = false;

    ble::ScanParameters scan_params;
    scan_params.setOwnAddressType(ble::own_address_type_t::RANDOM);

    ble_error_t error = _ble.gap().setScanParameters(scan_params);
    if (error) {
        printError(error, "Gap::setScanParameters failed");
        return error;
    }

    error = _ble.gap().startScan(_scan_time);
    if (error) {
        printError(error, "Gap::startScan failed");
        return error;
    }

    auto eh = getEventHandler();
    if (eh) {
        eh->onScanStart(ScanStartEvent(_scan_time.valueInMs()));
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BluetoothPlatform overrides
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char *MbedBluetoothPlatform::deviceName() const
{
    return "Power Consumption (mbed)";
}

void MbedBluetoothPlatform::getLocalAddress(uint8_t buf[6])
{
    ble::own_address_type_t addr_type;
    ble::address_t address;
    BLE::Instance().gap().getAddress(addr_type, address);

    memcpy(buf, address.data(), 6);
}

int MbedBluetoothPlatform::init()
{
    _ble.gap().setEventHandler(this);
    ble_error_t error = _ble.init(this, &MbedBluetoothPlatform::onInitComplete);
    if (error) {
        printError(error, "Error returned by BLE::init");
    }

    return static_cast<int>(error);
}

void MbedBluetoothPlatform::runEventLoop()
{
    _event_queue.dispatch_forever();
}

void MbedBluetoothPlatform::call(BluetoothPlatform::callback_t fn, void* arg)
{
    _event_queue.call(fn, arg);
}

void MbedBluetoothPlatform::callIn(uint32_t millis, BluetoothPlatform::callback_t fn, void* arg)
{
    assert(millis < std::numeric_limits<int>::max());
    _event_queue.call_in(std::chrono::milliseconds(millis), fn, arg);
}

void MbedBluetoothPlatform::printError(intmax_t error, const char *msg)
{
    print_error(static_cast<ble_error_t>(error), msg);
}

void MbedBluetoothPlatform::printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}

int MbedBluetoothPlatform::getchar()
{
    return ::getchar();
}

void MbedBluetoothPlatform::putchar(int c)
{
    ::putchar(c);
    fflush(stdout);
}

bool MbedBluetoothPlatform::isPeriodicAdvertisingAvailable()
{
    return _ble.gap().isFeatureSupported(ble::controller_supported_features_t::LE_EXTENDED_ADVERTISING)
        && _ble.gap().isFeatureSupported(ble::controller_supported_features_t::LE_PERIODIC_ADVERTISING)
    ;
}

int MbedBluetoothPlatform::startAdvertising()
{
    _is_periodic = false;
    if (_adv_handle != ble::LEGACY_ADVERTISING_HANDLE) {
        _adv_handle = ble::LEGACY_ADVERTISING_HANDLE;
        _adv_data_builder.setFlags();
        _adv_data_builder.setName(deviceName());
        auto error = _ble.gap().setAdvertisingPayload(_adv_handle, _adv_data_builder.getAdvertisingData());
        if (error) {
            printError(error, "Gap::setAdvertisingPayload() failed");
            return error;
        }
    }

    return commonStartAdvertising();
}

int MbedBluetoothPlatform::startPeriodicAdvertising()
{
    // Perform feature test.
    if (!isPeriodicAdvertisingAvailable()) {
        printf("Periodic advertising not supported, cannot run test.\r\n");
        return -1;
    }

    _is_periodic = true;

    // Only perform this setup when we are starting fresh or have previously performed legacy advertising.
    if (_adv_handle == ble::INVALID_ADVERTISING_HANDLE || _adv_handle == ble::LEGACY_ADVERTISING_HANDLE) {
        // Set parameters and payload with a new advertising set.
        ble::AdvertisingParameters adv_parameters(ble::advertising_type_t::NON_CONNECTABLE_UNDIRECTED);
        adv_parameters.setUseLegacyPDU(false);
        auto error = _ble.gap().createAdvertisingSet(&_adv_handle, adv_parameters);
        if (error) {
            printError(error, "Gap::createAdvertisingSet() failed");
            return error;
        }

        error = _ble.gap().setAdvertisingParameters(_adv_handle, adv_parameters);
        if (error) {
            printError(error, "Gap::setAdvertisingParameters() failed");
            return error;
        }

        _adv_data_builder.setFlags();
        _adv_data_builder.setName(deviceName());
        error = _ble.gap().setAdvertisingPayload(_adv_handle, _adv_data_builder.getAdvertisingData());
        if (error) {
            printError(error, "Gap::setAdvertisingPayload() failed");
            return error;
        }
    }

    return commonStartAdvertising();
}

int MbedBluetoothPlatform::startScan()
{
    _is_periodic = false;
    return commonStartScan();
}

int MbedBluetoothPlatform::startScanForPeriodicAdvertising()
{
    _is_periodic = true;
    return commonStartScan();
}

int MbedBluetoothPlatform::establishConnection(uint8_t peerAddressType, const uint8_t *peerAddress)
{
    ble_error_t error = _ble.gap().connect(
        static_cast<ble::peer_address_type_t::type>(peerAddressType),
        ble::address_t(peerAddress),
        ble::ConnectionParameters()
    );
    if (error) {
        printError(error, "Gap::connect failed");
    }

    return error;
}

int MbedBluetoothPlatform::syncToPeriodicAdvertising(
    int32_t sid,
    uint8_t peerAddressType,
    const uint8_t *peerAddress,
    uint32_t syncTimeoutMs
)
{
    ble_error_t error = _ble.gap().createSync(
        static_cast<ble::peer_address_type_t::type>(peerAddressType),
        ble::address_t(peerAddress),
        sid,
        2,
        ble::sync_timeout_t(ble::millisecond_t(syncTimeoutMs))
    );
    if (error) {
        printError(error, "Gap::createSync failed");
    }

    return error;
}

int MbedBluetoothPlatform::disconnect(handle_t connection_handle)
{
    assert(connection_handle != nullptr);
    auto error = _ble.gap().disconnect(
        *reinterpret_cast<ble::connection_handle_t*>(connection_handle),
        ble::local_disconnection_reason_t(ble::local_disconnection_reason_t::USER_TERMINATION)
    );
    if (error) {
        printError(error, "Gap::disconnect failed");
    }

    return error;
}

int MbedBluetoothPlatform::stopSync(handle_t sync_handle)
{
    assert(sync_handle != nullptr);
    auto error = _ble.gap().terminateSync(*reinterpret_cast<ble::periodic_sync_handle_t*>(sync_handle));
    if (error) {
        printError(error, "Gap::disconnect failed");
    }

    return error;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ble::Gap overrides
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MbedBluetoothPlatform::onAdvertisingStart(const ble::AdvertisingStartEvent &event)
{
    if (_is_periodic) {
        // Start periodic advertising.
        auto error = _ble.gap().setPeriodicAdvertisingParameters(
            _adv_handle,
            ble::periodic_interval_t(_periodic_interval.valueInMs()/2),
            ble::periodic_interval_t(_periodic_interval.valueInMs()*2)
        );
        if (error) {
            printError(error, "Gap::setPeriodicAdvertisingParameters() failed");
            return;
        }

        error = _ble.gap().startPeriodicAdvertising(_adv_handle);
        if (error) {
            printError(error, "Gap::startPeriodicAdvertising() failed");
            return;
        }
    }

    getEventHandler()->onAdvertisingStart(
        AdvertisingStartEvent(
            _advertise_time.valueInMs(),
            _is_periodic,
            _periodic_interval.valueInMs()
        )
    );
}

char *get_name_of_peer(const ble::AdvertisingReportEvent &event)
{
    ble::AdvertisingDataParser adv_parser(event.getPayload());
    while (adv_parser.hasNext()) {
        ble::AdvertisingDataParser::element_t field = adv_parser.next();
        if (field.type == ble::adv_data_type_t::COMPLETE_LOCAL_NAME) {
            char *buffer = new char[field.value.size() + 1];
            memcpy(buffer, field.value.data(), field.value.size());
            buffer[field.value.size()] = 0;
            return buffer;
        }
    }

    return nullptr;
}

void MbedBluetoothPlatform::onAdvertisingReport(const ble::AdvertisingReportEvent &event)
{
    if (_is_connecting_or_syncing) {
        return;
    }

    auto name = get_name_of_peer(event);
    getEventHandler()->onAdvertisingReport(
        AdvertisingReportEvent(
            static_cast<int32_t>(event.getSID()),
            event.getPeerAddressType().value(),
            event.getPeerAddress().data(),
            event.getPeerAddress().size(),
            const_cast<const char*>(name),
            event.isPeriodicIntervalPresent(),
            event.isPeriodicIntervalPresent() ? event.getPeriodicInterval().valueInMs() : 0UL
        )
    );

    delete[] name;
}

void MbedBluetoothPlatform::onAdvertisingEnd(const ble::AdvertisingEndEvent &event)
{
    if (!_is_connecting_or_syncing) {
        getEventHandler()->onAdvertisingTimeout();
    }
}

void MbedBluetoothPlatform::onScanTimeout(const ble::ScanTimeoutEvent&)
{
    if (!_is_connecting_or_syncing) {
        getEventHandler()->onScanTimeout();
    }
}

void MbedBluetoothPlatform::onConnectionComplete(const ble::ConnectionCompleteEvent &event)
{
    auto eh = getEventHandler();
    if (eh == nullptr) {
        return;
    }

    _is_connecting_or_syncing = true;

    auto handle = event.getConnectionHandle();
    eh->onConnection(
        ConnectEvent(
            event.getPeerAddressType().value(),
            event.getPeerAddress().data(),
            event.getPeerAddress().size(),
            static_cast<intmax_t>(event.getStatus()),
            _is_scanner ? connection_role_t::main : connection_role_t::peripheral,
            reinterpret_cast<handle_t>(&handle)
        )
    );
}

void MbedBluetoothPlatform::onDisconnectionComplete(const ble::DisconnectionCompleteEvent &event)
{
    // Don't raise event if already disconnected.
    if (!_is_connecting_or_syncing) {
        return;
    }

    _is_connecting_or_syncing = false;

    getEventHandler()->onDisconnect();
}

void MbedBluetoothPlatform::onPeriodicAdvertisingSyncEstablished(const ble::PeriodicAdvertisingSyncEstablishedEvent &event)
{
    auto eh = getEventHandler();
    if (eh == nullptr) {
        return;
    }

    _is_connecting_or_syncing = true;

    auto handle = event.getSyncHandle();
    eh->onPeriodicSync(
        PeriodicSyncEvent(
            static_cast<int32_t>(event.getSid()),
            event.getPeerAddressType().value(),
            event.getPeerAddress().data(),
            event.getPeerAddress().size(),
            static_cast<intmax_t>(event.getStatus()),
            _is_scanner ? connection_role_t::main : connection_role_t::peripheral,
            reinterpret_cast<handle_t>(&handle)
        )
    );
}

void MbedBluetoothPlatform::onPeriodicAdvertisingSyncLoss(const ble::PeriodicAdvertisingSyncLoss &event)
{
    if (!_is_connecting_or_syncing) {
        return;
    }

    _is_connecting_or_syncing = false;

    getEventHandler()->onSyncLoss();
}
