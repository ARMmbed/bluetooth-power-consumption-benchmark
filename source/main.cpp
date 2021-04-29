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
#include <chrono>
#include <events/mbed_events.h>

#include "ble/BLE.h"
#include "pretty_printer.h"

/** This program enters different BLE states according to operator input, allowing power consumption to be
 * measured.
 */

using namespace std::literals::chrono_literals;

static const char DEVICE_NAME[] = "Power Consumption";
static const uint16_t MAX_ADVERTISING_PAYLOAD_SIZE = 50;

static const ble::scan_duration_t SCAN_TIME(1000);
static const events::EventQueue::duration CONNECT_TIME(10000);

static const ble::adv_duration_t ADVERTISE_TIME(1000);
static const ble::advertising_type_t PERIODIC_ADV_TYPE(ble::advertising_type_t::NON_CONNECTABLE_UNDIRECTED);
static const bool PERIODIC_ADV_PDU(false);

events::EventQueue event_queue;

class PowerConsumptionTest : private mbed::NonCopyable<PowerConsumptionTest>, public ble::Gap::EventHandler
{
public:
    PowerConsumptionTest(BLE& ble, events::EventQueue& event_queue) :
        _ble(ble),
        _event_queue(event_queue),
        _adv_data_builder(_adv_buffer)
    {
    }

    ~PowerConsumptionTest()
    {
        if (_ble.hasInitialized()) {
            _ble.shutdown();
        }
    }

    /** Start BLE interface initialisation */
    void run()
    {
        /* handle gap events */
        _ble.gap().setEventHandler(this);

        ble_error_t error = _ble.init(this, &PowerConsumptionTest::on_init_complete);
        if (error) {
            print_error(error, "Error returned by BLE::init");
            return;
        }

        /* this will not return until shutdown */
        _event_queue.dispatch_forever();
    }

private:
    /** This is called when BLE interface is initialised and starts the first mode */
    void on_init_complete(BLE::InitializationCompleteCallbackContext *event)
    {
        if (event->error) {
            print_error(event->error, "Error during the initialisation");
            return;
        }

        print_mac_address();
        next_state();
    }

    /** Enter next state according to operator input. */
    void next_state()
    {
        printf(
            "Select state:\r\n"
            " * Advertise\r\n"
            " * Scan \r\n"
            " * Periodic advertise\r\n"
        );

        while (true) {
            printf("Choose one [a/s/p]: ");
            fflush(stdout);
            int c = getchar();
            putchar(c);
            switch (tolower(c)) {
                case 'a': advertise();                    return;
                case 's': scan();                         return;
                case 'p': advertise_periodic();           return;
                default:  printf("\r\nInvalid choice. "); break;
            }
        }
    }

    void enter_state(const char* name, bool is_scanner, bool is_periodic, bool is_connecting_or_syncing = false)
    {
        printf("\r\n#%s\r\n", name);
        _is_scanner = is_scanner;
        _is_periodic = is_periodic;
        _is_connecting_or_syncing = is_connecting_or_syncing;
    }

