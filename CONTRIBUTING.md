# Contributing

Thanks for helping improve Project Aqua. Contributions can include firmware fixes, clearer setup instructions, wiring documentation, calibration guidance, and reports from tested hardware.

## Before opening an issue

- Include the ESP32 board and core version, sensor/interface module, wiring, and relevant serial output.
- For measurement problems, include the calibration method and reference solution where possible.
- Remove Wi-Fi credentials, Arduino IoT Cloud keys, device identifiers, and other private information from logs and screenshots.
- Do not describe prototype readings as proof that water is safe to drink.

## Pull requests

1. Keep changes focused and explain the problem they solve.
2. Update the README when pins, setup, dependencies, or calibration behavior changes.
3. Build with the ESP32 target and report the board/core and library versions used. If hardware testing was not possible, say so.
4. Never commit `arduino_secrets.h`, cloud credentials, generated build output, or unrelated personal files.
5. Use clear commit messages and describe any sensor-specific assumptions.

There is no automated test suite or verified hardware fixture in this repository yet. Please report the checks you actually performed; do not claim hardware validation based on compilation alone.
