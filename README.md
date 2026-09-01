# Project Title

XpressWiFi

## Description

Variant of the XPressProx that utilizes an ESP32 chip with WiFi to sync
with access control databases without physical connection to an external device.

## Installation

Simply clone this repo and open the project with PlatformIO in FL Studio. The
project should work in Arduino IDE as well

## Dependencies

-[GxEPD2](https://github.com/ZinggJM/GxEPD2)

-[SQLite](https://github.com/sqlite/sqlite)

-[WiFiManager](https://github.com/tzapu/WiFiManager)

-[ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)

## Usage

The current state of the code has four Primary Functions:
1. Connecting to WiFi if the ESP32 can find a previously saved SSID and Password.
   In the case that this cannot be found, the ESP32 sets itself up as a WiFi
   access point and spins up a captive webportal. From there, the WiFi SSID and
   Password can be configured from an external device
2. Once a WiFi connection has been established, the ESP32 then initializes its
   SQL database. For debugging purposes, the database is currently deleted and
   and recreated on every boot. However, in a typical application the database
   is stored in the SPIFFS filesystem, meaning its contents will be saved even
   on reboot or after a fresh flash. The SPIFFS filesystem should have more
   than enough space to handle a typical database required for the XPressWiFi's
   function.
3. Finally, the ESP32 communicates with an Elatec TWN4 Multitech 3 M module over
   UART. The code for the [TWN4 Multitech 3 M](https://github.com/gavfxn/TWN4-ESP32-Communications) is in a seperate repo.

## Next Steps

This project is still in its prototyping stage. Currently, the prototype housing
has made quite decent progress. The CAD model for the current prototype design,
as well as a model which can act as a base for a new design.

PCB design is currently underway, and nearing completion.

The code still needs a good amount of further development. Primarily:
1. Getting the ESP32 to connect with an access control database.
2. Making the HTML for the captive webpage more presentable.
3. Cleaning up communication protocol between the ESP32 and TWN4.



## PIN WIRINGS

ePaper Module:
VCC -> 3V3
gnd -> GND
DIN -> 11
CLK -> 12
CS -> 10
DC -> 9
RST -> 8
BUSY -> 7

TWN4:
TX -> 4
RX -> 5
Power -> 5V
GND -> GND
