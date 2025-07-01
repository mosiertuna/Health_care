# Healthcare RFID System - ESP32 Module

## Overview
This ESP32 code is part of the Healthcare RFID System that communicates with the STM32F429I microcontroller. It provides:

- **UART Communication** with STM32 using a custom protocol
- **Valid Card Management** with EEPROM storage
- **Web Interface** for real-time monitoring and card management
- **WiFi Access Point** for standalone operation

## Features

### 🔗 STM32 Communication
- Receives card data (UID, validation status, weight measurements)
- Sends valid card lists to STM32
- Implements checksummed message protocol
- Automatic acknowledgment system

### 🎫 Card Management
- Add/remove valid RFID cards via web interface
- Persistent storage in EEPROM
- Real-time updates to STM32
- Support for up to 50 valid cards

### 🌐 Web Interface
- **Dashboard**: Real-time card readings and weight data
- **Card Management**: Add/remove authorized cards
- **Responsive Design**: Works on mobile and desktop
- **Auto-refresh**: Live data updates

### 📡 Connectivity
- WiFi Access Point mode
- Web server on port 80
- JSON API endpoints

## Hardware Connections

```
ESP32 DevKit V1    →    STM32F429I
GPIO1 (TX0)        →    PA10 (UART1_RX)
GPIO3 (RX0)        →    PA9 (UART1_TX)
GND                →    GND
```

## WiFi Configuration

**Default Settings:**
- SSID: `HealthcareRFID_AP`
- Password: `healthcare123`
- IP Address: `192.168.4.1`

## Web Interface URLs

- **Dashboard**: `http://192.168.4.1/`
- **Card Management**: `http://192.168.4.1/manage`
- **JSON Data API**: `http://192.168.4.1/data`
- **Cards List API**: `http://192.168.4.1/cards`

## Communication Protocol

### Message Format
```
[START][TYPE][LENGTH][DATA][CHECKSUM][END]
```

- **START**: 0xAA
- **TYPE**: Message type
  - 0x01: Card Data (STM32 → ESP32)
  - 0x02: Valid Cards (ESP32 → STM32)
  - 0x03: Acknowledgment
- **LENGTH**: Data length in bytes
- **DATA**: Message payload
- **CHECKSUM**: XOR of TYPE, LENGTH, and DATA
- **END**: 0x55

### Card Data Message (STM32 → ESP32)
```
UID (4 bytes) + Status (1 byte) + Weight (4 bytes float)
```

### Valid Cards Message (ESP32 → STM32)
```
UID1 (4 bytes) + UID2 (4 bytes) + ... + UIDn (4 bytes)
```

## API Endpoints

### GET /data
Returns current card reading data
```json
{
  "hasData": true,
  "uid": "12:34:56:78",
  "status": "VALID",
  "weight": 75.2,
  "timestamp": 1625097600000
}
```

### GET /cards
Returns list of valid cards
```json
{
  "cards": [
    {"uid": "12:34:56:78"},
    {"uid": "AB:CD:EF:01"}
  ],
  "count": 2
}
```

### POST /add_card
Add a new valid card
```
Parameters: uid=12:34:56:78
Response: "Card added successfully"
```

### POST /remove_card
Remove a valid card
```
Parameters: uid=12:34:56:78
Response: "Card removed successfully"
```

## Building and Uploading

### Using PlatformIO
```bash
# Build the project
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor
```

### Using PlatformIO IDE
1. Open project in PlatformIO IDE
2. Click "Build" to compile
3. Click "Upload" to flash to ESP32
4. Use "Serial Monitor" to view debug output

## Configuration

### WiFi Settings
Edit these lines in `main.cpp`:
```cpp
const char* ssid = "HealthcareRFID_AP";
const char* password = "healthcare123";
```

### Card Storage
- Maximum cards: 50 (configurable via `MAX_VALID_CARDS`)
- EEPROM storage: 512 bytes
- Automatic backup on changes

### Serial Communication
- Baud rate: 115200
- Hardware Serial1 (GPIO1/GPIO3)
- Message timeout: Handled automatically

## Usage Instructions

### Initial Setup
1. Upload code to ESP32
2. Power on ESP32 and STM32
3. Connect to WiFi network "HealthcareRFID_AP"
4. Open web browser to `192.168.4.1`

### Adding Valid Cards
1. Go to Card Management page
2. Enter UID in format XX:XX:XX:XX
3. Click "Add Card"
4. Card is automatically sent to STM32

### Monitoring Operations
1. View dashboard for real-time data
2. Check card validation status
3. Monitor weight measurements
4. View timestamps of latest readings

## Troubleshooting

### Connection Issues
- Verify GPIO1/GPIO3 connections to STM32
- Check common ground connection
- Ensure both devices powered properly

### WiFi Problems
- Reset ESP32 if AP doesn't start
- Check for interference on 2.4GHz
- Verify password is at least 8 characters

### Card Management Issues
- Use correct UID format (XX:XX:XX:XX)
- Check EEPROM storage isn't full
- Verify web interface connectivity

### Communication Errors
- Monitor serial output for protocol errors
- Check message checksums
- Verify STM32 is sending data

## Debug Information

Enable detailed logging by monitoring the serial port at 115200 baud:
- Card additions/removals
- Message protocol details
- STM32 communication status
- Web interface requests

## Extending the System

### Database Integration
Add database storage for card history:
```cpp
// Add MySQL or PostgreSQL client
// Store readings with timestamps
// Generate reports and analytics
```

### Cloud Connectivity
Add MQTT or HTTP client for cloud services:
```cpp
// Send data to AWS IoT, Google Cloud IoT
// Implement remote monitoring
// Add push notifications
```

### Additional Features
- Add buzzer for audio feedback
- Implement user authentication
- Add data logging to SD card
- Create mobile app interface
