# FM Transmitter on ESP32-C3

This project is an FM transmitter based on the ESP32-C3 microcontroller and the Adafruit Si4713 FM transmitter module. It features a web-based control interface using GyverHub, allowing you to set the frequency, power on/off, and select from preset stations via Wi-Fi.

## Features

- Control FM frequency (range: 80.00–110.00 MHz)
- Power on/off the transmitter
- Select from preset frequencies
- Frequency presets sorted by frequency
- Web interface via Wi-Fi Access Point (no password by default)
- Frequency correction offset
- Built with PlatformIO for Arduino framework

## Hardware Requirements

- ESP32-C3 development board
- Adafruit Si4713 FM transmitter module
- I2C connection (SDA: GPIO8, SCL: GPIO9 by default)
- Optional: Antenna for better transmission

## Getting Started

1. **Clone this repository**
2. **Open with PlatformIO (VS Code recommended)**
3. **Connect your hardware as follows:**
   - Si4713 RESET: GPIO10
   - I2C SDA: GPIO8
   - I2C SCL: GPIO9
4. **Build and upload the firmware**
5. **Connect to the Wi-Fi AP `ESP32C3-Radio`**
6. **Open the web interface (GyverHub) in your browser**

## File Structure

- `src/main.cpp` — Main application code
- `platformio.ini` — PlatformIO project configuration
- `include/`, `lib/`, `test/` — Standard PlatformIO folders

## Libraries Used

- [GyverHub](https://github.com/GyverLibs/GyverHub)
- [Adafruit Si4713](https://github.com/adafruit/Adafruit_Si4713)
- [Arduino Core for ESP32](https://github.com/espressif/arduino-esp32)

## Notes

- Default AP has no password. You can set one in `main.cpp` if needed.
- Preset frequencies can be changed in the `PRESETS` array in `main.cpp`.
- Make sure to use a suitable antenna for the Si4713 module.

## License

MIT License
