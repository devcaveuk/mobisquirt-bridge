# MobisquirtBridge

A simple bridge that allows Mobisquirt (or TunerStudio) to connect to the ECU via TCP.

Using an ESP32-C3 "supermini" or an ESP32-S3 Zero offers full WiFi connectivity in a small cheap package. This can either be connected directly to the ECU PCB (built into the ECU if desired) or can be connected to a UART to RS232 convertor with a DB9 connector, allowing connection through an existing external serial port on the ECU.

Both modules are 3.3v and can be powered from 5v, however, when connected to a 5V UART the RX pin will need level shifting from 5v to 3.3v - normally this is done using a simple resistor. Failing to protect the module from 5v on it's RX pin may damage the module.

There is a potential for this to also support a Bluetooth BLE connection to stream data to/from UART via Bluetooth BLE. This might be useful in some scenarios, particularly as it could mean auto connecting from an app rather than having to connect to the WiFi AP all the time.

PlatformIO firmware for ESP32-C3 and ESP32-S3 boards that provides:

- A TCP socket bridge to a wired UART (`Serial1`)
- A built-in web page for configuration
- Wi-Fi operation in either AP or STA mode
- Configurable UART baud rate

## Features

- TCP socket server on port `9001`
- One TCP client at a time (simplifies transparent serial bridging)
- Web configuration page on port `80`
- Persistent settings stored in NVS (`Preferences`)
- Automatic fallback to AP mode if STA connection fails

## Default Settings

- Device name: `MobisquirtBridge`
- Wi-Fi mode: AP
- AP SSID: `MobisquirtBridge`
- AP password: `mobisquirt123`
- UART baud rate: `115200`
- TCP port: `9001`

## Hardware Notes

Default UART bridge pins in firmware:

- `esp32-c3-supermini`: RX GPIO20, TX GPIO21
- `esp32-s3-zero`: RX GPIO17, TX GPIO18

If your board wiring differs, adjust `BRIDGE_RX_PIN` and `BRIDGE_TX_PIN` in `platformio.ini` for the relevant environment.

Supported PlatformIO environments:

- `esp32-c3-supermini` (`esp32-c3-devkitm-1`)
- `esp32-s3-zero` (`esp32-s3-zero`, 4MB flash)

## Build and Flash

1. Install PlatformIO Core or use the VS Code PlatformIO extension.
2. From this project directory, run:

```bash
pio run -e esp32-c3-supermini
pio run -e esp32-s3-zero

pio run -e esp32-c3-supermini -t upload
pio run -e esp32-s3-zero -t upload

pio device monitor -b 115200
```

## Usage

1. Power the board and connect to Wi-Fi:
   - AP mode default SSID: `MobisquirtBridge`
   - Password: `mobisquirt123`
2. Open `http://192.168.4.1/` in a browser.
3. Update:
   - UART baud rate
   - Wi-Fi mode (AP or STA)
   - SSID/password fields
4. Save settings from the page. The device reboots and applies changes.
5. Connect a TCP client to port `9001` on the device IP to bridge serial traffic.

## Security Notes

- This is a simple configuration UI intended for trusted local networks.
- For production, add authentication and transport protections.

## Planned features

- Add Bluetooth BLE support for streaming to/from the UART
- Step by step documentation on hardware setup/connection
- Refactor from the single main.cpp into function specific classes and associated header files
