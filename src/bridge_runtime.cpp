#include "bridge_runtime.h"

#include <esp32-hal-rgb-led.h>

#include "ble_bridge.h"
#include "config.h"

namespace {

bool gLedIsOn = false;
uint32_t gLedLastToggleAt = 0;

bool ledEnabled() { return cfg::kStatusLedPin >= 0; }

void writeLed(bool on) {
  if (!ledEnabled()) {
    return;
  }

  if (cfg::kStatusLedIsNeoPixel) {
    const uint8_t value = on ? 16 : 0;
    neopixelWrite(cfg::kStatusLedPin, value, value, value);
    gLedIsOn = on;
    return;
  }

  int level = on ? HIGH : LOW;
  if (cfg::kStatusLedActiveLow) {
    level = on ? LOW : HIGH;
  }
  digitalWrite(cfg::kStatusLedPin, level);
  gLedIsOn = on;
}

void setupStatusLed() {
  if (!ledEnabled()) {
    return;
  }

  pinMode(cfg::kStatusLedPin, OUTPUT);
  writeLed(false);
  gLedLastToggleAt = millis();
}

void updateStatusLed(bool connected) {
  if (!ledEnabled()) {
    return;
  }

  if (connected) {
    if (!gLedIsOn) {
      writeLed(true);
    }
    return;
  }

  const uint32_t now = millis();
  if (now - gLedLastToggleAt >= cfg::kStatusLedBlinkIntervalMs) {
    gLedLastToggleAt = now;
    writeLed(!gLedIsOn);
  }
}

}  // namespace

namespace bridge_runtime {

void setupBridge(WiFiServer &tcpServer, HardwareSerial &uart, const BridgeConfig &config) {
  // Enable power to MAX3232 before initializing UART
  // Hardware circuit: GPIO -> 5kΩ resistor -> 2N3904 NPN base
  //   3.3V -> MAX3232 VCC
  //   MAX3232 GND -> NPN collector
  //   NPN emitter -> GND
  // When GPIO=HIGH, NPN conducts, completing ground path and powering MAX3232
  pinMode(cfg::kMax3232PowerPin, OUTPUT);
  digitalWrite(cfg::kMax3232PowerPin, HIGH);  // HIGH = NPN ON (for 2N3904)
  
  Serial.printf("MAX3232 power control enabled (GPIO %d = HIGH for NPN ON).\n", cfg::kMax3232PowerPin);
  Serial.println("** Circuit: GPIO --[5kΩ]--> 2N3904 base, MAX3232-GND -> collector, emitter -> GND **");
  delay(50);  // Give MAX3232 time to power up
  
  uart.begin(config.baudRate, SERIAL_8N1, cfg::kBridgeRxPin, cfg::kBridgeTxPin);
  setupStatusLed();
  tcpServer.begin();
  tcpServer.setNoDelay(true);
  Serial.printf("TCP server started on port %u.\n", cfg::kTcpPort); 
  Serial.printf("UART bridge on RX=%d TX=%d at %lu baud.\n", cfg::kBridgeRxPin, cfg::kBridgeTxPin,
                static_cast<unsigned long>(config.baudRate));
}

void serviceTcpBridge(WiFiServer &tcpServer, WiFiClient &tcpClient, HardwareSerial &uart) {
  bool connected = tcpClient && tcpClient.connected();
  if (!connected) {
    WiFiClient incoming = tcpServer.available();
    if (incoming) {
      if (tcpClient) {
        tcpClient.stop();
      }
      tcpClient = incoming;
      tcpClient.setNoDelay(true);
      Serial.printf("TCP client connected: %s\n", tcpClient.remoteIP().toString().c_str());
      connected = true;
    }
  }

  // Only update LED if BLE is not connected (BLE takes priority)
  if (!ble_bridge::isBleConnected()) {
    updateStatusLed(connected);
  }

  if (!connected) {
    return;
  }

  uint8_t buf[256];

  while (tcpClient.available()) {
    const size_t n = tcpClient.read(buf, sizeof(buf));
    if (n > 0) {
      uart.write(buf, n);
    }
  }

  while (uart.available()) {
    const size_t n = uart.readBytes(buf, sizeof(buf));
    if (n > 0) {
      tcpClient.write(buf, n);
    }
  }

  if (!tcpClient.connected()) {
    tcpClient.stop();
    Serial.println("TCP client disconnected.");
  }
}

void updateBridgeStatusLed(bool connected) { updateStatusLed(connected); }

}  // namespace bridge_runtime
