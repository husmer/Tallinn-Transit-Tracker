# Tallinn Transit Tracker

A real-time* bus arrival display for Tallinn, Estonia using ESP32 and RGB LED matrix panels.


*It's not exactly real-time as the current bus location isn't taken into account. peatus.ee api only contains predetermined arrival times. If example a bus is late to the stop then the bus arrival display still shows the original arrival time. If the bus arrives earlier than the timetable then you just simply have to pray it doesn't leave before the official stop time.

## Description

This project displays the next 3 bus arrivals from two nearby bus stops on a 128x32 RGB LED matrix display. It queries the peatus.ee GraphQL API every 60 seconds and shows route numbers, destinations, and arrival times.

The idea was taken from this https://transit-tracker.eastsideurbanism.org/docs/introduction and modified for Tallinn's public transit.

## Hardware Requirements

- ESP32-S3 microcontroller (Waveshare Matrix Portal S3)
- 2x 64x32 RGB LED matrix panels (HUB75 interface, P2.5 pitch)
- 5V power supply (sufficient for 2 LED panels)

Build guide: https://transit-tracker.eastsideurbanism.org/docs/build-guide
All parts were ordered from AliExpress. European vendors were out of stock.
The terminal frame was 3D printed. The print stl file can be found from the build-guide.

## Software Requirements

- Arduino IDE
- ESP32 board support (ESP32 by Espressif Systems)
- Libraries:
  - ESP32-HUB75-MatrixPanel-I2S-DMA (v3.0.13)
  - ArduinoJson (v7.x)
  - WiFi (included with ESP32)
  - HTTPClient (included with ESP32)

Arduino IDE should import the libraries.
Make sure you select Waveshare ESP32-S3-Matrix and select COM6 port before uploading the code to the controller. Otherwise it doesn't recognize it. 

## How It Works

1. Connects to WiFi on startup
2. Syncs time with NTP server (daylight savings is applied but idk if it works)
3. Queries peatus.ee GraphQL API every 60 seconds
4. Parses departure data from two bus stops
5. Merges and sorts by arrival time
6. Displays next 3 departures on LED matrix
7. Retries in 5 seconds on API errors, 60 seconds on success
## Configuration

### WiFi Settings

Find the `// WiFi Configuration` section in `TransitProject.ino`:

```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```
Arduino IDE doesn't support .env files so you have to hardcode your wifi credentials.

### Bus Stop Configuration

Find the `// API Configuration` section and modify the stop IDs in the GraphQL query:

```cpp
const char* graphqlQuery = "{\"query\":\"{ stop1: stop(id: \\\"estonia:STOP_ID_1\\\") { name stoptimesWithoutPatterns(numberOfDepartures: 5) { realtimeArrival headsign trip { route { shortName } } } } stop2: stop(id: \\\"estonia:STOP_ID_2\\\") { name stoptimesWithoutPatterns(numberOfDepartures: 5) { realtimeArrival headsign trip { route { shortName } } } } }\"}";
```

Replace `STOP_ID_1` and `STOP_ID_2` with your desired stop IDs. You can find stop IDs at https://peatus.ee/ The ID is at the end of the URL when you click on your bus stop. Then add `estonia:` in front of the ID in the query.
Example: For stop 139010, use `estonia:139010`

### Color Customization

All color settings are in the `// Display on LED` section.

#### Route Number Colors

Find the route number color-coding block in `displayOnLED()`:

```cpp
if (routeNum == "54") {
  matrix->setTextColor(matrix->color565(0, 100, 255));  // Blue for route 54
} else if (routeNum == "61") {
  matrix->setTextColor(matrix->color565(0, 255, 0));    // Green for route 61
} else {
  matrix->setTextColor(matrix->color565(255, 255, 255)); // White for other routes
}
```

Change the route numbers ("54", "61") and their RGB values using `matrix->color565(R, G, B)` where R, G, B are 0-255.

#### Destination Text Color

Find the destination comment and modify:

```cpp
matrix->setTextColor(matrix->color565(255, 177, 0));  // Orange
```

#### Time Display Colors

Normal minutes:
```cpp
matrix->setTextColor(matrix->color565(255, 177, 0));  // Orange
```

Urgent "NOW" message:
```cpp
matrix->setTextColor(matrix->color565(187, 0, 0));  // Dark red
```

### LED Matrix Settings

#### Brightness

Find the brightness setting in the `// Setup` section:

```cpp
matrix->setBrightness8(60);  // Range: 0-255
```

#### Signal Quality

If you experience ghosting or flickering, find the i2sspeed setting in the `// Setup` section:

```cpp
mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;  // Options: HZ_10M, HZ_15M, HZ_20M
```

Lower speeds (HZ_10M) = better signal quality, less ghosting

### Filter Settings

To change the maximum time range for displayed buses, find this check in the `// Display on LED` section:

```cpp
if (allDepartures[i].minutesUntil > 99) {  // Skip buses more than 99 minutes away
```

## Pin Configuration

The default pin configuration matches the Waveshare ESP32-S3 setup. If using different hardware, modify the `// LED Matrix Configuration` section (pin definitions). I can't guarantee other hardware compability with this software or LED panels.

## Features

- Displays next 3 soonest bus arrivals
- Color-coded route numbers
- Automatic WiFi reconnection on signal loss (I had some issues when WiFi randomly disconnects)
- Filters out next-day departures (>99 minutes)

## Current Error Handling

- "API ERR:-1" = Connection failed (check WiFi/internet)
- "API ERR:-2" = WiFi disconnected
- "NO DATA" = API returned no departures
- Automatic WiFi reconnection on connection errors

## License

Open source - modify as needed for your use case :)
