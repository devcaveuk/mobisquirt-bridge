# MobisquirtBridge

A simple bridge that allows Mobisquirt (or TunerStudio) to connect to the ECU via TCP or Bluetooth LE.

Using an ESP32-C3 "supermini" or an ESP32-S3 Zero offers full WiFi and BLE connectivity in a small cheap package. This can either be connected directly to the ECU PCB (built into the ECU if desired) or can be connected to a UART to RS232 convertor with a DB9 connector, allowing connection through an existing external serial port on the ECU.

Both modules are 3.3v and can be powered from 5v, however, when connected to a 5V UART the RX pin will need level shifting from 5v to 3.3v - normally this is done using a simple resistor. Failing to protect the module from 5v on it's RX pin may damage the module.

PlatformIO firmware for ESP32-C3 and ESP32-S3 boards that provides:

- A TCP socket bridge to a wired UART (`Serial1`)
- A built-in web page for configuration
- Wi-Fi operation in either AP or STA mode
- Configurable UART baud rate

## Features

- TCP socket server on port `9001` for WiFi connectivity
- Bluetooth LE (BLE) bridge as an alternative to WiFi
- PIN-protected BLE pairing for security
- One client at a time (TCP or BLE) for transparent serial bridging
- Web configuration page on port `80`
- Persistent settings stored in NVS (`Preferences`)
- Automatic fallback to AP mode if STA connection fails
- Onboard status LED: blinking while waiting for a client, solid when connected
- Configurable BLE Service and Characteristic UUIDs

## Default Settings

- Device name: `MobisquirtBridge`
- Wi-Fi mode: AP
- AP SSID: `MobisquirtBridge`
- AP password: `mobisquirt123`
- UART baud rate: `115200`
- TCP port: `9001`
- BLE: Off by default
- BLE Service UUID: `8f771e35-9b8f-42e7-a91c-5dcb9184d354`
- BLE Characteristic UUID: `d5a18ee7-877d-4b4a-8cce-c8b11c724b2d`
- BLE PIN: `123456`

## Hardware Notes

Default UART bridge pins in firmware (mapped by environment and board):

- `esp32-c3-supermini` -> `esp32-c3-devkitm-1` (ESP32-C3): RX GPIO20, TX GPIO21
- `esp32-s3-zero` -> `adafruit_qtpy_esp32s3_n4r2` (ESP32-S3): RX GPIO8, TX GPIO7

### MAX3232 Power Control

The firmware includes power control for the MAX3232 RS-232 transceiver, ensuring it only powers on after the ESP32 is fully initialized. This prevents floating signals during boot.

**Power Control GPIO:**

- ESP32-S3: GPIO 9
- ESP32-C3: GPIO 10

**Required Circuit:**
The GPIO cannot directly power the MAX3232 (it draws ~10mA, which would overload the pin). You need a simple NPN transistor switching circuit:

```
3.3V rail ────────────────→ MAX3232 VCC

MAX3232 GND ──→ Collector─┤ 2N3904 NPN (or similar)
                        Base ──[1kΩ-10kΩ]── ESP32 GPIO 9/10
                     Emitter ──→ GND
```

**Parts needed:**

- 1x NPN transistor (e.g., 2N3904, 2N2222, BC547)
- 1x resistor (1kΩ - 10kΩ work well; 5kΩ is ideal)

When the ESP32 boots, it sets the GPIO HIGH, turning on the transistor and completing the ground path for the MAX3232.

If your board wiring differs, adjust `kBridgeRxPin`, `kBridgeTxPin`, and `kMax3232PowerPin` in `src/config.h`.
The status LED uses `LED_BUILTIN` by default. You can override this with `STATUS_LED_PIN` and set active-low behavior with `STATUS_LED_ACTIVE_LOW=1` in `platformio.ini`.

Supported PlatformIO environments:

- `esp32-c3-supermini` (`esp32-c3-devkitm-1`)
- `esp32-s3-zero` (`adafruit_qtpy_esp32s3_n4r2`, 4MB flash / 2MB PSRAM)

## Source Layout

- `src/main.cpp`: Startup orchestration, web route wiring, and reboot scheduling
- `src/config.h`: Centralized constants, defaults, limits, preference keys, and board pin defaults
- `src/bridge_types.h`: Shared data types (`WifiMode`, `BridgeConfig`)
- `src/config_store.h` / `src/config_store.cpp`: NVS load/save and config sanitization
- `src/wifi_manager.h` / `src/wifi_manager.cpp`: STA/AP startup and STA fallback to AP behavior
- `src/web_ui.h` / `src/web_ui.cpp`: Web page rendering and request parsing/validation
- `src/bridge_runtime.h` / `src/bridge_runtime.cpp`: TCP socket to UART bridge runtime
- `src/ble_bridge.h` / `src/ble_bridge.cpp`: BLE to UART bridge runtime

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

### WiFi TCP Mode

1. Power the board and connect to Wi-Fi:
   - AP mode default SSID: `MobisquirtBridge`
   - Password: `mobisquirt123`
2. Open `http://192.168.4.1/` in a browser.
3. Update:
   - UART baud rate
   - Wi-Fi mode (AP or STA)
   - SSID/password fields
   - BLE settings (enable/disable, UUIDs, PIN)
4. Save settings from the page. The device reboots and applies changes.
5. Connect a TCP client to port `9001` on the device IP to bridge serial traffic.

### Bluetooth LE Mode

1. Enable BLE in the web configuration page
2. Configure the BLE PIN (default: `123456`, must be 6 digits)
3. Save settings and reboot the device
4. The device will advertise as `MobisquirtBridge` (or your configured device name)
5. Connect to the BLE device from your client application
6. When prompted for a PIN during pairing, enter the configured 6-digit PIN
7. Use the configured Service and Characteristic UUIDs to send/receive UART data
8. Data written to the characteristic is forwarded to UART
9. Data from UART is notified through the characteristic

**Note:** BLE and WiFi TCP can both be enabled simultaneously. WiFi is used for the web configuration interface, while BLE provides the data bridge 4. Save settings from the page. The device reboots and applies changes. 5. Connect a TCP client to port `9001` on the device IP to bridge serial traffic.

## Security Notes

- BLE communication uses PIN-based pairing with bonding and encryption.
- Change the default PIN via the web interface for better security.

## Planned features

## Planned features

- Add Bluetooth BLE support for streaming to/from the UART
- Step by step documentation on hardware setup/connection

```

```