    /** Set up and start advertising */
    void advertise()
    {
        enter_state(__func__, false, false);
        ble_error_t error;

        // Set payload for legacy handle.        
        _adv_data_builder.setFlags();
        _adv_data_builder.setName(DEVICE_NAME);
        error = _ble.gap().setAdvertisingPayload(
            ble::LEGACY_ADVERTISING_HANDLE,
            _adv_data_builder.getAdvertisingData()
        );
        if (error) {
            print_error(error, "Gap::setAdvertisingPayload() failed");
            return;
        }

        // Start advertising with legacy handle.
        error = _ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE, ADVERTISE_TIME);
        if (error) {
            print_error(error, "Gap::startAdvertising() failed");
            return;
        }
    }

    /** Set up and start periodic advertising. */ 
    void advertise_periodic()
    {
        enter_state(__func__, false, true);

        // Perform feature test.
        if (!_ble.gap().isFeatureSupported(ble::controller_supported_features_t::LE_EXTENDED_ADVERTISING) ||
            !_ble.gap().isFeatureSupported(ble::controller_supported_features_t::LE_PERIODIC_ADVERTISING)) {
            printf("Periodic advertising not supported, cannot run test.\r\n");
            return;
        }

        ble_error_t error;

        // Set advertising parameters. We only do this once, as it allocates memory and we do not call 
        // destroyAdvertisingSet.
        if (!_have_adv_handle) {
            ble::AdvertisingParameters adv_parameters(PERIODIC_ADV_TYPE);
            adv_parameters.setUseLegacyPDU(PERIODIC_ADV_PDU);

            error = _ble.gap().createAdvertisingSet(
                &_adv_handle,
                adv_parameters
            );
            if (error) {
                print_error(error, "Gap::createAdvertisingSet() failed");
                return;
            }

            error = _ble.gap().setAdvertisingParameters(_adv_handle, adv_parameters);
            if (error) {
                print_error(error, "Gap::setAdvertisingParameters() failed");
                return;
            }

            // Set advertising payload.
            _adv_data_builder.setFlags();
            _adv_data_builder.setName(DEVICE_NAME);
            error = _ble.gap().setAdvertisingPayload(
                _adv_handle,
                _adv_data_builder.getAdvertisingData()
            );
            if (error) {
                print_error(error, "Gap::setAdvertisingPayload() failed");
                return;
            }

            _have_adv_handle = true;
        }

        // Start advertising. Periodic advertising will be enabled in onAdvertisingBegin.
        error = _ble.gap().startAdvertising(_adv_handle, ADVERTISE_TIME);
        if (error) {
            print_error(error, "Gap::startAdvertising() failed");
            return;
        }
    }

    /** Set up and start scanning */
    void scan()
    {
        enter_state(__func__, true, false);

        ble::ScanParameters scan_params;
        scan_params.setOwnAddressType(ble::own_address_type_t::RANDOM);

        ble_error_t error = _ble.gap().setScanParameters(scan_params);
        if (error) {
            print_error(error, "Gap::setScanParameters failed");
            return;
        }

        error = _ble.gap().startScan(SCAN_TIME);
        if (error) {
            print_error(error, "Gap::startScan failed");
            return;
        }

        printf("Scanning for %ums\r\n", SCAN_TIME.valueInMs());
    }

    void connect_peripheral()
    {
        enter_state(__func__, false, _is_periodic, true);
        printf("Connected as peripheral\r\n");
    }

    void connect_master()
    {
        enter_state(__func__, true, _is_periodic, true);
        printf("Connected as master\r\n");
    }

