# Wiring

## Node: ESP32-C3 Super Mini

| Part | Pin | C3 Super Mini | Notes |
|---|---|---|---|
| HC-SR501 PIR | VCC | 5V | Module needs 4.5 V or more; it has its own 3.3 V regulator |
| | OUT | GPIO3 | 3.3 V output. GPIO0 to 5 are the only deep-sleep wake pins on the C3 |
| | GND | GND | |
| HC-SR04 | VCC | 5V | |
| | TRIG | GPIO4 | 3.3 V is enough to trigger it |
| | ECHO | GPIO5 **through a divider** | ECHO is 5 V. Use 1 kΩ from ECHO to GPIO5 and 2 kΩ from GPIO5 to GND (≈3.3 V) |
| | GND | GND | |

Avoid GPIO2, GPIO8 and GPIO9: they are strapping pins on the C3 and can stop the board from booting.

The 5V pin is only powered from USB. Running on a battery later needs a 5 V boost converter, or the 3.3 V HC-SR04P variant.

### HC-SR501 settings

- Set the jumper to **H (retrigger)** so OUT stays high while motion continues.
- Turn the delay potentiometer fully anticlockwise (≈3 s) so the node can go back to motion-wake quickly.
- After power-up the PIR needs up to 60 s to settle; the firmware ignores it during that time.

## Gateway: ESP32 DevKit V1

No sensors. USB power only. The onboard LED (GPIO2) blinks on every received report.
