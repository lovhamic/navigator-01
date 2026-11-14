# Hardware Reference and Wiring Guide

This document provides detailed electrical specifications, pinout tables, wiring schematics, and hardware recommendations for the ESP32 Multifunction OLED Display project.

---

## 1. Bill of Materials (BOM)

| Component | Quantity | Specification / Description |
|:---|:---:|:---|
| Microcontroller | 1 | ESP32-WROOM-32 Development Board (30-pin or 38-pin DevKit v1) |
| Display Module | 1 | 0.96 inch or 1.3 inch monochrome OLED display (SSD1306 / SH1106), 128x64 resolution, I2C interface |
| User Input Sensor | 1 | TTP223 Capacitive Digital Touch Sensor Module (or tactile momentary push button) |
| Power Source | 1 | Standard 5V USB power supply (micro-USB or USB-C depending on board variant) |
| Connecting Wires | 1 set | Breadboard jumper wires or 28-30 AWG hookup wires |
| Prototyping Board | 1 | Solderless breadboard or custom perfboard (optional) |

---

## 2. Pinout Configuration

The firmware assigns hardware peripherals according to the following pin configuration:

| ESP32 GPIO | Peripheral Function | Module Connection | Notes |
|:---|:---|:---|:---|
| GPIO 21 | I2C Data (SDA) | OLED Display SDA | Hardware I2C data line |
| GPIO 22 | I2C Clock (SCL) | OLED Display SCL | Hardware I2C clock line |
| GPIO 33 | Digital Input | TTP223 Touch SIG / Push Button | Active HIGH digital read with software debouncing |
| 3.3V / VIN | Power Output | OLED VCC, TTP223 VCC | 3.3V recommended; use 5V/VIN only if module includes onboard LDO |
| GND | Ground Reference | OLED GND, TTP223 GND | Common ground connection |

---

## 3. Wiring Schematic

```
                      +-------------------+
                      |   ESP32 DevKit    |
                      |                   |
                      |          3.3V/VIN |----+--------------------+
                      |               GND |----+---------+          |
                      |                   |    |         |          |
                      |           GPIO 21 |----+         |          |
                      |           GPIO 22 |----+         |          |
                      |           GPIO 33 |----+         |          |
                      +-------------------+    |         |          |
                                               |         |          |
                                               |         |          |
             +-----------------------+         |         |          |
             |   SSD1306 OLED (I2C)  |         |         |          |
             |                       |         |         |          |
             |                   VCC |<--------+---------|----------+
             |                   GND |<------------------+
             |                   SCL |<--------+ (GPIO 22)
             |                   SDA |<--------+ (GPIO 21)
             +-----------------------+                   |
                                                         |
             +-----------------------+                   |
             |   TTP223 Touch Module |                   |
             |                       |                   |
             |                   VCC |<------------------+
             |                   GND |<------------------+
             |                   SIG |<------------------+ (GPIO 33)
             +-----------------------+
```

---

## 4. Hardware Details and Considerations

### 4.1. Display Interface (I2C)
- Protocol: I2C (Inter-Integrated Circuit).
- Bus Frequency: Standard Fast Mode supported by the U8g2 full buffer driver (`U8G2_SSD1306_128X64_NONAME_F_HW_I2C`).
- Default I2C Address: `0x3C` (most modules) or `0x3D` (selectable via zero-ohm jumper on the rear of some OLED boards).
- Bus Pull-ups: Most breakout boards contain internal 4.7k ohm or 10k ohm pull-up resistors on SDA and SCL to 3.3V. If using bare glass panels, add external 4.7k ohm pull-up resistors between SDA/SCL and 3.3V.

### 4.2. Capacitive Touch Sensor (TTP223)
- The TTP223 module outputs a digital HIGH signal when a touch event is detected, and LOW when idle.
- Debounce Period: 50 milliseconds software debounce filter implemented inside `readTouch()` in firmware.
- Mode Switching Sequence:
  Touch Event -> Animation -> Digital Clock -> Spotify -> Navigation -> Reminder -> Animation.
- Tactile Push Button Alternative:
  If substituting with a mechanical tactile push button instead of a capacitive touch module:
  1. Connect one leg of the button to GPIO 33.
  2. Connect the other leg to 3.3V.
  3. Place a 10k ohm pull-down resistor between GPIO 33 and GND to prevent floating inputs when the button is open.

### 4.3. Power Requirements
- Typical Operating Current:
  - ESP32 with WiFi active (AP+STA mode): 120 mA to 240 mA peak during RF transmissions.
  - SSD1306 OLED: 15 mA to 25 mA depending on display fill factor (number of illuminated pixels).
  - TTP223 Touch Sensor: Microamp quiescent current (< 5 uA).
- Power Supply Recommendation: Ensure the USB source or external power supply provides at least 500 mA at 5V to maintain RF stability and prevent brownout detector resets (`Brownout detector was triggered`).
