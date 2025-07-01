/*
 * Healthcare RFID System - ESP32 Communication Module
 * STM32-ESP32 RFID/Weight System Communication & Web Interface
 */

#include <WiFi.h>
#include <WebServer.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// WiFi Configuration
const char* ap_ssid = "HealthcareRFID";
const char* ap_password = "12345678";
bool useStationMode = false;

// Serial Communication
#define STM32_SERIAL_BAUD   115200
#define DEBUG_SERIAL_BAUD   115200

// Communication Protocol Constants
#define MSG_START_BYTE      0xAA
#define MSG_END_BYTE        0x55

// Message types
#define MSG_TYPE_CARD_REGISTERED    0x01
#define MSG_TYPE_CARD_UNREGISTERED  0x02
#define MSG_TYPE_REGISTER_CARD      0x03

// Legacy compatibility
#define MSG_TYPE_CARD_DATA  0x01
#define MSG_TYPE_VALID_CARDS 0x04
#define MSG_TYPE_ACK        0x03

#define CARD_STATUS_VALID   0x01
#define CARD_STATUS_INVALID 0x00
#define UID_SIZE           4
#define MAX_VALID_CARDS    50
#define UART_BUFFER_SIZE   256

// EEPROM Storage
#define EEPROM_SIZE        512
#define EEPROM_CARD_COUNT_ADDR  0
#define EEPROM_CARDS_START_ADDR 4

// Global Variables
WebServer server(80);
HardwareSerial stm32Serial(1);

// Valid Cards Database
struct ValidCard {
    uint8_t uid[UID_SIZE];
    bool active;
};

ValidCard validCards[MAX_VALID_CARDS];
uint8_t validCardCount = 0;

// Latest received data
struct CardReading {
    uint8_t uid[UID_SIZE];
    uint8_t status;
    float weight;
    unsigned long timestamp;
    bool hasData;
} latestReading = {0};

// Last scanned card
struct ScannedCard {
    uint8_t uid[UID_SIZE];
    bool isValid;
    float weight;
    unsigned long timestamp;
    bool hasData;
} lastScannedCard = {0};

// Weight History Storage
#define MAX_WEIGHT_HISTORY 20
struct WeightRecord {
    uint8_t uid[UID_SIZE];
    float weight;
    unsigned long timestamp;
    bool valid;
};

WeightRecord weightHistory[MAX_WEIGHT_HISTORY];
uint8_t historyIndex = 0;
uint8_t historyCount = 0;

// Message buffer for STM32 communication
uint8_t rxBuffer[UART_BUFFER_SIZE];
uint8_t rxIndex = 0;

// Function prototypes
void setupWiFi();
void initValidCards();
void saveValidCardsToEEPROM();
void loadValidCardsFromEEPROM();
bool addValidCard(uint8_t* uid);
bool removeValidCard(uint8_t* uid);
void sendValidCardsToSTM32();
void processSTM32Message();
void processCompleteMessage(uint8_t msgType);
void sendAckToSTM32();
void sendRegisterCardMessage(uint8_t* uid);
String uidToString(uint8_t* uid);
void stringToUID(String uidStr, uint8_t* uid);
bool isValidUID(String uidStr);
void addWeightRecord(uint8_t* uid, float weight, unsigned long timestamp);
String getWeightHistoryForCard(uint8_t* uid);
void setupWiFi();

// Web Interface Functions
String getMainPage();
String getCardManagementPage();
void handleRoot();
void handleData();
void handleCards();
void handleCardManagementPage();
void handleAddCard();
void handleRemoveCard();
void handleRefresh();
void handleWeightHistory();
void handleTestUART();

void setup() {
    Serial.begin(DEBUG_SERIAL_BAUD);
    delay(1000);
    Serial.println("Healthcare RFID System - ESP32 Starting...");
    
    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);
    
    // Initialize STM32 Serial communication - GPIO4(RX), GPIO2(TX)
    stm32Serial.begin(STM32_SERIAL_BAUD, SERIAL_8N1, 4, 2);
    
    // Load valid cards from EEPROM
    loadValidCardsFromEEPROM();
    
    // Setup WiFi Access Point
    setupWiFi();
    
    // Setup web server routes
    server.on("/", handleRoot);
    server.on("/data", HTTP_GET, handleData);
    server.on("/cards", HTTP_GET, handleCards);
    server.on("/manage", HTTP_GET, handleCardManagementPage);
    server.on("/add_card", HTTP_POST, handleAddCard);
    server.on("/remove_card", HTTP_POST, handleRemoveCard);
    server.on("/refresh", HTTP_POST, handleRefresh);
    server.on("/weight_history", HTTP_GET, handleWeightHistory);
    server.on("/test_uart", HTTP_GET, handleTestUART);
    
    // Start web server
    server.begin();
    Serial.println("Web Server Started - Access: http://192.168.4.1");
    
    // Send initial valid cards list to STM32
    delay(2000);
    sendValidCardsToSTM32();
    
    Serial.println("System Ready for UART Communication");
}

