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

 * nrf52 doesn't receive disconnect signal until main board is reset when connected as peripheral, though it does trigger disconnect correctly when it is the main
