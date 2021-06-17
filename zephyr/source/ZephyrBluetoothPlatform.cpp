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
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include <bluetooth/bluetooth.h>
#include <console/console.h>
#include <zephyr.h>

#include <BluetoothPlatform.h>
#include <config.h>
#include <strerror.h>
#include <ZephyrBluetoothPlatform.h>

#define CALL(FN, ...)                \
    do {                             \
        auto error = FN(__VA_ARGS__);\
        if (error) {                 \
            printError(error, #FN);  \
            return error;            \
        }                            \
    } while (0)

#define CALLFN(FN)                 \
    do {                           \
        auto error = FN();         \
        if (error) {               \
            printError(error, #FN);\
            return error;          \
        }                          \
    } while (0)

#define CALL_NORET(FN, ...)          \
    do {                             \
        auto error = FN(__VA_ARGS__);\
        if (error) {                 \
            printError(error, #FN);  \
        }                            \
    } while (0)

#define CALLFN_NORET(FN)           \
    do {                           \
        auto error = FN();         \
        if (error) {               \
            printError(error, #FN);\
        }                          \
    } while (0)

ZephyrBluetoothPlatform ZephyrBluetoothPlatform::_instance;

ZephyrBluetoothPlatform &ZephyrBluetoothPlatform::instance()
{
    return _instance;
}

int ZephyrBluetoothPlatform::init()
{
    // Initialise subsystems.
    CALLFN(console_init);
    CALL(bt_enable, nullptr);
    k_mutex_init(&_scan_sync_mutex);

    // Register callbacks.
    bt_conn_cb_register(&conn_callbacks);
    bt_le_scan_cb_register(&scan_callbacks);
#if CONFIG_USE_PER_ADV_SYNC
	bt_le_per_adv_sync_cb_register(&sync_callbacks);
#endif

    // Trigger event.
    getEventHandler()->onInitComplete();
    return 0;
}

void ZephyrBluetoothPlatform::getLocalAddress(uint8_t buf[6])
{
    size_t count = CONFIG_BT_ID_MAX;
    bt_addr_le_t *addrs = new bt_addr_le_t[count];
    bt_id_get(addrs, &count);
    if (count == 0) {
        return;
    }

    memcpy(buf, addrs->a.val, 6);
    delete[] addrs;
}

void ZephyrBluetoothPlatform::runEventLoop()
{
    _event_queue.dispatch_forever();
}

void ZephyrBluetoothPlatform::call(BluetoothPlatform::callback_t fn, void *arg)
{
    _event_queue.call(fn, arg);
}

void ZephyrBluetoothPlatform::callIn(uint32_t millis, BluetoothPlatform::callback_t fn, void *arg)
{
    _event_queue.call_in(millis, fn, arg);
}

void ZephyrBluetoothPlatform::printError(intmax_t error, const char *msg)
{
    printk(
        "%s: error %" PRId64, // Zephyr's C lib does not provide PRIdMAX.
        msg,
        static_cast<int64_t>(error)
    );

    if (error < 0) {
        printk(" (%s)\n", strerror(static_cast<int>(-error)));
    } else {
        printk("\n");
    }
}

void ZephyrBluetoothPlatform::printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}

int ZephyrBluetoothPlatform::getchar()
{
    return console_getchar();
}

void ZephyrBluetoothPlatform::putchar(int c)
{
    console_putchar(static_cast<char>(c));
}

const char *ZephyrBluetoothPlatform::deviceName() const
{
    return "Power Consumption (Zephyr)";
}

bool ZephyrBluetoothPlatform::isPeriodicAdvertisingAvailable()
{
    return CONFIG_USE_PER_ADV_SYNC; // Zephyr provides no way to feature test this at runtime.
}

static const uint8_t adv_data_data[] = {0, 0, 0};
static const bt_data adv_data[] = {
    BT_DATA(BT_DATA_MANUFACTURER_DATA, adv_data_data, ARRAY_SIZE(adv_data_data))
};

int ZephyrBluetoothPlatform::startAdvertising()
{
    assert(!_is_connecting_or_syncing);
    assert(!_is_scanning_or_advertising);
    _is_scanner = false;
    _is_periodic = false;

    static const bt_le_adv_param adv_params[] = {
        BT_LE_ADV_PARAM_INIT(
            BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME,
            BT_GAP_ADV_FAST_INT_MIN_2,
            BT_GAP_ADV_FAST_INT_MAX_2,
            nullptr
        )
    };
    CALL(bt_le_adv_start, adv_params, adv_data, ARRAY_SIZE(adv_data), nullptr, 0);

    _is_scanning_or_advertising = true;
    _is_connecting_or_syncing = false;
    _event_queue.call_in(CONFIG_ADVERTISE_TIME, &ZephyrBluetoothPlatform::endAdvertisingCallback, this);

    getEventHandler()->onAdvertisingStart(
        AdvertisingStartEvent(
            CONFIG_ADVERTISE_TIME,
            false,
            0
        )
    );
    return 0;
}

int ZephyrBluetoothPlatform::startScan()
{
    assert(!_is_connecting_or_syncing);
    assert(!_is_scanning_or_advertising);
    _is_scanner = true;
    _is_periodic = false;

    static const bt_le_scan_param scan_params = {
        .type     = BT_LE_SCAN_TYPE_ACTIVE,
        .options  = BT_LE_SCAN_OPT_NONE,
        .interval = 0x10, // ] Default values
        .window   = 0x10, // ]
    };

    CALL(bt_le_scan_start, &scan_params, nullptr);

    _is_scanning_or_advertising = true;
    _is_connecting_or_syncing = false;
    _event_queue.call_in(CONFIG_SCAN_TIME, &ZephyrBluetoothPlatform::endScanCallback, this);

    getEventHandler()->onScanStart(ScanStartEvent(CONFIG_SCAN_TIME));

    return 0;
}

int ZephyrBluetoothPlatform::establishConnection(uint8_t peerAddressType, const uint8_t *peerAddress)
{
    assert(_is_scanner);
    assert(_conn == nullptr);

    _is_connecting_or_syncing = true;
    endScan();

    // Create the connection. The connectedCallback will be called when the connection is actually established.
    static const struct bt_conn_le_create_param create_params[] {
        BT_CONN_LE_CREATE_PARAM_INIT(BT_CONN_LE_OPT_NONE, BT_GAP_SCAN_FAST_INTERVAL, BT_GAP_SCAN_FAST_WINDOW)
    };
    static const struct bt_le_conn_param conn_params[] {
        BT_LE_CONN_PARAM_INIT(BT_GAP_INIT_CONN_INT_MIN, BT_GAP_INIT_CONN_INT_MAX, 0, 400)
    };
    bt_addr_le_t addr {.type = peerAddressType};
    memcpy(addr.a.val, peerAddress, sizeof(addr.a.val));

    auto error = bt_conn_le_create(&addr, create_params, conn_params, &_conn);
    if (error) {
        printError(error, "bt_conn_le_create");
        // NB: If bt_conn_le_create is successful we will call 'EventHandler::onConnection()' in 'connected()'. This is
        // to keep the program running if we don't get that far, as 'connectedCallback()' won't be called.
        getEventHandler()->onConnection(ConnectEvent(error));
        return error;
    }

    return 0;
}

int ZephyrBluetoothPlatform::disconnect(handle_t connection_handle)
{
    assert(_conn != nullptr);
    assert(connection_handle == _conn);
    assert(_is_connecting_or_syncing);

    _is_connecting_or_syncing = false;

    CALL(bt_conn_disconnect, _conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);

    bt_conn_unref(_conn);
    _conn = nullptr;
    return 0;
}

#if CONFIG_USE_PER_ADV_SYNC
int ZephyrBluetoothPlatform::startPeriodicAdvertising_Error(int error, const char* func)
{
    printError(error, func);
    cleanUpExtendedAdvertising();
    return error;
}

int ZephyrBluetoothPlatform::startPeriodicAdvertising()
{
    assert(!_is_connecting_or_syncing);
    assert(!_is_scanning_or_advertising);
    _is_scanner = false;
    _is_periodic = true;

    static const bt_le_adv_param adv_params[] = {
        BT_LE_ADV_PARAM_INIT(
            BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_USE_NAME,
            BT_GAP_ADV_FAST_INT_MIN_2,
            BT_GAP_ADV_FAST_INT_MAX_2,
            nullptr
        )
    };
	static const bt_le_per_adv_param per_adv_params[] {
		BT_LE_PER_ADV_PARAM_INIT(
            BT_GAP_ADV_SLOW_INT_MIN,
            BT_GAP_ADV_SLOW_INT_MAX,
            BT_LE_PER_ADV_OPT_NONE
        )
	};
    static bt_le_ext_adv_start_param adv_start_params[] = {
        BT_LE_EXT_ADV_START_PARAM_INIT(0, 0)
    };

    int error = bt_le_ext_adv_create(adv_params, nullptr, &_adv_set);
    if (error) {
        printError(error, "bt_le_ext_adv_create");
        return error;
    }

    error = bt_le_per_adv_set_param(_adv_set, per_adv_params);
    if (error) {
        return startPeriodicAdvertising_Error(error, "bt_le_per_adv_set_param");
    }

    error = bt_le_per_adv_start(_adv_set);
    if (error) {
        return startPeriodicAdvertising_Error(error, "bt_le_per_adv_start");
    }

    error = bt_le_ext_adv_start(_adv_set, adv_start_params);
    if (error) {
        return startPeriodicAdvertising_Error(error, "bt_le_ext_adv_start");
    }

    _is_scanning_or_advertising = true;
    _is_connecting_or_syncing = false;
    _event_queue.call_in(CONFIG_ADVERTISE_TIME, &ZephyrBluetoothPlatform::endAdvertisingCallback, this);

    getEventHandler()->onAdvertisingStart(
        AdvertisingStartEvent(
            CONFIG_ADVERTISE_TIME,
            true,
            CONFIG_APP_PERIODIC_INTERVAL
        )
    );
    return 0;
}

int ZephyrBluetoothPlatform::startScanForPeriodicAdvertising()
{
    auto ret = startScan();
    _is_periodic = true;
    return ret;
}

int ZephyrBluetoothPlatform::syncToPeriodicAdvertising(
    int32_t sid,
    uint8_t peerAddressType,
    const uint8_t *peerAddress,
    uint32_t syncTimeoutMs
)
{
    // Hold sync lock to prevent scanCallback from running again until we have tried to sync.
    int error;
    CALL(k_mutex_lock, &_scan_sync_mutex, K_FOREVER);
    {
        static bt_le_per_adv_sync_param sync_params;
        memset(&sync_params, 0, sizeof(sync_params));
        sync_params.sid = sid;
        sync_params.timeout = MIN(MAX(0xA, syncTimeoutMs/10), 0x4000);
        sync_params.addr.type = peerAddressType;
        memcpy(sync_params.addr.a.val, peerAddress, sizeof(sync_params.addr.a.val));
        error = bt_le_per_adv_sync_create(&sync_params, &_sync);
        if (error) {
            printError(error, "bt_le_per_adv_sync_create");
            _is_connecting_or_syncing = false;
        } else {
            _is_connecting_or_syncing = true;
        }
    }

    k_mutex_unlock(&_scan_sync_mutex);
    return error;
}

int ZephyrBluetoothPlatform::stopSync(handle_t sync_handle)
{
    assert(_is_connecting_or_syncing);
    CALL(bt_le_per_adv_sync_delete, _sync);
    _sync = nullptr;
    _is_connecting_or_syncing = false;
    return 0;
}

void ZephyrBluetoothPlatform::cleanUpExtendedAdvertising()
{
    // Stop periodic advertising, stop extended advertising and delete the advertising set.
    // The API docs imply it may be possible to do only the 3rd step but it's unclear.
    // See https://docs.zephyrproject.org/latest/reference/bluetooth/gap.html
    CALL_NORET(bt_le_per_adv_stop, _adv_set);
    CALL_NORET(bt_le_ext_adv_stop, _adv_set);
    CALL_NORET(bt_le_ext_adv_delete, _adv_set);
}

#endif // CONFIG_USE_PER_ADV_SYNC

void ZephyrBluetoothPlatform::endAdvertising()
{
    if (!_is_scanning_or_advertising || _is_scanner) {
        return;
    }

    // Update flags and stop advertising.
    _is_scanning_or_advertising = false;
#if CONFIG_USE_PER_ADV_SYNC
    if (_is_periodic) {
        cleanUpExtendedAdvertising();
    } else {
        CALLFN_NORET(bt_le_adv_stop);
    }
#else
    assert(!_is_periodic);
    CALLFN_NORET(bt_le_adv_stop);
#endif

    // Trigger timeout, unless we are already connecting.
    if (!_is_connecting_or_syncing) {
        getEventHandler()->onAdvertisingTimeout();
    }
}

void ZephyrBluetoothPlatform::endScan()
{
    if (!_is_scanning_or_advertising || !_is_scanner) {
        return;
    }

    // Update flags & stop the scan.
    _is_scanning_or_advertising = false;
    CALLFN_NORET(bt_le_scan_stop);

    // Trigger timeout unless we are already connecting.
    if (!_is_connecting_or_syncing) {
        getEventHandler()->onScanTimeout();
    }
}

void ZephyrBluetoothPlatform::endAdvertisingCallback(void *ignored)
{
    ((void)ignored);
    _instance.endAdvertising();
}

void ZephyrBluetoothPlatform::endScanCallback(void *ignored)
{
    ((void)ignored);
    _instance.endScan();
}

#define DEV_NAME_MAX 50

static bool nameCallback(bt_data *data, void *user_data)
{
	if (data->type == BT_DATA_NAME_SHORTENED || data->type == BT_DATA_NAME_COMPLETE) {
	    auto name = reinterpret_cast<char *>(user_data);
        auto len = MIN(data->data_len, DEV_NAME_MAX - 1);
        memcpy(name, data->data, len);
        name[len] = '\0';
        return false;
    }

    return true;
}

void ZephyrBluetoothPlatform::scanCallback(const bt_le_scan_recv_info *info, net_buf_simple *buf)
{
    {
        // Don't call the event handler if we are already connecting or syncing.
        auto error = k_mutex_lock(&_instance._scan_sync_mutex, K_FOREVER);
        if (error) {
            _instance.printError(error, "k_mutex_lock");
            return;
        }

        auto ignore = _instance._is_connecting_or_syncing;
        k_mutex_unlock(&_instance._scan_sync_mutex);
        if (ignore) {
            return;
        }
    }

    char local_name[DEV_NAME_MAX];
    memset(local_name, 0, sizeof(local_name));
    bt_data_parse(buf, &nameCallback, local_name);
    _instance.getEventHandler()->onAdvertisingReport(
        AdvertisingReportEvent(
            info->sid,
            info->addr->type,
            info->addr->a.val,
            sizeof(info->addr->a.val),
            local_name,
            info->interval > 0,
            info->interval
        )
    );
}

void ZephyrBluetoothPlatform::connectedCallback(bt_conn *conn, uint8_t err)
{
    // Update flags and stop scan/adv.
    _instance._is_connecting_or_syncing = true;
    _instance._conn = conn;
    if (_instance._is_scanner) {
        _instance.endScan();
    } else {
        _instance.endAdvertising();
    }

    // Get peer address & connection info.
    auto addr = bt_conn_get_dst(conn);
    bt_conn_info info;
    {
        memset(&info, 0, sizeof(info));
        auto error = bt_conn_get_info(conn, &info);
        if (error) {
            _instance.printError(error, "bt_conn_get_info");
            err = error;
        }
    }

    // Raise event.
    _instance.getEventHandler()->onConnection(
        ConnectEvent(
            info.type,
            &(addr->a.val[0]),
            sizeof(addr->a.val),
            static_cast<intmax_t>(err),
            info.role == BT_CONN_ROLE_MASTER
                       ? BluetoothPlatform::connection_role_t::main
                       : BluetoothPlatform::connection_role_t::peripheral,
            _instance._conn
        )
    );
}

void ZephyrBluetoothPlatform::disconnectedCallback(bt_conn *conn, uint8_t reason)
{
    assert(_instance._conn == conn);
    _instance._conn = nullptr;
    _instance._is_connecting_or_syncing = false;
    _instance.getEventHandler()->onDisconnect();
}

void ZephyrBluetoothPlatform::syncedCallback(bt_le_per_adv_sync *sync, bt_le_per_adv_sync_synced_info *sync_info)
{
    assert(_sync == nullptr);
    _instance._is_connecting_or_syncing = true;
    if (_instance._is_scanner) {
        _instance.endScan();
    } else {
        _instance.endAdvertising();
    }

    int error = 0;

    _instance._sync = sync;
    auto addr = bt_conn_get_dst(sync_info->conn);
    _instance.getEventHandler()->onPeriodicSync(
        PeriodicSyncEvent(
            sync_info->sid,
            addr->type,
            &(addr->a.val[0]),
            sizeof(addr->a.val),
            static_cast<intmax_t>(error),
            _instance._is_scanner ? BluetoothPlatform::connection_role_t::main
                                  : BluetoothPlatform::connection_role_t::peripheral,
            _instance._sync
        )
    );
}

void ZephyrBluetoothPlatform::syncLostCallback(bt_le_per_adv_sync *sync, const bt_le_per_adv_sync_term_info *info)
{
    _instance._is_connecting_or_syncing = false;
    _instance.getEventHandler()->onSyncLoss();
}
