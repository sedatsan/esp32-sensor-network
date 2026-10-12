# Roadmap

1. ✅ Skeleton: core library, HAL, CI with tests, static analysis and firmware builds
2. ✅ HC-SR04 and HC-SR501 drivers with host tests
3. ✅ Node wake/sample/report/sleep cycle with retained state
4. ✅ ESP-NOW link, versioned binary report with CRC, gateway loss tracking
5. ☐ Measure node current in each state, document battery-life estimate
6. ☐ Gateway: Wi-Fi provisioning, MQTT over TLS, dashboard (Grafana or Node-RED)
7. ☐ Security: ESP-NOW encryption (PMK/LMK), signed OTA with rollback, secure boot
8. ☐ Robustness: task watchdog coverage, brown-out and reset-reason logging to NVS
9. ☐ Hardware-in-the-loop smoke test (Wokwi simulator in CI)
10. ☐ Add an I2C environment sensor (e.g. BME280) to show an I2C driver