void loop() {
    server.handleClient();
    processSTM32Message();
    delay(10);
}

void initValidCards() {
    validCardCount = 0;
    uint8_t card1[] = {0x12, 0x34, 0x56, 0x78};
    uint8_t card2[] = {0xAB, 0xCD, 0xEF, 0x01};
    addValidCard(card1);
    addValidCard(card2);
    saveValidCardsToEEPROM();
}

void saveValidCardsToEEPROM() {
    EEPROM.write(EEPROM_CARD_COUNT_ADDR, validCardCount);
    for (int i = 0; i < validCardCount; i++) {
        int addr = EEPROM_CARDS_START_ADDR + (i * (UID_SIZE + 1));
        for (int j = 0; j < UID_SIZE; j++) {
            EEPROM.write(addr + j, validCards[i].uid[j]);
        }
        EEPROM.write(addr + UID_SIZE, validCards[i].active ? 1 : 0);
    }
    EEPROM.commit();
}

void loadValidCardsFromEEPROM() {
    validCardCount = EEPROM.read(EEPROM_CARD_COUNT_ADDR);
    
    if (validCardCount > MAX_VALID_CARDS || validCardCount == 0 || validCardCount == 255) {
        initValidCards();
        return;
    }
    
    for (int i = 0; i < validCardCount; i++) {
        int addr = EEPROM_CARDS_START_ADDR + (i * (UID_SIZE + 1));
        for (int j = 0; j < UID_SIZE; j++) {
            validCards[i].uid[j] = EEPROM.read(addr + j);
        }
        validCards[i].active = EEPROM.read(addr + UID_SIZE) == 1;
    }
}

bool addValidCard(uint8_t* uid) {
    if (validCardCount >= MAX_VALID_CARDS) {
        return false;
    }
    
    // Check if card already exists
    for (int i = 0; i < validCardCount; i++) {
        if (memcmp(validCards[i].uid, uid, UID_SIZE) == 0) {
            validCards[i].active = true;
            return true;
        }
    }
    
    // Add new card
    memcpy(validCards[validCardCount].uid, uid, UID_SIZE);
    validCards[validCardCount].active = true;
    validCardCount++;
    
    saveValidCardsToEEPROM();
    sendValidCardsToSTM32();
    
    return true;
}

bool removeValidCard(uint8_t* uid) {
    for (int i = 0; i < validCardCount; i++) {
        if (memcmp(validCards[i].uid, uid, UID_SIZE) == 0) {
            validCards[i].active = false;
            saveValidCardsToEEPROM();
            sendValidCardsToSTM32();
            return true;
        }
    }
    return false;
}

void sendValidCardsToSTM32() {
    uint8_t buffer[UART_BUFFER_SIZE];
    uint8_t index = 0;
    uint8_t activeCards = 0;
    
    for (int i = 0; i < validCardCount; i++) {
        if (validCards[i].active) {
            activeCards++;
        }
    }
    
    uint8_t dataLength = activeCards * UID_SIZE;
    uint8_t checksum = MSG_TYPE_VALID_CARDS ^ dataLength;
    
    buffer[index++] = MSG_START_BYTE;
    buffer[index++] = MSG_TYPE_VALID_CARDS;
    buffer[index++] = dataLength;
    
    for (int i = 0; i < validCardCount; i++) {
        if (validCards[i].active) {
            for (int j = 0; j < UID_SIZE; j++) {
                buffer[index++] = validCards[i].uid[j];
                checksum ^= validCards[i].uid[j];
            }
        }
    }
    
    buffer[index++] = checksum;
    buffer[index++] = MSG_END_BYTE;
    
    stm32Serial.write(buffer, index);
}

