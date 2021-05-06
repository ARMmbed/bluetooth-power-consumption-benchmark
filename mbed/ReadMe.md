# Bluetooth Power Consumption Test - mbed OS

## Configuration

There are compile-time configurable parameters specific to this test which can be placed in `mbed_app.json`:

## Compilation

### Mbed CLI

```shell
ble-power-consumption/mbed $ mbed compile -m <board> -t ARMC6 --source . --source ../shared
```

## Known good board combinations

 * nRF52 scan, Nucleo WB55 advertise
 * WB55 scan, nRF52 advertise
 * WB55 scan, nRF52 advertise (periodic)

## Known Issues

 * Error when periodic advertising is initiated for the second time
   * `Gap::startPeriodicAdvertising() failed: BLE_ERROR_INVALID_STATE: Invalid state`
