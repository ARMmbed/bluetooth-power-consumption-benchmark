# Bluetooth Power Consumption Test - Zephyr

## Configuration

Some test values can be configured via [CMakeLists.txt](./CMakeLists.txt):

 * `CONFIG_APP_SCAN_TIME`: How long to wait for connection when scanning (ms)
 * `CONFIG_APP_ADVERTISE_TIME`: How long to wait for connection when advertising (ms)
 * `CONFIG_APP_CONNECT_TIME`: How long to stay connected when master (ms)
 * `CONFIG_APP_PERIODIC_INTERVAL`: Average interval for periodic advertising (ms)
 * `CONFIG_APP_TRACE_UNMATCHED_PEERS`: List unmatched peers when scanning (0: disable, 1: enable)

## Compilation

### West

Build from the Zephyr source directory created by `west init`.

```shell
~/zephyr/zephyr $ west build -b <board> -p auto ~/bluetooth-power-consumption-benchmark/zephyr
```

## Known Issues

 * -5 (EIO) when calling bt_le_adv_stop when advertising times out
 * Peripheral sometimes does not receive or does not act on disconnect signal from main