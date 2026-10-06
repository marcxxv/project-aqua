# Project Aqua

Project Aqua is an ESP32 water quality monitoring prototype. It samples analog turbidity, total dissolved solids (TDS), and pH sensors, reports the readings to Arduino IoT Cloud, and sends the same values to a configured ESP-NOW peer.

> **Prototype notice:** The firmware's turbidity scale and TDS conversion are placeholders that require calibration with the actual sensors and reference solutions. Readings are not certified water-safety measurements and must not be used to determine whether water is safe to drink.

## Features

- Turbidity input on GPIO 34, averaged over 10 samples and mapped to a configurable 1–5 scale (reported in the `ntu` cloud field; this is not a calibrated NTU measurement).
- TDS input on GPIO 35, converted with a placeholder voltage-to-ppm factor.
- pH input on GPIO 32, converted with configurable neutral-voltage and slope constants.
- Arduino IoT Cloud properties for `ntu`, `tds`, and `ph`.
- ESP-NOW transmission every five seconds to a configured receiver MAC address.
- Serial diagnostics at 115200 baud.

## Hardware and wiring

The source identifies these ESP32 ADC inputs:

| Signal | ESP32 pin | Firmware field |
| --- | --- | --- |
| Turbidity sensor analog output | GPIO 34 | `ntu` (mapped 1–5) |
| TDS sensor analog output | GPIO 35 | `tds` (approximate ppm) |
| pH sensor analog output | GPIO 32 | `ph` |

Connect sensor power and ground according to each sensor module's datasheet. Confirm that every analog output stays within the ESP32 input voltage limits before connecting it. The archive contains no wiring diagram or bill of materials, so verify module voltages and pinouts from the hardware you use.

## Requirements

- An ESP32 board supported by the Arduino ESP32 core.
- Arduino IDE or Arduino CLI.
- Arduino IoT Cloud libraries: `ArduinoIoTCloud` and `Arduino_ConnectionHandler`.
- The Wi-Fi network and Arduino IoT Cloud device credentials configured locally.
- Compatible analog turbidity, TDS, and pH modules.
- A receiver configured for the same ESP-NOW data structure and channel.

The project uses ESP32-specific ADC and ESP-NOW APIs. The board profile in `sketch.json` names the `esp32:esp32:esp32` target; install the Espressif ESP32 board package before compiling. Its current Arduino IDE **Flavours** are:

| Setting | Value in `sketch.json` |
| --- | --- |
| Upload speed | 921600 |
| CPU frequency | 240 MHz (Wi-Fi/BT) |
| Flash frequency / mode / size | 80 MHz / QIO / 4 MB (32 Mb) |
| Partition scheme | Default 4 MB with SPIFFS (1.2 MB app / 1.5 MB SPIFFS) |
| Core debug level | None |
| PSRAM | Disabled |
| Arduino / events run on | Core 1 / Core 1 |
| Erase all flash before upload | Disabled |
| JTAG adapter | Disabled |

These are the values currently stored with the project. If your ESP32 board differs, use its supported settings and update `sketch.json` to match.

## Setup

1. Open `project-aqua.ino` from this folder in Arduino IDE. Install the ESP32 board package and the Arduino IoT Cloud libraries if they are not already installed.
2. In Arduino IoT Cloud, create or select a Thing and configure the float properties `ntu`, `tds`, and `ph` to match the callbacks in `thingProperties.h`.
3. Generate or update the Cloud device properties for your Thing. Set the device login name in `thingProperties.h` and copy your credentials into `arduino_secrets.h`.
4. Set `broadcastAddress` in `project-aqua.ino` to your receiver's Wi-Fi MAC address. The receiver must use a compatible ESP-NOW payload: three floats in the order turbidity, TDS, and pH.
5. Confirm the sensor output voltages are safe for the ESP32 ADC pins, wire the sensors, select the matching ESP32 board and port, then compile and upload.
6. Open Serial Monitor at 115200 baud and verify sensor readings and ESP-NOW send status.

### Local credentials

`arduino_secrets.h` is intentionally excluded from version control. Copy `arduino_secrets.example.h` to `arduino_secrets.h` and fill in your own values. Never commit Wi-Fi passwords or Arduino IoT Cloud device keys.

### Calibration

- **pH:** Calibrate `VOLTAGE_AT_PH7_ADJUSTED` and `SLOPE_V_PER_PH` using appropriate pH buffer solutions and your sensor interface circuit.
- **Turbidity:** Determine whether ADC counts rise or fall as water becomes more turbid; set `adcIncreasesWithTurbidity`, `adcMin`, and `adcMax` accordingly. The current 1–5 result is an arbitrary index, despite the cloud property's `ntu` name.
- **TDS:** Replace the placeholder `voltage * 750` conversion with a conversion appropriate for the probe, interface board, cell constant, and calibration solution. Temperature compensation is not implemented.

Record your calibration method and equipment when sharing measurements. Sensor modules and analog front ends vary, so the values in the source are starting points only.

## Repository layout

```text
project-aqua/
├── project-aqua.ino
├── thingProperties.h
├── arduino_secrets.example.h
├── sketch.json
├── README.md
├── CONTRIBUTING.md
├── CODE_OF_CONDUCT.md
├── LICENSE
└── .gitignore
```

## Contributing

Bug reports, wiring notes, calibration improvements, and tested firmware changes are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) before opening an issue or pull request. Please do not include private credentials, personal Wi-Fi details, or unverified drinking-water safety claims.

## License

The supplied project metadata declares this work **Public Domain**. See [LICENSE](LICENSE) for the repository's licensing notice. Check that you have the rights to publish all included work before redistributing it.