void processSTM32Message() {
    while (stm32Serial.available() && rxIndex < UART_BUFFER_SIZE) {
        uint8_t receivedByte = stm32Serial.read();
        rxBuffer[rxIndex] = receivedByte;
        
        if (rxIndex == 0) {
            if (receivedByte != MSG_START_BYTE) {
                rxIndex = 0;
                continue;
            }
        }
        
        if (rxIndex >= 1) {
            uint8_t msgType = rxBuffer[1];
            bool messageComplete = false;
            uint8_t expectedLength = 0;
            
            switch (msgType) {
                case MSG_TYPE_CARD_REGISTERED:
                    expectedLength = 10;
                    break;
                case MSG_TYPE_CARD_UNREGISTERED:
                    expectedLength = 6;
                    break;
                default:
                    if (rxIndex > 1) {
                        rxIndex = 0;
                        continue;
                    }
                    break;
            }
            
            if (expectedLength > 0 && rxIndex + 1 == expectedLength) {
                if (receivedByte == MSG_END_BYTE) {
                    messageComplete = true;
                }
            }
            
            if (msgType == MSG_TYPE_CARD_UNREGISTERED && rxIndex >= 5) {
                if (rxIndex >= 5) {
                    processCompleteMessage(msgType);
                    rxIndex = 0;
                    continue;
                }
            }
            
            if (messageComplete) {
                processCompleteMessage(msgType);
                rxIndex = 0;
                continue;
            }
        }
        
        rxIndex++;
        
        static unsigned long lastByteTime = millis();
        if (rxIndex > 0) {
            lastByteTime = millis();
        }
        
        if (rxIndex > 2 && (millis() - lastByteTime) > 100) {
            uint8_t msgType = rxBuffer[1];
            if (msgType == MSG_TYPE_CARD_UNREGISTERED && rxIndex >= 5) {
                processCompleteMessage(msgType);
            }
            rxIndex = 0;
        }
    }
    
    if (rxIndex >= UART_BUFFER_SIZE - 1) {
        rxIndex = 0;
    }
}

void processCompleteMessage(uint8_t msgType) {
    switch (msgType) {
        case MSG_TYPE_CARD_REGISTERED: {
            uint8_t* uid = &rxBuffer[2];
            float weight;
            memcpy(&weight, &rxBuffer[6], 4);
            
            memcpy(latestReading.uid, uid, UID_SIZE);
            latestReading.status = CARD_STATUS_VALID;
            latestReading.weight = weight;
            latestReading.timestamp = millis();
            latestReading.hasData = true;
            
            memcpy(lastScannedCard.uid, uid, UID_SIZE);
            lastScannedCard.isValid = true;
            lastScannedCard.weight = weight;
            lastScannedCard.timestamp = millis();
            lastScannedCard.hasData = true;
            
            Serial.printf("REGISTERED CARD: %02X:%02X:%02X:%02X Weight: %.2f g\n", 
                         uid[0], uid[1], uid[2], uid[3], weight);
            
            addWeightRecord(uid, weight, millis());
            break;
        }
        
        case MSG_TYPE_CARD_UNREGISTERED: {
            uint8_t* uid = &rxBuffer[2];
            
            memcpy(latestReading.uid, uid, UID_SIZE);
            latestReading.status = CARD_STATUS_INVALID;
            latestReading.weight = 0.0f;
            latestReading.timestamp = millis();
            latestReading.hasData = true;
            
            memcpy(lastScannedCard.uid, uid, UID_SIZE);
            lastScannedCard.isValid = false;
            lastScannedCard.weight = 0.0f;
            lastScannedCard.timestamp = millis();
            lastScannedCard.hasData = true;
            
            Serial.printf("UNREGISTERED CARD: %02X:%02X:%02X:%02X\n", 
                         uid[0], uid[1], uid[2], uid[3]);
            break;
        }
    }
}

void sendAckToSTM32() {
    uint8_t buffer[6];
    uint8_t checksum = MSG_TYPE_ACK ^ 0; // No data
    
    buffer[0] = MSG_START_BYTE;
    buffer[1] = MSG_TYPE_ACK;
    buffer[2] = 0; // No data
    buffer[3] = checksum;
    buffer[4] = MSG_END_BYTE;
    
    stm32Serial.write(buffer, 5);
}

void sendRegisterCardMessage(uint8_t* uid) {
    uint8_t buffer[7];
    buffer[0] = MSG_START_BYTE;
    buffer[1] = MSG_TYPE_REGISTER_CARD;
    memcpy(&buffer[2], uid, UID_SIZE);
    buffer[6] = MSG_END_BYTE;
    
    Serial.printf("ESP32: Sending register message: ");
    for (int i = 0; i < 7; i++) {
        Serial.printf("0x%02X ", buffer[i]);
    }
    Serial.println();
    
    stm32Serial.write(buffer, 7);
    stm32Serial.flush(); // Ensure data is sent
    delay(100); // Give STM32 time to process
    
    Serial.printf("REGISTER CARD: %02X:%02X:%02X:%02X\n", uid[0], uid[1], uid[2], uid[3]);
}

String uidToString(uint8_t* uid) {
    String result = "";
    for (int i = 0; i < UID_SIZE; i++) {
        if (i > 0) result += ":";
        if (uid[i] < 0x10) result += "0";
        result += String(uid[i], HEX);
    }
    result.toUpperCase();
    return result;
}

void stringToUID(String uidStr, uint8_t* uid) {
    uidStr.toUpperCase();
    int index = 0;
    for (int i = 0; i < uidStr.length() && index < UID_SIZE; i += 3) {
        String byteStr = uidStr.substring(i, i + 2);
        uid[index++] = strtol(byteStr.c_str(), NULL, 16);
    }
}

