# Bluetooth Power Consumption Test - mbed OS

## Configuration

Configuration is through variables in `mbed_app.json`:

 * `scan_time`: How long to wait for connection when scanning
 * `advertise_time`: How long to wait for connection when advertising
 * `connect_time`: How long to stay connected when master
 * `periodic_interval`: Average interval for periodic advertising

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
