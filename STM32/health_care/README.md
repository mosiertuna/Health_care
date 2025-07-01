# Healthcare RFID System

## Overview
This project implements a healthcare RFID system using STM32F429I-DISC1 microcontroller with the following components:
- RC522 RFID module for card reading
- HX711 load cell amplifier for weight measurement
- ESP32 communication via UART for data transmission

## Hardware Connections

### RC522 RFID Module
- VCC → 3.3V
- GND → GND
- SCK → PA5 (SPI1_SCK)
- MISO → PA6 (SPI1_MISO)
- MOSI → PA7 (SPI1_MOSI)
- CS → PB0 (RC522_CS_Pin)
- RST → GND (not used)
- IRQ → Not connected (or PB1 if interrupt needed)

### HX711 Weight Sensor
- VDD → 3.3V (or 5V if load cell requires it)
- VCC → 3.3V
- GND → GND
- DT → PB11 (HX711_DT_Pin)
- SCK → PB12 (HX711_SCK_Pin)
- E+ → Load cell excitation positive
- E- → Load cell excitation negative
- A+ → Load cell signal positive
- A- → Load cell signal negative

### ESP32 Communication
- GND → GND (common ground)
- STM32 PA9 (UART1_TX) → ESP32 GPIO3 (RX0)
- STM32 PA10 (UART1_RX) → ESP32 GPIO1 (TX0)

## Software Features

### RFID Card Reading
- Reads UID from RFID cards using RC522 module
- Compares with valid card list received from ESP32
- Returns card status (valid/invalid)

### Weight Measurement
- Reads weight from HX711 load cell amplifier
- Supports taring (zero calibration)
- Configurable scale factor for different load cells

### ESP32 Communication
- Sends card data (UID, status, weight) to ESP32
- Receives valid card list from ESP32
- Uses structured message protocol with checksums

### Message Protocol
Messages are formatted as: [START][TYPE][LENGTH][DATA][CHECKSUM][END]
- START: 0xAA
- TYPE: Message type (0x01 for card data, 0x02 for valid cards)
- LENGTH: Data length
- DATA: Message payload
- CHECKSUM: XOR of TYPE, LENGTH, and DATA
- END: 0x55

## Calibration

### HX711 Scale Calibration
1. Place a known weight on the scale
2. Read the raw value
3. Calculate scale factor: scale = raw_value / known_weight
4. Update the scale factor in `System_Init()` function

### Example:
```c
// If 1kg gives raw reading of 420000
HX711_SetScale(420.0f);  // Scale factor for grams
```

## Usage

1. Power on the system
2. Wait for initialization (2 seconds for tare)
3. System will automatically:
   - Check for RFID cards every 500ms
   - Monitor weight every 1000ms
   - Send data to ESP32 when card is detected

## Status Indicators
- Valid card: Can add green LED indication
- Invalid card: Can add red LED indication
- System ready: System operational after initialization

## Error Handling
- UART communication errors are handled automatically
- Invalid messages are discarded
- System continues operation even if ESP32 is disconnected

## Customization
- Adjust timing intervals in main.c:
  - `CARD_CHECK_INTERVAL`: RFID card check frequency
  - `WEIGHT_CHECK_INTERVAL`: Weight measurement frequency
- Modify scale factor for different load cells
- Add additional status indicators (LEDs, buzzer)
- Extend message protocol for additional data
