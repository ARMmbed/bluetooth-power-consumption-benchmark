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
#include <inttypes.h>

#include "ble/BLE.h"
#include "pretty_printer.h"
#include "bt_test_state.hpp"

/** This program enters different BLE states according to operator input, allowing power consumption to be
 * measured.
 */

using namespace std::literals::chrono_literals;

static const char DEVICE_NAME[] = "Power Consumption";
static const uint16_t MAX_ADVERTISING_PAYLOAD_SIZE = 50;
static constexpr size_t MAC_ADDRESS_LENGTH = 2*6+5; // 6 2-digit bytes + 5 optional colon separators.

events::EventQueue event_queue;

class PowerConsumptionTest : public ble::Gap::EventHandler {
public:
    PowerConsumptionTest(BLE& ble, events::EventQueue& eq) :
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
                default:  printf("\r\nInvalid choice. "); break;
            }
        }
    }

    void toggle_periodic()
    {
        _is_periodic = !_is_periodic;
        printf("\r\nPeriodic mode toggled %s\r\n", _is_periodic ? "ON" : "OFF");
        _event_queue.call(this, &PowerConsumptionTest::next_state);
    }

    void read_target_mac()
    {
        _target_mac_len = 0;
        memset(_target_mac, 0, sizeof(_target_mac));

        printf(
            "\r\n* Set target MAC by inputting 6 hex bytes (12 digits) with optional : separators"
            "\r\n* Unset target MAC and use name to match by pressing ENTER"
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
                _target_mac[_target_mac_len] = c;
                _target_mac_len++;
                xdigits++;
                putchar(c);
            }

            // Insert a colon when needed.
            if (xdigits == 2 && _target_mac_len < MAC_ADDRESS_LENGTH) {
                _target_mac[_target_mac_len] = ':';
                _target_mac_len++;
                xdigits = 0;
                putchar(':');
            }
        } while (_target_mac_len < MAC_ADDRESS_LENGTH);

        if (_target_mac_len == 0) {
            printf("Will look for peer with name \"%s\"\r\n", DEVICE_NAME);
        } else if (_target_mac_len == MAC_ADDRESS_LENGTH) {
            printf("\r\nWill look for peer with MAC \"%s\"\r\n", _target_mac);
        } else {
            printf("\r\nInvalid MAC \"%s\"\r\n", _target_mac);
        }

        _event_queue.call(this, &PowerConsumptionTest::next_state);
    }

    void update_state(bt_test_state_t state)
    {
        if (state != _state) {
            print_bt_test_state(state, [](const char* s) { printf("\r\n#%s\r\n", s); });
        }

        _state = state;
        _is_scanner = (state == bt_test_state_t::SCAN) || (state == bt_test_state_t::CONNECT_MASTER);
        _is_connecting_or_syncing = (state == bt_test_state_t::CONNECT_PERIPHERAL) || (state == bt_test_state_t::CONNECT_MASTER);
    }

    bool have_periodic_advertising()
    {
        return _ble.gap().isFeatureSupported(ble::controller_supported_features_t::LE_EXTENDED_ADVERTISING)
            && _ble.gap().isFeatureSupported(ble::controller_supported_features_t::LE_PERIODIC_ADVERTISING)
        ;
    }

    /** Set up and start advertising */
    void advertise()
    {
        update_state(bt_test_state_t::ADVERTISE);
        ble_error_t error;
        if (_is_periodic) {
            // Perform feature test.
            if (!have_periodic_advertising()) {
                printf("Periodic advertising not supported, cannot run test.\r\n");
                return;
            }

            // Set advertising parameters. We only do this once, as it allocates memory and we do not call 
            // destroyAdvertisingSet.
            if (!_have_adv_handle) {
                ble::AdvertisingParameters adv_parameters(ble::advertising_type_t::NON_CONNECTABLE_UNDIRECTED);
                adv_parameters.setUseLegacyPDU(false);

                error = _ble.gap().createAdvertisingSet(&_adv_handle, adv_parameters);
                if (error) {
                    print_error(error, "Gap::createAdvertisingSet() failed");
                    return;
                }

                error = _ble.gap().setAdvertisingParameters(_adv_handle, adv_parameters);
                if (error) {
                    print_error(error, "Gap::setAdvertisingParameters() failed");
                    return;
                }

                _have_adv_handle = true;
            }
        } else {
            // Use legacy advertising handle.
            _adv_handle = ble::LEGACY_ADVERTISING_HANDLE;
        }

        // Set advertising payload.
        _adv_data_builder.setFlags();
        _adv_data_builder.setName(DEVICE_NAME);
        error = _ble.gap().setAdvertisingPayload(_adv_handle, _adv_data_builder.getAdvertisingData());
        if (error) {
            print_error(error, "Gap::setAdvertisingPayload() failed");
            return;
        }

        // Start advertising.
        error = _ble.gap().startAdvertising(_adv_handle, _advertise_time);
        if (error) {
            print_error(error, "Gap::startAdvertising() failed");
            return;
        }
    }

    /** Set up and start scanning */
    void scan()
    {
        update_state(bt_test_state_t::SCAN);

        ble::ScanParameters scan_params;
        scan_params.setOwnAddressType(ble::own_address_type_t::RANDOM);

        ble_error_t error = _ble.gap().setScanParameters(scan_params);
        if (error) {
            print_error(error, "Gap::setScanParameters failed");
            return;
        }

        error = _ble.gap().startScan(_scan_time);
        if (error) {
            print_error(error, "Gap::startScan failed");
            return;
        }

        printf("Scanning for %" PRIu32 "ms\r\n", _scan_time.valueInMs());
    }

    void connect_peripheral()
    {
        update_state(bt_test_state_t::CONNECT_PERIPHERAL);
        printf("Connected as peripheral\r\n");
    }

    void connect_master()
    {
        update_state(bt_test_state_t::CONNECT_MASTER);
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

            printf("Periodic advertising for %" PRIu32 "ms\r\n", _advertise_time.valueInMs());
        } else {
            printf("Advertising started for %" PRIu32 "ms\r\n", _advertise_time.valueInMs());
        }
    }

    void onAdvertisingEnd(const ble::AdvertisingEndEvent &event) override
    {
        if (!_is_connecting_or_syncing) {
            printf("Advertise time elapsed\r\n");
            next_state();
        }
    }

    bool peer_is_match(const ble::AdvertisingReportEvent &event)
    {
        if (_target_mac_len == 0) {
            // Match by name.
            ble::AdvertisingDataParser adv_parser(event.getPayload());
            while (adv_parser.hasNext()) {
                ble::AdvertisingDataParser::element_t field = adv_parser.next();
                if (field.type == ble::adv_data_type_t::COMPLETE_LOCAL_NAME
                    && field.value.size() == strlen(DEVICE_NAME)
                    && memcmp(field.value.data(), DEVICE_NAME, field.value.size()) == 0) {
                    printf("Peer matched by name\r\n");
                    return true;
                }
            }
        } else {
            // Match by MAC.
            auto peer_address = event.getPeerAddress();
            auto raw = peer_address.data();
            assert(peer_address.size() == 6);
            auto peer_mac = new char[MAC_ADDRESS_LENGTH + 1];
            sprintf(peer_mac, "%02x:%02x:%02x:%02x:%02x:%02x", raw[5], raw[4], raw[3], raw[2], raw[1], raw[0]);
            peer_mac[MAC_ADDRESS_LENGTH] = 0;
            if (strcmp(_target_mac, peer_mac) == 0) {
                printf("Peer matched by MAC\r\n");
                return true;
            }
        }

        return false;
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

        // Match peer.
        if (peer_is_match(event)) {
            // Sync if periodic flag is ON, otherwise connect.
            if (_is_periodic) {
                printf(
                    "We found the peer, syncing with SID %d and periodic interval %" PRIu32 "ms\r\n",
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
                    print_error(error, "Gap::createSync failed");
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
                    print_error(error, "Gap::connect failed");
                    return;
                }
            }

            /* we may have already scan events waiting to be processed
             * so we need to remember that we are already connecting
             * or syncing and ignore them */
            _is_connecting_or_syncing = true;
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
                    _connect_time,
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
        _is_connecting_or_syncing = false;
        next_state();
    }

    /** Called when first advertising packet in periodic advertising is received. */
    void onPeriodicAdvertisingSyncEstablished(const ble::PeriodicAdvertisingSyncEstablishedEvent &event) override
    {
        ble_error_t error = event.getStatus();
        if (error) {
            print_error(error, "Sync with periodic advertising failed");
        } else {
            printf("Synced with periodic advertising\r\n");
            _sync_handle = event.getSyncHandle();
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
    ble::scan_duration_t _scan_time = ble::scan_duration_t(MBED_CONF_APP_SCAN_TIME);
    events::EventQueue::duration _connect_time = events::EventQueue::duration(MBED_CONF_APP_CONNECT_TIME);
    ble::adv_duration_t _advertise_time = ble::adv_duration_t(MBED_CONF_APP_ADVERTISE_TIME);

    BLE &_ble;
    events::EventQueue &_event_queue;

    uint8_t _adv_buffer[MAX_ADVERTISING_PAYLOAD_SIZE];
    ble::AdvertisingDataBuilder _adv_data_builder;

    ble::advertising_handle_t _adv_handle = ble::INVALID_ADVERTISING_HANDLE;
    ble::periodic_sync_handle_t _sync_handle = ble::INVALID_ADVERTISING_HANDLE;

    char _target_mac[MAC_ADDRESS_LENGTH + 1];
    size_t _target_mac_len = 0;
    bt_test_state_t _state;
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