bool isValidUID(String uidStr) {
    if (uidStr.length() != 11) return false; // Format: XX:XX:XX:XX
    for (int i = 0; i < uidStr.length(); i++) {
        if (i % 3 == 2) {
            if (uidStr[i] != ':') return false;
        } else {
            if (!isHexadecimalDigit(uidStr[i])) return false;
        }
    }
    return true;
}

void addWeightRecord(uint8_t* uid, float weight, unsigned long timestamp) {
    memcpy(weightHistory[historyIndex].uid, uid, UID_SIZE);
    weightHistory[historyIndex].weight = weight;
    weightHistory[historyIndex].timestamp = timestamp;
    weightHistory[historyIndex].valid = true;
    
    historyIndex = (historyIndex + 1) % MAX_WEIGHT_HISTORY;
    if (historyCount < MAX_WEIGHT_HISTORY) {
        historyCount++;
    }
}

String getWeightHistoryForCard(uint8_t* uid) {
    String history = "";
    int count = 0;
    
    for (int i = 0; i < historyCount && count < 5; i++) {
        int index = (historyIndex - 1 - i + MAX_WEIGHT_HISTORY) % MAX_WEIGHT_HISTORY;
        if (weightHistory[index].valid && 
            memcmp(weightHistory[index].uid, uid, UID_SIZE) == 0) {
            
            unsigned long timeAgo = (millis() - weightHistory[index].timestamp) / 1000;
            String timeStr;
            if (timeAgo < 60) {
                timeStr = String(timeAgo) + "s ago";
            } else if (timeAgo < 3600) {
                timeStr = String(timeAgo / 60) + "m ago";
            } else {
                timeStr = String(timeAgo / 3600) + "h ago";
            }
            
            history += "<div class='history-item'>";
            history += "<span class='weight'>" + String(weightHistory[index].weight, 1) + " g</span>";
            history += "<span class='time'>" + timeStr + "</span>";
            history += "</div>";
            count++;
        }
    }
    
    if (count == 0) {
        history = "<div class='no-history'>No previous records</div>";
    }
    
    return history;
}

void setupWiFi() {
    WiFi.mode(WIFI_AP);
    
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    
    if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
        Serial.println("AP Config Failed");
    }
    
    if (WiFi.softAP(ap_ssid, ap_password)) {
        Serial.printf("WiFi AP Started: %s\n", ap_ssid);
        Serial.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
    } else {
        Serial.println("Failed to start Access Point!");
    }
}


