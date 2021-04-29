# BLE Power Consumption Test

This program allows an operator to select a BLE state (scan or advertise in constant or periodic mode) to measure power consumption in that state. Two boards in complementary modes will establish a connection so power can also be measured when connected as peripheral or master.

Scan and advertise will wait up to 10 seconds to connect. Once connected, the master disconnects after 10 seconds. 

## Known good combinations

 * nRF52 scan, Nucleo WB55 advertise
 * WB55 scan, nRF52 advertise
