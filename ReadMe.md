# BLE Power Consumption Test

This program allows an operator to select a BLE state (scan or advertise in constant or periodic mode) to measure power consumption in that state. Running the program on two boards and selecting scan on one and advertise/periodic advertise on the other will result in a connection between the boards so that power can also be measured in the connect as master/connect as peripheral states.

## Configuration

There are compile-time configurable parameters specific to this test which can be placed in `mbed_app.json`:

## Invocation



## Known good board combinations

 * nRF52 scan, Nucleo WB55 advertise
 * WB55 scan, nRF52 advertise
 * WB55 scan, nRF52 advertise (periodic)