// Web Interface Implementation
String getMainPage() {
    String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Healthcare RFID System</title>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <style>
        body { 
            font-family: Arial, Helvetica, 'Microsoft JhengHei', 'Microsoft YaHei', sans-serif; 
            margin: 0; padding: 20px; 
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: #333;
        }
        .container { 
            max-width: 800px; margin: 0 auto; 
            background: white; 
            border-radius: 15px; 
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            overflow: hidden;
        }
        .header { 
            background: linear-gradient(45deg, #4CAF50, #45a049);
            color: white; 
            padding: 30px 20px; 
            text-align: center;
        }
        .header h1 { margin: 0; font-size: 2em; }
        .content { padding: 30px; }
        .data-card { 
            margin: 20px 0; 
            padding: 25px; 
            border: 1px solid #e0e0e0; 
            border-radius: 10px;
            background: #f9f9f9;
            transition: all 0.3s ease;
        }
        .data-card:hover { 
            box-shadow: 0 5px 15px rgba(0,0,0,0.1);
            transform: translateY(-2px);
        }
        .data-label { 
            font-weight: bold; 
            color: #666;
            display: inline-block;
            width: 120px;
        }
        .data-value { 
            font-size: 1.3em; 
            color: #2196F3;
            font-weight: 600;
        }
        .status-valid { color: #4CAF50; }
        .status-invalid { color: #f44336; }
        .button { 
            padding: 12px 25px; 
            margin: 10px 5px;
            font-size: 1em; 
            border: none;
            border-radius: 5px;
            cursor: pointer;
            transition: all 0.3s ease;
            text-decoration: none;
            display: inline-block;
        }
        .btn-primary { background: #2196F3; color: white; }
        .btn-primary:hover { background: #1976D2; }
        .btn-success { background: #4CAF50; color: white; }
        .btn-success:hover { background: #45a049; }
        .btn-secondary { background: #6c757d; color: white; }
        .btn-secondary:hover { background: #5a6268; }
        .weight-history { 
            margin-top: 20px; 
            padding: 15px;
            background: #e3f2fd;
            border-radius: 8px;
            border-left: 4px solid #2196F3;
        }
        .weight-history h4 { 
            margin: 0 0 10px 0; 
            color: #1976D2;
            font-size: 1.1em;
        }
        .history-item { 
            display: flex; 
            justify-content: space-between;
            padding: 8px 0;
            border-bottom: 1px solid #bbdefb;
        }
        .history-item:last-child { border-bottom: none; }
        .history-item .weight { 
            font-weight: bold; 
            color: #1976D2;
        }
        .history-item .time { 
            color: #666; 
            font-size: 0.9em;
        }
        .no-history { 
            color: #888; 
            font-style: italic;
            text-align: center;
            padding: 10px;
        }
        .timestamp { 
            font-size: 0.9em; 
            color: #888;
            margin-top: 10px;
        }
        .no-data { 
            text-align: center; 
            color: #888;
            font-style: italic;
            padding: 40px;
        }
        @media (max-width: 600px) {
            .container { margin: 10px; }
            .content { padding: 20px; }
        }
    </style>
</head>
<body>
    <div class='container'>
        <div class='header'>
            <h1>Healthcare RFID System</h1>
            <p>Real-time Patient Card & Weight Monitoring</p>
            <div style='font-size: 0.9em; margin-top: 10px;'>
                WiFi: )" + (WiFi.getMode() == WIFI_STA ? 
                    "Connected (" + WiFi.localIP().toString() + ")" : 
                    "AP Mode (" + WiFi.softAPIP().toString() + ")") + R"(
            </div>
        </div>
        <div class='content'>)";

    if (latestReading.hasData) {
        unsigned long timeSince = (millis() - latestReading.timestamp) / 1000;
        
        html += R"(
            <div class='data-card'>
                <h3>Latest Reading</h3>
                <p><span class='data-label'>Card UID:</span> 
                   <span class='data-value' id='uid'>)" + uidToString(latestReading.uid) + R"(</span></p>
                <p><span class='data-label'>Status:</span> 
                   <span class='data-value )" + 
                   (latestReading.status == CARD_STATUS_VALID ? "status-valid'>VALID" : "status-invalid'>INVALID") + 
                   R"(</span></p>
                <p><span class='data-label'>Weight:</span> 
                   <span class='data-value' id='weight'>)" + String(latestReading.weight, 1) + R"( g</span></p>
                <div class='timestamp'>Last updated: )" + String(timeSince) + R"( seconds ago</div>
                
                <div class='weight-history'>
                    <h4>Recent Weight History:</h4>
                    )" + getWeightHistoryForCard(latestReading.uid) + R"(
                </div>
            </div>)";
        
        // Show unregistered card management if card is invalid
        if (latestReading.status == CARD_STATUS_INVALID) {
            html += R"(
            <div class='data-card' style='border-left: 4px solid #f44336; background: #ffebee;'>
                <h3>Unregistered Card Detected</h3>
                <p>The card <strong>)" + uidToString(latestReading.uid) + R"(</strong> is not registered in the system.</p>
                <p>You can register this card to allow access and weight measurement.</p>
                <button onclick='addThisCard(")" + uidToString(latestReading.uid) + R"(")' 
                        class='button btn-primary' style='background: #4CAF50;'>
                    Register This Card
                </button>
                <div id='addCardMessage' style='margin-top: 10px; display: none;'></div>
            </div>)";
        }
    } else {
        html += R"(
            <div class='data-card no-data'>
                <h3>No Data Available</h3>
                <p>Waiting for card readings from STM32...</p>
            </div>)";
    }

    html += R"(
            <div class='data-card'>
                <h3> System Controls</h3>
                <a href='/manage' class='button btn-primary'>Manage Valid Cards</a>
                <button onclick='refreshData()' class='button btn-success'>Refresh Data</button>
                <a href='/weight_history' class='button btn-secondary'>View All Weight History</a>
            </div>
        </div>
    </div>
    
    <script>
        function refreshData() {
            console.log('Manual refresh requested');
            location.reload();
        }
        
        // Auto-refresh every 10 seconds
        setInterval(function() {
            console.log('Auto-refresh: Fetching data...');
            fetch('/data')
                .then(response => {
                    console.log('Response status:', response.status);
                    return response.json();
                })
                .then(data => {
                    console.log('Data received:', data);
                    if (data.hasData) {
                        const uidElement = document.getElementById('uid');
                        const weightElement = document.getElementById('weight');
                        if (uidElement && weightElement) {
                            uidElement.innerText = data.uid;
                            weightElement.innerText = data.weight.toFixed(1) + ' g';
                            console.log('Updated UI with new data');
                        } else {
                            console.log('UI elements not found, page may need full reload');
                            location.reload();
                        }
                    } else {
                        console.log('No data available from STM32');
                    }
                })
                .catch(error => {
                    console.log('Auto-refresh error:', error);
                    // Don't reload on error to avoid infinite loop
                });
        }, 10000);
        
        // Function to add card from unregistered card
        function addThisCard(uid) {
            console.log('Adding card:', uid);
            
            fetch('/add_card', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'uid=' + encodeURIComponent(uid)
            })
            .then(response => response.text())
            .then(data => {
                const messageDiv = document.getElementById('addCardMessage');
                if (data.includes('success')) {
                    messageDiv.innerHTML = '<div style="color: #4CAF50; font-weight: bold;">Card registered successfully! It will now be accepted by the system.</div>';
                    messageDiv.style.display = 'block';
                    console.log('Card added successfully');
                    // Refresh page after 3 seconds
                    setTimeout(() => location.reload(), 3000);
                } else {
                    messageDiv.innerHTML = '<div style="color: #f44336; font-weight: bold;">Failed to register card: ' + data + '</div>';
                    messageDiv.style.display = 'block';
                }
            })
            .catch(error => {
                const messageDiv = document.getElementById('addCardMessage');
                messageDiv.innerHTML = '<div style="color: #f44336; font-weight: bold;">Error: ' + error + '</div>';
                messageDiv.style.display = 'block';
                console.error('Error adding card:', error);
            });
        }
        
        // Log page load
        console.log('Healthcare RFID System - Web Interface Loaded');
        console.log('Auto-refresh enabled (10 second interval)');
    </script>
</body>
</html>)";

    return html;
}

String getCardManagementPage() {
    String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Card Management - Healthcare RFID</title>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <style>
        body { 
            font-family: Arial, Helvetica, 'Microsoft JhengHei', 'Microsoft YaHei', sans-serif; 
            margin: 0; padding: 20px; 
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        }
        .container { 
            max-width: 900px; margin: 0 auto; 
            background: white; 
            border-radius: 15px; 
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            overflow: hidden;
        }
        .header { 
            background: linear-gradient(45deg, #FF6B6B, #FF8E8E);
            color: white; 
            padding: 30px 20px; 
            text-align: center;
        }
        .content { padding: 30px; }
        .form-section { 
            background: #f8f9fa; 
            padding: 25px; 
            margin: 20px 0;
            border-radius: 10px;
            border-left: 4px solid #FF6B6B;
        }
        .form-group { margin: 15px 0; }
        .form-group label { 
            display: block; 
            margin-bottom: 5px; 
            font-weight: bold;
            color: #333;
        }
        .form-group input { 
            width: 100%; 
            padding: 12px; 
            border: 2px solid #ddd;
            border-radius: 5px;
            font-size: 1em;
            transition: border-color 0.3s;
        }
        .form-group input:focus { 
            border-color: #FF6B6B;
            outline: none;
        }
        .button { 
            padding: 12px 25px; 
            margin: 10px 5px;
            font-size: 1em; 
            border: none;
            border-radius: 5px;
            cursor: pointer;
            transition: all 0.3s ease;
        }
        .btn-primary { background: #007bff; color: white; }
        .btn-primary:hover { background: #0056b3; }
        .btn-danger { background: #dc3545; color: white; }
        .btn-danger:hover { background: #c82333; }
        .btn-secondary { background: #6c757d; color: white; }
        .btn-secondary:hover { background: #545b62; }
        .card-list { 
            background: white;
            border: 1px solid #e0e0e0;
            border-radius: 10px;
            margin: 20px 0;
        }
        .card-item { 
            padding: 15px 20px; 
            border-bottom: 1px solid #eee;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .card-item:last-child { border-bottom: none; }
        .card-uid { 
            font-family: 'Courier New', Consolas, Monaco, monospace;
            font-weight: bold;
            color: #2196F3;
        }
        .back-link { 
            color: white; 
            text-decoration: none;
            margin-top: 10px;
            display: inline-block;
        }
        .message { 
            padding: 15px; 
            margin: 15px 0;
            border-radius: 5px;
            display: none;
        }
        .message.success { 
            background: #d4edda; 
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .message.error { 
            background: #f8d7da; 
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
    </style>
</head>
<body>
    <div class='container'>
        <div class='header'>
            <h1> Valid Card Management</h1>
            <p>Add or remove authorized RFID cards</p>
            <a href='/' class='back-link'>Back to Dashboard</a>
        </div>
        <div class='content'>
            <div id='message' class='message'></div>
            
            <div class='form-section'>
                <h3> Add New Valid Card</h3>
                <form id='addForm' onsubmit='addCard(event)'>
                    <div class='form-group'>
                        <label for='newUid'>Card UID (Format: XX:XX:XX:XX):</label>
                        <input type='text' id='newUid' name='uid' 
                               placeholder='12:34:56:78' 
                               pattern='[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}'
                               required>
                    </div>
                    <button type='submit' class='button btn-primary'>Add Card</button>
                </form>
            </div>
            
            <div class='form-section'>
                <h3> Current Valid Cards ()" + String(validCardCount) + R"()</h3>
                <div class='card-list' id='cardList'>)";

    // Add current valid cards
    for (int i = 0; i < validCardCount; i++) {
        if (validCards[i].active) {
            html += "<div class='card-item'>";
            html += "<span class='card-uid'>" + uidToString(validCards[i].uid) + "</span>";
            html += "<button onclick='removeCard(\"" + uidToString(validCards[i].uid) + "\")' class='button btn-danger'>Remove</button>";
            html += "</div>";
        }
    }

    if (validCardCount == 0) {
        html += "<div class='card-item' style='text-align: center; color: #888;'>No valid cards configured</div>";
    }

    html += R"(
                </div>
                <button onclick='refreshCards()' class='button btn-secondary'>Refresh List</button>
            </div>
        </div>
    </div>
    
    <script>
        function showMessage(text, type) {
            const msg = document.getElementById('message');
            msg.textContent = text;
            msg.className = 'message ' + type;
            msg.style.display = 'block';
            setTimeout(() => {
                msg.style.display = 'none';
            }, 5000);
        }
        
        function addCard(event) {
            event.preventDefault();
            const uid = document.getElementById('newUid').value.toUpperCase();
            
            fetch('/add_card', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'uid=' + encodeURIComponent(uid)
            })
            .then(response => response.text())
            .then(data => {
                if (data.includes('success')) {
                    showMessage('Card added successfully!', 'success');
                    document.getElementById('newUid').value = '';
                    setTimeout(() => location.reload(), 2000);
                } else {
                    showMessage('Failed to add card: ' + data, 'error');
                }
            })
            .catch(error => {
                showMessage('Error: ' + error, 'error');
            });
        }
        
        function removeCard(uid) {
            if (confirm('Are you sure you want to remove card ' + uid + '?')) {
                fetch('/remove_card', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: 'uid=' + encodeURIComponent(uid)
                })
                .then(response => response.text())
                .then(data => {
                    if (data.includes('success')) {
                        showMessage('Card removed successfully!', 'success');
                        setTimeout(() => location.reload(), 2000);
                    } else {
                        showMessage('Failed to remove card: ' + data, 'error');
                    }
                })
                .catch(error => {
                    showMessage('Error: ' + error, 'error');
                });
            }
        }
        
        function refreshCards() {
            location.reload();
        }
    </script>
</body>
</html>)";

    return html;
}

void handleRoot() {
    server.send(200, "text/html", getMainPage());
}

void handleData() {
    DynamicJsonDocument doc(1024);
    
    if (latestReading.hasData) {
        doc["hasData"] = true;
        doc["uid"] = uidToString(latestReading.uid);
        doc["status"] = (latestReading.status == CARD_STATUS_VALID) ? "VALID" : "INVALID";
        doc["weight"] = latestReading.weight;
        doc["timestamp"] = latestReading.timestamp;
        
        Serial.println("Data API called - returning card data: " + uidToString(latestReading.uid));
    } else {
        doc["hasData"] = false;
        Serial.println("Data API called - no data available");
    }
    
    // Add last scanned card info (regardless of validity)
    if (lastScannedCard.hasData) {
        JsonObject lastCard = doc.createNestedObject("lastScannedCard");
        lastCard["uid"] = uidToString(lastScannedCard.uid);
        lastCard["isValid"] = lastScannedCard.isValid;
        lastCard["weight"] = lastScannedCard.weight;
        lastCard["timestamp"] = lastScannedCard.timestamp;
        lastCard["canAdd"] = !lastScannedCard.isValid;  // Can add if not valid
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleCards() {
    DynamicJsonDocument doc(1024);
    JsonArray cardsArray = doc.createNestedArray("cards");
    
    for (int i = 0; i < validCardCount; i++) {
        if (validCards[i].active) {
            JsonObject card = cardsArray.add<JsonObject>();
            card["uid"] = uidToString(validCards[i].uid);
        }
    }
    
    doc["count"] = cardsArray.size();
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleCardManagementPage() {
    server.send(200, "text/html", getCardManagementPage());
}

void handleAddCard() {
    if (server.hasArg("uid")) {
        String uidStr = server.arg("uid");
        uidStr.toUpperCase();
        
        if (isValidUID(uidStr)) {
            uint8_t newUID[UID_SIZE];
            stringToUID(uidStr, newUID);
            
            if (addValidCard(newUID)) {
                // Send register message to STM32
                sendRegisterCardMessage(newUID);
                delay(200); // Extra delay for STM32 to process and update
                server.send(200, "text/plain", "Card added successfully and sent to STM32");
                Serial.println("Card added via web interface: " + uidStr);
            } else {
                server.send(400, "text/plain", "Failed to add card (may already exist or storage full)");
            }
        } else {
            server.send(400, "text/plain", "Invalid UID format. Use XX:XX:XX:XX format.");
        }
    } else {
        server.send(400, "text/plain", "Missing UID parameter");
    }
}

void handleRemoveCard() {
    if (server.hasArg("uid")) {
        String uidStr = server.arg("uid");
        uidStr.toUpperCase();
        
        if (isValidUID(uidStr)) {
            uint8_t targetUID[UID_SIZE];
            stringToUID(uidStr, targetUID);
            
            if (removeValidCard(targetUID)) {
                server.send(200, "text/plain", "Card removed successfully");
                Serial.println("Card removed via web interface: " + uidStr);
            } else {
                server.send(400, "text/plain", "Card not found");
            }
        } else {
            server.send(400, "text/plain", "Invalid UID format");
        }
    } else {
        server.send(400, "text/plain", "Missing UID parameter");
    }
}

void handleRefresh() {
    sendValidCardsToSTM32();
    server.send(200, "text/plain", "Valid cards list refreshed and sent to STM32");
}

void handleWeightHistory() {
    String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Weight History - Healthcare RFID</title>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <style>
        body { 
            font-family: Arial, Helvetica, 'Microsoft JhengHei', 'Microsoft YaHei', sans-serif; 
            margin: 0; padding: 20px; 
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        }
        .container { 
            max-width: 1000px; margin: 0 auto; 
            background: white; 
            border-radius: 15px; 
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
            overflow: hidden;
        }
        .header { 
            background: linear-gradient(45deg, #9C27B0, #E1BEE7);
            color: white; 
            padding: 30px 20px; 
            text-align: center;
        }
        .content { padding: 30px; }
        .history-table { 
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
        }
        .history-table th, .history-table td { 
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }
        .history-table th { 
            background: #f5f5f5;
            font-weight: bold;
        }
        .card-uid { 
            font-family: 'Courier New', Consolas, Monaco, monospace;
            font-weight: bold;
            color: #2196F3;
        }
        .weight-value { 
            font-size: 1.2em;
            font-weight: bold;
            color: #4CAF50;
        }
        .back-link { 
            color: white; 
            text-decoration: none;
            margin-top: 10px;
            display: inline-block;
        }
        .no-data { 
            text-align: center; 
            color: #888;
            font-style: italic;
            padding: 40px;
        }
    </style>
</head>
<body>
    <div class='container'>
        <div class='header'>
            <h1>Weight History</h1>
            <p>Complete weight measurement history</p>
            <a href='/' class='back-link'>Back to Dashboard</a>
        </div>
        <div class='content'>)";

    if (historyCount > 0) {
        html += R"(
            <table class='history-table'>
                <thead>
                    <tr>
                        <th>Card UID</th>
                        <th>Weight</th>
                        <th>Time</th>
                    </tr>
                </thead>
                <tbody>)";

        // Display history (newest first)
        for (int i = 0; i < historyCount; i++) {
            int index = (historyIndex - 1 - i + MAX_WEIGHT_HISTORY) % MAX_WEIGHT_HISTORY;
            if (weightHistory[index].valid) {
                unsigned long timeAgo = (millis() - weightHistory[index].timestamp) / 1000;
                String timeStr;
                if (timeAgo < 60) {
                    timeStr = String(timeAgo) + " seconds ago";
                } else if (timeAgo < 3600) {
                    timeStr = String(timeAgo / 60) + " minutes ago";
                } else {
                    timeStr = String(timeAgo / 3600) + " hours ago";
                }

                html += "<tr>";
                html += "<td><span class='card-uid'>" + uidToString(weightHistory[index].uid) + "</span></td>";
                html += "<td><span class='weight-value'>" + String(weightHistory[index].weight, 1) + " g</span></td>";
                html += "<td>" + timeStr + "</td>";
                html += "</tr>";
            }
        }

        html += R"(
                </tbody>
            </table>)";
    } else {
        html += R"(
            <div class='no-data'>
                <h3>No Weight History Available</h3>
                <p>No weight measurements have been recorded yet.</p>
            </div>)";
    }

    html += R"(
        </div>
    </div>
</body>
</html>)";

    server.send(200, "text/html", html);
}

void handleTestUART() {
    uint8_t testMsg[] = {0xAA, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x03, 0x55};
    stm32Serial.write(testMsg, sizeof(testMsg));
    
    delay(100);
    if (stm32Serial.available()) {
        while (stm32Serial.available()) {
            stm32Serial.read();
        }
    }
    
    server.send(200, "text/plain", "UART test completed");
}