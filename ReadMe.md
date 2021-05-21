# BLE Power Consumption Test

This program allows an operator to select a BLE state (scan or advertise in constant or periodic mode) to measure power consumption in that state. Running the program on two boards and selecting scan on one and advertise/periodic advertise on the other will result in a connection between the boards so that power can also be measured in the connect as master/connect as peripheral states.

## Building

Build using Mbed Studio, mbed CLI or cmake.

## Configuration

There are compile-time configurable parameters specific to this test program defined in `mbed_app.json`:

## Invocation

Input and output is via serial. The program can be commanded to enter either the advertise (`a` command) or scan (`s` command) state, which last for 60 seconds by default. If two boards are set to complementary states, a connection will be formed and maintained for a default length of 60 seconds. Instead of connecting, the boards can be synced via periodic advertising by toggling the periodic flag with the `p` command before using the `s` and `a` commands. By default, the scanning board will look for another device with the name `Power Consumption`; using the `m` command and inputting a hexadecimal MAC address (`0a1b2c3d4e5f` or `0a:1b:2c:3d:4e:5f` format) will cause `s` to scan for the device with the given MAC instead. This can be reverted by using the `m` command again and pressing `ENTER`.

## Known good board combinations

 * nRF52 scan, Nucleo WB55 advertise
 * WB55 scan, nRF52 advertise
 * WB55 scan, nRF52 advertise (periodic)
