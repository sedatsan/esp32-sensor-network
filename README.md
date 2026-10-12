# ESP32 Sensor Network

Battery-friendly sensor nodes (ESP32-C3) that wake on motion or on a timer, measure distance, and report over ESP-NOW to an ESP32 gateway. Written in modern C++ (C++20) on ESP-IDF, with the hardware-independent core unit-tested on the host.

```
 ┌────────────────────────┐   ESP-NOW (16-byte report, CRC-16)   ┌──────────────────────────┐
 │ Node: ESP32-C3 SuperMini│ ───────────────────────────────────▶ │ Gateway: ESP32 DevKit V1  │
 │  HC-SR501 PIR (wake)    │                                      │  RX callback → queue →    │
 │  HC-SR04 ultrasonic     │   ◀── MAC-layer ACK, retry/backoff   │  report task (FreeRTOS)   │
 │  deep sleep between     │                                      │  per-node loss tracking   │
 └────────────────────────┘                                      └────────────┬─────────────┘
                                                                              │ (planned)
                                                                              ▼ MQTT over TLS
```

## What this project demonstrates

| Area | Where |
|---|---|
| Hardware abstraction: drivers depend on `IClock`/`IInputPin`/`IOutputPin`, not on ESP-IDF | `components/sensor_core/include/sn/hal` |
| Own drivers: HC-SR04 (timeouts, temperature compensation, median filter), HC-SR501 PIR (warm-up, edge events) | `components/sensor_core/src` |
| Binary protocol: versioned, little-endian, CRC-16/CCITT, every single-bit error detected (tested) | `sn/protocol/report.hpp` |
| Low power: deep sleep with timer + GPIO wake, state kept in RTC memory, cold boot handling | `firmware/node/main/main.cpp`, `NodeController` |
| Reliability: retry with exponential backoff, sequence numbers, gap/duplicate/restart detection on the gateway | `NodeController`, `GatewayTracker` |
| FreeRTOS: the radio callback (Wi-Fi task) only enqueues; decoding happens in its own task | `firmware/gateway/main/main.cpp` |
| Embedded-friendly C++20: no exceptions, no RTTI, no heap in the core, `std::span`, `constexpr` CRC, `Expected<T, E>` errors | throughout |
| Host unit tests with fakes, under AddressSanitizer + UBSan | `tests/` |
| CI: host tests, clang-tidy, cppcheck, clang-format, firmware builds for both chips | `.github/workflows/ci.yml` |

## Layout

```
components/
  sensor_core/    platform-independent: HAL interfaces, drivers, protocol, app logic
  esp_platform/   ESP-IDF implementations of the HAL + ESP-NOW link
firmware/
  node/           ESP32-C3 sensor node app
  gateway/        ESP32 gateway app
tests/            GoogleTest suite for sensor_core (runs on the PC)
docs/             wiring and roadmap
```

## Build

Host tests (any Linux/macOS with CMake ≥ 3.20 and a C++20 compiler):

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Firmware (ESP-IDF v5.3):

```sh
cd firmware/gateway && idf.py set-target esp32 build flash monitor
# note the "gateway MAC" line it prints, then:
cd firmware/node && idf.py set-target esp32c3 menuconfig   # Sensor node → Gateway MAC address
idf.py build flash monitor
```

Wiring is in [docs/wiring.md](docs/wiring.md). **The HC-SR04 ECHO pin outputs 5 V; use the voltage divider.**

## Roadmap

See [docs/roadmap.md](docs/roadmap.md).
