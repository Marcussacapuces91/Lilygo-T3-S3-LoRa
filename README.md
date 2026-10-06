# Lilygo-T3-S3-LoRa

Arduino/ESP32 development project to test LoRaWAN communication on the TTN (The Things Network) with a LilyGO T3-S3 LoRa board.

This project configures an SX1262 radio in LoRaWAN ABP mode, sends telemetry frames, and manages deep sleep / wake-up cycles to reduce power consumption.

## Overview

This repository is intended for a LilyGO T3-S3 board equipped with an ESP32-S3 and a LoRa radio. It enables:

- LoRaWAN initialization on EU868
- ABP authentication against TTN
- uplink transmission of telemetry data
- time synchronization from the network
- battery voltage measurement
- low-power sleep management
- reset-reason diagnostics

## Features

- SX1262 radio initialization with RadioLib
- LoRaWAN ABP activation
- sending payloads over configured LoRaWAN ports
- battery voltage reading via ADC
- LED control for status indication
- timezone configuration for local time display
- periodic wake-up and sleep logic for energy saving

## Hardware

- LilyGO T3-S3 LoRa board
- suitable LoRa antenna
- power source or battery compatible with the board
- LoRaWAN network such as TTN

## Requirements

To compile and flash this project, you need:

- Arduino IDE 2.x or PlatformIO
- ESP32 board support package
- RadioLib library
- appropriate ESP32-S3 toolchain and USB drivers

## Installation

1. Clone the repository:

```bash
git clone https://github.com/Marcussacapuces91/Lilygo-T3-S3-LoRa.git
cd Lilygo-T3-S3-LoRa
```

2. Open `lilygo_t3s3.ino` in Arduino IDE.

3. Install the required libraries:
   - `RadioLib`
   - ESP32 board package from Espressif

4. Create or configure the file `keys.h` required by `radio.h` for LoRaWAN ABP credentials.

Example structure:

```cpp
namespace LoRaConfig {
    constexpr uint8_t FN_NWK_S_INT_KEY[16] = { /* ... */ };
    constexpr uint8_t SN_NWK_S_INT_KEY[16] = { /* ... */ };
    constexpr uint8_t NWK_S_ENC_KEY[16] = { /* ... */ };
    constexpr uint8_t APP_S_KEY[16] = { /* ... */ };
}
```

5. Verify the board pin definitions and radio configuration according to your exact hardware revision.

6. Build and upload to the device.

## Project structure

- `lilygo_t3s3.ino` — Arduino entry point
- `application.h` — main application logic
- `radio.h` — LoRa radio and LoRaWAN configuration
- `adc.h` — battery voltage measurement
- `led.h` — LED control helper
- `LICENSE` — project license
- `README.md` — project documentation

## Behavior

On startup, the firmware:

- initializes the serial monitor
- configures the timezone
- installs the ESP32 temperature sensor driver
- initializes the LoRa radio
- connects to the LoRaWAN network using ABP
- sends a startup payload
- sets the radio to sleep mode when appropriate

During normal operation, it periodically wakes up, reads the current time and battery voltage, builds a telemetry payload, and sends it to TTN.

## TTN usage

To use this project with The Things Network:

1. create a TTN application
2. configure the device in ABP mode
3. enter the correct network session keys in `keys.h`
4. verify the regional frequency is set to EU868
5. monitor uplink payloads through the TTN console

## Notes

- This project is intended for development and testing.
- LoRaWAN keys and security material must not be committed to a public repository.
- Depending on the exact board revision, some pin definitions may differ.
- Always validate the radio setup before production deployment.

## Related resources

- https://github.com/pmanzoni/lilygot3s3-lorawan/tree/main
- https://github.com/jgromes/RadioLib/tree/master/examples
- https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series/tree/master/examples
- https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series/blob/master/schematic/T3_S3_V1.3.pdf
- https://wiki.lilygo.cc/products/t3-series/t3-s3/

## License

This project is distributed under the MIT license. See the `LICENSE` file for details.