private:
    /* Gap::EventHandler */

    void onAdvertisingStart(const ble::AdvertisingStartEvent &event) override
    {
        if (_is_periodic) {
            ble_error_t error = _ble.gap().setPeriodicAdvertisingParameters(
                _adv_handle,
                ble::periodic_interval_t(100),
                ble::periodic_interval_t(1000)
            );
            if (error) {
                print_error(error, "Gap::setPeriodicAdvertisingParameters() failed");
                return;
            }

            error = _ble.gap().startPeriodicAdvertising(_adv_handle);
            if (error) {
                print_error(error, "Gap::startPeriodicAdvertising() failed");
                return;
            }

            printf("Periodic advertising for %ums\r\n", ADVERTISE_TIME.valueInMs());
        } else {
            printf("Advertising started for %ums\r\n", ADVERTISE_TIME.valueInMs());
        }
    }

    void onAdvertisingEnd(const ble::AdvertisingEndEvent &event) override
    {
        if (!_is_connecting_or_syncing) {
            printf("Advertise time elapsed\r\n");
            next_state();
        }
    }

    /** Look at scan payload to find a peer device and connect to it */
    void onAdvertisingReport(const ble::AdvertisingReportEvent &event) override
    {
        /* don't bother with analysing scan result if we're already connecting */
        if (_is_connecting_or_syncing) {
            return;
        }

        /* if we're looking for periodic advertising don't bother unless it's present */
        if (_is_periodic && !event.isPeriodicIntervalPresent()) {
            return;
        }

        ble::AdvertisingDataParser adv_parser(event.getPayload());

        /* parse the advertising payload, looking for a discoverable device */
        while (adv_parser.hasNext()) {
            ble::AdvertisingDataParser::element_t field = adv_parser.next();

            /* identify peer by name */
            if (field.type == ble::adv_data_type_t::COMPLETE_LOCAL_NAME &&
                field.value.size() == strlen(DEVICE_NAME) &&
                (memcmp(field.value.data(), DEVICE_NAME, field.value.size()) == 0)) {
                /* if we haven't established our roles connect, otherwise sync with advertising */
                if (_is_periodic) {
                    printf(
                        "We found the peer, syncing with SID %d and periodic interval %dms\r\n",
                        event.getSID(),
                        event.getPeriodicInterval().valueInMs()
                    );

                    ble_error_t error = _ble.gap().createSync(
                        event.getPeerAddressType(),
                        event.getPeerAddress(),
                        event.getSID(),
                        2,
                        ble::sync_timeout_t(ble::millisecond_t(5000))
                    );

                    if (error) {
                        print_error(error, "Error caused by Gap::createSync");
                        return;
                    }
                } else {
                    printf("We found the peer, connecting\r\n");

                    ble_error_t error = _ble.gap().connect(
                        event.getPeerAddressType(),
                        event.getPeerAddress(),
                        ble::ConnectionParameters() // use the default connection parameters
                    );

                    if (error) {
                        print_error(error, "Error caused by Gap::connect");
                        return;
                    }
                }

                /* we may have already scan events waiting to be processed
                 * so we need to remember that we are already connecting
                 * or syncing and ignore them */
                _is_connecting_or_syncing = true;

                return;
            }
        }
    }

    void onScanTimeout(const ble::ScanTimeoutEvent&) override
    {
        if (!_is_connecting_or_syncing) {
            printf("Scanning ended, failed to find peer\r\n");
            next_state();
        }
    }

    /** This is called by Gap to notify the application we connected */
    void onConnectionComplete(const ble::ConnectionCompleteEvent &event) override
    {
        if (event.getStatus() == BLE_ERROR_NONE) {
            printf("Connected to: ");
            print_address(event.getPeerAddress().data());
            if (_is_scanner) {
                _event_queue.call_in(
                    CONNECT_TIME,
                    [this, handle = event.getConnectionHandle()] {
                        _ble.gap().disconnect(
                            handle,
                            ble::local_disconnection_reason_t(ble::local_disconnection_reason_t::USER_TERMINATION)
                        );
                    }
                );

                connect_master();
            } else {
                connect_peripheral();
            }
        } else {
            printf("Failed to connect\r\n");
            next_state();
        }
    }

    /** This is called by Gap to notify the application we disconnected */
    void onDisconnectionComplete(const ble::DisconnectionCompleteEvent &event) override
    {
        printf("Disconnected\r\n");
        next_state();
    }

    /** Called when first advertising packet in periodic advertising is received. */
    void onPeriodicAdvertisingSyncEstablished(const ble::PeriodicAdvertisingSyncEstablishedEvent &event) override
    {
        if (event.getStatus() == BLE_ERROR_NONE) {
            printf("Synced with periodic advertising\r\n");
            _sync_handle = event.getSyncHandle();
        } else {
            printf("Sync with periodic advertising failed\r\n");
        }
    }

    /** Called when a periodic advertising sync has been lost. */
    void onPeriodicAdvertisingSyncLoss(const ble::PeriodicAdvertisingSyncLoss &event) override
    {
        printf("Sync to periodic advertising lost\r\n");
        _sync_handle = ble::INVALID_ADVERTISING_HANDLE;
        _event_queue.call(this, &PowerConsumptionTest::scan);
    }

private:
    BLE &_ble;
    events::EventQueue &_event_queue;

    uint8_t _adv_buffer[MAX_ADVERTISING_PAYLOAD_SIZE];
    ble::AdvertisingDataBuilder _adv_data_builder;

    ble::advertising_handle_t _adv_handle = ble::INVALID_ADVERTISING_HANDLE;
    ble::periodic_sync_handle_t _sync_handle = ble::INVALID_ADVERTISING_HANDLE;

    bool _is_connecting_or_syncing = false;
    bool _is_periodic = false;
    bool _is_scanner = false;
    bool _have_adv_handle = false;
};

/** Schedule processing of events from the BLE middleware in the event queue. */
void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *context)
{
    event_queue.call(Callback<void()>(&context->ble, &BLE::processEvents));
}

int main()
{
    BLE &ble = BLE::Instance();
    ble.onEventsToProcess(schedule_ble_events);

    PowerConsumptionTest app(ble, event_queue);
    app.run();

    return 0;
}
