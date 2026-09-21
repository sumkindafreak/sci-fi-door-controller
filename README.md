# Sci-Fi Door Controller

Standalone animated door-lighting controller for an ESP32-C3 SuperMini, mechanical limit switch, 128x64 SSD1306 OLED and WS2812B LEDs. This project is independent of Showduino.

## Hardware
| Function | Connection |
|---|---|
| WS2812B data | GPIO 2 |
| Limit switch | GPIO 4 to GND |
| OLED SDA | GPIO 5 |
| OLED SCL | GPIO 6 |
| OLED address | 0x3C |
| OLED | 128x64 SSD1306, rotated 180 degrees |

The switch uses INPUT_PULLUP: **pressed/LOW = door closed**, **released/HIGH = door open**.

Power WS2812B LEDs from an appropriate external 5 V supply and connect the LED supply ground to ESP32 ground.

## Firmware v0.1.0
Four non-blocking states: **CLOSED -> OPENING -> OPEN -> CLOSING**.

- Closed: pixels off.
- Opening: centre-out cyan/white energy pulse.
- Open: animated cyan scanner.
- Closing: blue light retracts toward the centre.
- Switch remains responsive during animations.
- Mechanical switch debounce.
- OLED live status.
- Serial diagnostics at 115200 baud.

## Arduino libraries
Install **Adafruit GFX Library**, **Adafruit SSD1306**, and **Adafruit NeoPixel** plus Espressif Arduino ESP32 board support.

## First bench test
1. OLED: SDA GPIO5, SCL GPIO6.
2. Limit switch: GPIO4 to GND.
3. WS2812B data: GPIO2.
4. Common the ESP32 and LED PSU grounds.
5. Set PIXEL_COUNT in the sketch for the fitted strip.
6. Upload firmware/sci_fi_door_controller/sci_fi_door_controller.ino.
7. Open Serial Monitor at 115200.
8. Press/release the switch and verify the state/effects.

## Next
Persistent settings and a local Wi-Fi configuration UI for pixel count, brightness, colours, speed and selectable effects.
