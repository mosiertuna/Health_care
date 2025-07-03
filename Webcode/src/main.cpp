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

// Message types - Updated for new protocol
#define MSG_TYPE_CARD_DETECTED      0x01  // STM32 -> ESP32: Any card detected with weight
#define MSG_TYPE_SYSTEM_READY       0x02  // STM32 -> ESP32: System startup
#define MSG_TYPE_ACK                0x05  // STM32 -> ESP32: Acknowledgment
#define MSG_TYPE_SYSTEM_RESET       0x07  // ESP32 -> STM32: Reset command
#define MSG_TYPE_CONFIG_UPDATE      0x08  // ESP32 -> STM32: Config update

// Legacy constants removed
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
    String name; // Optional: card holder name
};

ValidCard validCards[MAX_VALID_CARDS];
uint8_t validCardCount = 0;

// Latest received data from STM32
struct CardReading {
    uint8_t uid[UID_SIZE];
    bool isValid;
    float weight;
    unsigned long timestamp;
    bool hasData;
} latestReading = {0};

// Weight History Storage
#define MAX_WEIGHT_HISTORY 20
struct WeightRecord {
    uint8_t uid[UID_SIZE];
    float weight;
    unsigned long timestamp;
    bool valid;
    bool isValidCard;
};

WeightRecord weightHistory[MAX_WEIGHT_HISTORY];
uint8_t historyIndex = 0;
uint8_t historyCount = 0;

// Message buffer for STM32 communication
uint8_t rxBuffer[UART_BUFFER_SIZE];
uint8_t rxIndex = 0;

// Function prototypes - Updated
void setupWiFi();
void initValidCards();
void saveValidCardsToEEPROM();
void loadValidCardsFromEEPROM();
bool addValidCard(uint8_t* uid);
bool removeValidCard(uint8_t* uid);
bool isCardValid(uint8_t* uid);
void processSTM32Message();
void processCompleteMessage(uint8_t msgType);
void processCardDetected(uint8_t* uid, float weight);
void sendSystemResetToSTM32();
void sendConfigUpdateToSTM32();
String uidToString(uint8_t* uid);
void stringToUID(String uidStr, uint8_t* uid);
bool isValidUID(String uidStr);
void addWeightRecord(uint8_t* uid, float weight, unsigned long timestamp, bool isValid);
String getWeightHistoryForCard(uint8_t* uid);
void sendHTMLResponse(String html);  // Helper function for UTF-8 HTML responses
String formatUptime(unsigned long milliseconds);  // Helper function to format uptime

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
    
    // Start web server
    server.begin();
    Serial.println("Web Server Started - Access: http://192.168.4.1");
    
    Serial.println("System Ready for UART Communication");
}

void loop() {
    server.handleClient();
    processSTM32Message();
    delay(1); // Giảm delay từ 10ms xuống 1ms để phản hồi nhanh hơn
}

void setupWiFi() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
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
    
    return true;
}

bool removeValidCard(uint8_t* uid) {
    for (int i = 0; i < validCardCount; i++) {
        if (memcmp(validCards[i].uid, uid, UID_SIZE) == 0) {
            validCards[i].active = false;
            saveValidCardsToEEPROM();
            return true;
        }
    }
    return false;
}

// Check if a card is in the valid cards database
bool isCardValid(uint8_t* uid) {
    for (int i = 0; i < validCardCount; i++) {
        if (validCards[i].active && 
            memcmp(validCards[i].uid, uid, UID_SIZE) == 0) {
            return true;
        }
    }
    return false;
}

void processSTM32Message() {
    // Thêm debug để theo dõi UART
    static unsigned long lastDebugTime = 0;
    if (millis() - lastDebugTime > 5000) { // Debug mỗi 5 giây
        if (stm32Serial.available() > 0) {
            Serial.printf("UART buffer has %d bytes\n", stm32Serial.available());
        }
        lastDebugTime = millis();
    }
    
    while (stm32Serial.available() && rxIndex < UART_BUFFER_SIZE) {
        uint8_t receivedByte = stm32Serial.read();
        
        // Debug raw bytes
        Serial.printf("RX[%d]: 0x%02X\n", rxIndex, receivedByte);
        
        rxBuffer[rxIndex] = receivedByte;
        
        // Check for start byte
        if (rxIndex == 0) {
            if (receivedByte != MSG_START_BYTE) {
                Serial.printf("Waiting for start byte, got: 0x%02X\n", receivedByte);
                continue; // Wait for start byte
            }
            Serial.println("Start byte found!");
        }
        
        rxIndex++;
        
        // Check if we have enough bytes to determine message type
        if (rxIndex >= 2) {
            uint8_t msgType = rxBuffer[1];
            uint8_t expectedLength = 0;
            bool messageComplete = false;
            
            switch (msgType) {
                case MSG_TYPE_CARD_DETECTED:
                    expectedLength = 11; // AA 01 UID[4] WEIGHT[4] 55
                    break;
                case MSG_TYPE_SYSTEM_READY:
                    expectedLength = 3;  // AA 02 55
                    break;
                case MSG_TYPE_ACK:
                    expectedLength = 4;  // AA 05 TYPE 55
                    break;
                default:
                    Serial.printf("Unknown message type: 0x%02X, resetting\n", msgType);
                    rxIndex = 0;
                    continue;
            }
            
            // Check if message is complete
            if (rxIndex >= expectedLength) {
                if (rxBuffer[expectedLength - 1] == MSG_END_BYTE) {
                    messageComplete = true;
                    Serial.printf("Complete message received, type: 0x%02X, length: %d\n", msgType, expectedLength);
                } else {
                    Serial.printf("Invalid end byte: 0x%02X, expected: 0x%02X\n", rxBuffer[expectedLength - 1], MSG_END_BYTE);
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
        
        // Prevent buffer overflow
        if (rxIndex >= UART_BUFFER_SIZE) {
            Serial.println("Buffer overflow, resetting");
            rxIndex = 0;
        }
    }
    
    // Reset if buffer has been stuck for too long
    static unsigned long bufferStartTime = 0;
    if (rxIndex > 0) {
        if (bufferStartTime == 0) {
            bufferStartTime = millis();
        } else if (millis() - bufferStartTime > 500) { // 500ms timeout
            Serial.printf("Buffer timeout, resetting. Had %d bytes\n", rxIndex);
            rxIndex = 0;
            bufferStartTime = 0;
        }
    } else {
        bufferStartTime = 0;
    }
}

void processCompleteMessage(uint8_t msgType) {
    switch (msgType) {
        case MSG_TYPE_CARD_DETECTED: {
            // Extract UID and weight from message: AA 01 UID[4] WEIGHT[4] 55
            uint8_t uid[UID_SIZE];
            float weight;
            
            memcpy(uid, &rxBuffer[2], UID_SIZE);
            memcpy(&weight, &rxBuffer[6], sizeof(float));
            
            processCardDetected(uid, weight);
            break;
        }
        
        case MSG_TYPE_SYSTEM_READY: {
            Serial.println("STM32 System Ready");
            // Optionally send configuration or do initial setup
            break;
        }
        
        case MSG_TYPE_ACK: {
            uint8_t ackedType = rxBuffer[2];
            Serial.printf("STM32 ACK for message type: 0x%02X\n", ackedType);
            break;
        }
        
        default:
            Serial.printf("Unknown message type: 0x%02X\n", msgType);
            break;
    }
}

void processCardDetected(uint8_t* uid, float weight) {
    // Check if card is in valid database
    bool isValid = isCardValid(uid);
    unsigned long currentTime = millis();
    
    // Update latest reading
    memcpy(latestReading.uid, uid, UID_SIZE);
    latestReading.isValid = isValid;
    latestReading.weight = weight;
    latestReading.timestamp = currentTime;
    latestReading.hasData = true;
    
    // Add to weight history
    addWeightRecord(uid, weight, currentTime, isValid);
    
    // Log the detection
    String uidStr = uidToString(uid);
    Serial.printf("Card Detected: %s, Weight: %.1fg, Valid: %s\n", 
                  uidStr.c_str(), weight, isValid ? "YES" : "NO");
    
    // Optional: Send feedback to STM32 (LED control, buzzer, etc.)
    // For now, we just log and store the data
}

void sendSystemResetToSTM32() {
    uint8_t buffer[3];
    buffer[0] = MSG_START_BYTE;
    buffer[1] = MSG_TYPE_SYSTEM_RESET;
    buffer[2] = MSG_END_BYTE;
    
    stm32Serial.write(buffer, 3);
    Serial.println("Sent system reset command to STM32");
}

void sendConfigUpdateToSTM32() {
    uint8_t buffer[3];
    buffer[0] = MSG_START_BYTE;
    buffer[1] = MSG_TYPE_CONFIG_UPDATE;
    buffer[2] = MSG_END_BYTE;
    
    stm32Serial.write(buffer, 3);
    Serial.println("Sent config update command to STM32");
}

// Utility Functions
String uidToString(uint8_t* uid) {
    String result = "";
    for (int i = 0; i < UID_SIZE; i++) {
        if (uid[i] < 0x10) result += "0";
        result += String(uid[i], HEX);
        if (i < UID_SIZE - 1) result += ":";
    }
    result.toUpperCase();
    return result;
}

void stringToUID(String uidStr, uint8_t* uid) {
    int index = 0;
    int start = 0;
    for (int i = 0; i <= uidStr.length() && index < UID_SIZE; i++) {
        if (i == uidStr.length() || uidStr.charAt(i) == ':') {
            String byteStr = uidStr.substring(start, i);
            uid[index] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
            index++;
            start = i + 1;
        }
    }
}

bool isValidUID(String uidStr) {
    if (uidStr.length() != 11) return false; // Expected format: XX:XX:XX:XX
    for (int i = 0; i < uidStr.length(); i++) {
        char c = uidStr.charAt(i);
        if (i % 3 == 2) {
            if (c != ':') return false;
        } else {
            if (!isHexadecimalDigit(c)) return false;
        }
    }
    return true;
}

void addWeightRecord(uint8_t* uid, float weight, unsigned long timestamp, bool isValid) {
    memcpy(weightHistory[historyIndex].uid, uid, UID_SIZE);
    weightHistory[historyIndex].weight = weight;
    weightHistory[historyIndex].timestamp = timestamp;
    weightHistory[historyIndex].valid = true;
    weightHistory[historyIndex].isValidCard = isValid;
    
    historyIndex = (historyIndex + 1) % MAX_WEIGHT_HISTORY;
    if (historyCount < MAX_WEIGHT_HISTORY) {
        historyCount++;
    }
}

String getWeightHistoryForCard(uint8_t* uid) {
    String history = "";
    for (int i = 0; i < historyCount; i++) {
        int index = (historyIndex - 1 - i + MAX_WEIGHT_HISTORY) % MAX_WEIGHT_HISTORY;
        if (memcmp(weightHistory[index].uid, uid, UID_SIZE) == 0) {
            history += String(weightHistory[index].weight, 1) + "g ";
        }
    }
    return history;
}

// Helper function to send HTML with UTF-8 charset
void sendHTMLResponse(String html) {
    server.sendHeader("Content-Type", "text/html; charset=UTF-8");
    server.send(200, "text/html", html);
}

// Helper function to format uptime from milliseconds
String formatUptime(unsigned long milliseconds) {
    unsigned long seconds = milliseconds / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    
    seconds %= 60;
    minutes %= 60;
    hours %= 24;
    
    String result = "";
    if (days > 0) {
        result += String(days) + " ngày ";
    }
    if (hours > 0 || days > 0) {
        result += (hours < 10 ? "0" : "") + String(hours) + ":";
    }
    result += (minutes < 10 ? "0" : "") + String(minutes) + ":";
    result += (seconds < 10 ? "0" : "") + String(seconds);
    
    return result;
}

// Web Interface Implementation
String getMainPage() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<title>Healthcare RFID System</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
    html += "body { font-family: 'Segoe UI', Arial, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }";
    html += ".container { max-width: 1200px; margin: 0 auto; padding: 20px; }";
    html += ".header { text-align: center; color: white; margin-bottom: 30px; }";
    html += ".header h1 { font-size: 2.5rem; margin-bottom: 10px; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }";
    html += ".header p { font-size: 1.1rem; opacity: 0.9; }";
    html += ".dashboard { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; margin-bottom: 30px; }";
    html += ".card { background: white; border-radius: 15px; padding: 25px; box-shadow: 0 10px 30px rgba(0,0,0,0.2); transition: transform 0.3s ease; }";
    html += ".card:hover { transform: translateY(-5px); }";
    html += ".card-title { font-size: 1.3rem; color: #333; margin-bottom: 15px; display: flex; align-items: center; }";
    html += ".card-title::before { content: ''; margin-right: 10px; font-size: 1.5rem; }";
    html += ".status-display { font-size: 1.1rem; padding: 15px; background: #f8f9fa; border-radius: 8px; border-left: 4px solid #007bff; }";
    html += ".actions { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; }";
    html += ".btn { padding: 15px 25px; border: none; border-radius: 8px; font-size: 1rem; font-weight: 600; cursor: pointer; transition: all 0.3s ease; text-decoration: none; display: inline-block; text-align: center; color: white; }";
    html += ".btn-primary { background: linear-gradient(45deg, #007bff, #0056b3); }";
    html += ".btn-success { background: linear-gradient(45deg, #28a745, #1e7e34); }";
    html += ".btn-warning { background: linear-gradient(45deg, #ffc107, #e0a800); }";
    html += ".btn:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(0,0,0,0.2); }";
    html += ".stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px; margin-top: 15px; }";
    html += ".stat-item { text-align: center; padding: 15px; background: #f8f9fa; border-radius: 8px; }";
    html += ".stat-number { font-size: 2rem; font-weight: bold; color: #007bff; }";
    html += ".stat-label { font-size: 0.9rem; color: #666; margin-top: 5px; }";
    html += "@media (max-width: 768px) { .header h1 { font-size: 2rem; } .dashboard { grid-template-columns: 1fr; } }";
    html += "</style>";
    html += "<script>";
    html += "function updateStatus() {";
    html += "  fetch('/data').then(r => r.json()).then(d => {";
    html += "    const status = document.getElementById('status');";
    html += "    const cardCount = document.getElementById('cardCount');";
    html += "    const historyCount = document.getElementById('historyCount');";
    html += "    if (d.lastCard && d.lastCard !== 'None') {";
    html += "      const uptime = formatUptime(d.timestamp);";
    html += "      status.innerHTML = `<strong>Thẻ cuối:</strong> ${d.lastCard}<br><strong>Cân nặng:</strong> ${d.weight}g<br><strong>Hợp lệ:</strong> ${d.valid ? 'Có' : 'Không'}<br><strong>Thời gian:</strong> ${uptime}`;";
    html += "      status.style.borderLeftColor = d.valid ? '#28a745' : '#dc3545';";
    html += "    } else {";
    html += "      status.innerHTML = '<strong>Trạng thái:</strong> Đang chờ quẹt thẻ...';";
    html += "      status.style.borderLeftColor = '#007bff';";
    html += "    }";
    html += "  }).catch(e => {";
    html += "    document.getElementById('status').innerHTML = '<strong>Lỗi:</strong> Không thể kết nối đến hệ thống';";
    html += "  });";
    html += "  fetch('/cards').then(r => r.json()).then(cards => {";
    html += "    document.getElementById('cardCount').textContent = cards.length;";
    html += "  });";
    html += "}";
    html += "function formatUptime(ms) {";
    html += "  const seconds = Math.floor(ms / 1000);";
    html += "  const minutes = Math.floor(seconds / 60);";
    html += "  const hours = Math.floor(minutes / 60);";
    html += "  const days = Math.floor(hours / 24);";
    html += "  const s = seconds % 60;";
    html += "  const m = minutes % 60;";
    html += "  const h = hours % 24;";
    html += "  let result = '';";
    html += "  if (days > 0) result += days + ' ngày ';";
    html += "  if (hours > 0 || days > 0) result += (h < 10 ? '0' : '') + h + ':';";
    html += "  result += (m < 10 ? '0' : '') + m + ':';";
    html += "  result += (s < 10 ? '0' : '') + s;";
    html += "  return result;";
    html += "}";
    html += "setInterval(updateStatus, 1000);";
    html += "window.onload = updateStatus;";
    html += "</script></head><body>";
    html += "<div class='container'>";
    html += "<div class='header'>";
    html += "<h1>🏥 Healthcare RFID System</h1>";
    html += "<p>Hệ thống quản lý thẻ RFID và cân nặng y tế</p>";
    html += "</div>";
    html += "<div class='dashboard'>";
    html += "<div class='card'>";
    html += "<div class='card-title'>📊 Trạng thái hệ thống</div>";
    html += "<div id='status' class='status-display'>Đang tải...</div>";
    html += "<div class='stats'>";
    html += "<div class='stat-item'><div id='cardCount' class='stat-number'>0</div><div class='stat-label'>Thẻ hợp lệ</div></div>";
    html += "<div class='stat-item'><div id='historyCount' class='stat-number'>" + String(historyCount) + "</div><div class='stat-label'>Lịch sử</div></div>";
    html += "</div></div>";
    html += "<div class='card'>";
    html += "<div class='card-title'>⚡ Thao tác</div>";
    html += "<div class='actions'>";
    html += "<a href='/manage' class='btn btn-primary'>🏷️ Quản lý thẻ</a>";
    html += "<a href='/weight_history' class='btn btn-success'>📊 Lịch sử cân nặng</a>";
    html += "<button onclick=\"fetch('/refresh',{method:'POST'}).then(()=>alert('Đã làm mới hệ thống!'))\" class='btn btn-warning'>🔄 Làm mới</button>";
    html += "</div></div></div></div></body></html>";
    return html;
}

String getCardManagementPage() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<title>Quản lý thẻ - Healthcare RFID</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
    html += "body { font-family: 'Segoe UI', Arial, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }";
    html += ".container { max-width: 800px; margin: 0 auto; padding: 20px; }";
    html += ".header { text-align: center; color: white; margin-bottom: 30px; }";
    html += ".card { background: white; border-radius: 15px; padding: 25px; margin-bottom: 20px; box-shadow: 0 10px 30px rgba(0,0,0,0.2); }";
    html += ".card-title { font-size: 1.5rem; color: #333; margin-bottom: 20px; }";
    html += ".form-group { margin-bottom: 20px; }";
    html += ".form-label { display: block; margin-bottom: 8px; font-weight: 600; color: #333; }";
    html += ".form-input { width: 100%; padding: 12px; border: 2px solid #e1e5e9; border-radius: 8px; font-size: 1rem; transition: border-color 0.3s; }";
    html += ".form-input:focus { outline: none; border-color: #007bff; }";
    html += ".btn { padding: 12px 24px; border: none; border-radius: 8px; font-size: 1rem; font-weight: 600; cursor: pointer; transition: all 0.3s; text-decoration: none; display: inline-block; }";
    html += ".btn-primary { background: #007bff; color: white; }";
    html += ".btn-danger { background: #dc3545; color: white; }";
    html += ".btn-secondary { background: #6c757d; color: white; }";
    html += ".btn:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(0,0,0,0.2); }";
    html += ".table { width: 100%; border-collapse: collapse; margin-top: 20px; }";
    html += ".table th, .table td { padding: 12px; text-align: left; border-bottom: 1px solid #dee2e6; }";
    html += ".table th { background: #f8f9fa; font-weight: 600; }";
    html += ".table tr:hover { background: #f8f9fa; }";
    html += ".badge { padding: 4px 8px; border-radius: 4px; font-size: 0.8rem; font-weight: 600; }";
    html += ".badge-success { background: #d4edda; color: #155724; }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<div class='header'><h1>🏷️ Quản lý thẻ RFID</h1></div>";
    html += "<div class='card'>";
    html += "<div class='card-title'>➕ Thêm thẻ mới</div>";
    html += "<form method='POST' action='/add_card'>";
    html += "<div class='form-group'>";
    html += "<label class='form-label'>UID thẻ (định dạng: XX:XX:XX:XX)</label>";
    html += "<input type='text' name='uid' class='form-input' pattern='[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}' placeholder='VD: 12:34:56:78' required>";
    html += "</div>";
    html += "<button type='submit' class='btn btn-primary'>➕ Thêm thẻ</button>";
    html += "</form></div>";
    html += "<div class='card'>";
    html += "<div class='card-title'>Danh sách thẻ hợp lệ</div>";
    html += "<table class='table'>";
    html += "<thead><tr><th>UID</th><th>Trạng thái</th><th>Thao tác</th></tr></thead><tbody>";
    
    for (int i = 0; i < validCardCount; i++) {
        if (validCards[i].active) {
            html += "<tr><td><code>" + uidToString(validCards[i].uid) + "</code></td>";
            html += "<td><span class='badge badge-success'>Hoạt động</span></td>";
            html += "<td><form method='POST' action='/remove_card' style='display:inline;'>";
            html += "<input type='hidden' name='uid' value='" + uidToString(validCards[i].uid) + "'>";
            html += "<button type='submit' class='btn btn-danger' onclick='return confirm(\"Bạn có chắc muốn xóa thẻ này?\")'>Xóa</button></form></td></tr>";
        }
    }
    
    html += "</tbody></table>";
    html += "<div style='margin-top: 20px; text-align: center;'>";
    html += "<a href='/' class='btn btn-secondary'>Về trang chủ</a>";
    html += "</div></div></div></body></html>";
    return html;
}

void handleRoot() {
    sendHTMLResponse(getMainPage());
}

void handleData() {
    String json = "{";
    if (latestReading.hasData) {
        json += "\"lastCard\":\"" + uidToString(latestReading.uid) + "\",";
        json += "\"weight\":" + String(latestReading.weight, 1) + ",";
        json += "\"valid\":" + String(latestReading.isValid ? "true" : "false") + ",";
        json += "\"timestamp\":" + String(latestReading.timestamp);
    } else {
        json += "\"lastCard\":\"None\",\"weight\":0,\"valid\":false,\"timestamp\":0";
    }
    json += "}";
    server.send(200, "application/json", json);
}

void handleCards() {
    String json = "[";
    for (int i = 0; i < validCardCount; i++) {
        if (validCards[i].active) {
            if (json != "[") json += ",";
            json += "{\"uid\":\"" + uidToString(validCards[i].uid) + "\",\"active\":true}";
        }
    }
    json += "]";
    server.send(200, "application/json", json);
}

void handleCardManagementPage() {
    sendHTMLResponse(getCardManagementPage());
}

void handleAddCard() {
    if (server.hasArg("uid")) {
        String uidStr = server.arg("uid");
        if (isValidUID(uidStr)) {
            uint8_t uid[UID_SIZE];
            stringToUID(uidStr, uid);
            if (addValidCard(uid)) {
                sendHTMLResponse("<meta charset='UTF-8'><script>alert('Đã thêm thẻ thành công!'); window.location.href='/manage';</script>");
            } else {
                sendHTMLResponse("<meta charset='UTF-8'><script>alert('Thêm thẻ thất bại!'); window.location.href='/manage';</script>");
            }
        } else {
            sendHTMLResponse("<meta charset='UTF-8'><script>alert('Định dạng UID không hợp lệ!'); window.location.href='/manage';</script>");
        }
    } else {
        server.send(400, "text/plain", "Missing UID parameter");
    }
}

void handleRemoveCard() {
    if (server.hasArg("uid")) {
        String uidStr = server.arg("uid");
        if (isValidUID(uidStr)) {
            uint8_t uid[UID_SIZE];
            stringToUID(uidStr, uid);
            if (removeValidCard(uid)) {
                sendHTMLResponse("<meta charset='UTF-8'><script>alert('Đã xóa thẻ thành công!'); window.location.href='/manage';</script>");
            } else {
                sendHTMLResponse("<meta charset='UTF-8'><script>alert('Xóa thẻ thất bại!'); window.location.href='/manage';</script>");
            }
        } else {
            sendHTMLResponse("<meta charset='UTF-8'><script>alert('Định dạng UID không hợp lệ!'); window.location.href='/manage';</script>");
        }
    } else {
        server.send(400, "text/plain", "Missing UID parameter");
    }
}

void handleRefresh() {
    sendSystemResetToSTM32();
    server.send(200, "text/plain", "System refresh command sent to STM32");
}

void handleWeightHistory() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<title>Lịch sử cân nặng - Healthcare RFID</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
    html += "body { font-family: 'Segoe UI', Arial, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }";
    html += ".container { max-width: 1000px; margin: 0 auto; padding: 20px; }";
    html += ".header { text-align: center; color: white; margin-bottom: 30px; }";
    html += ".card { background: white; border-radius: 15px; padding: 25px; box-shadow: 0 10px 30px rgba(0,0,0,0.2); }";
    html += ".card-title { font-size: 1.5rem; color: #333; margin-bottom: 20px; }";
    html += ".table { width: 100%; border-collapse: collapse; margin-top: 20px; }";
    html += ".table th, .table td { padding: 12px; text-align: left; border-bottom: 1px solid #dee2e6; }";
    html += ".table th { background: #f8f9fa; font-weight: 600; }";
    html += ".table tr:hover { background: #f8f9fa; }";
    html += ".badge { padding: 4px 8px; border-radius: 4px; font-size: 0.8rem; font-weight: 600; }";
    html += ".badge-success { background: #d4edda; color: #155724; }";
    html += ".badge-danger { background: #f8d7da; color: #721c24; }";
    html += ".btn { padding: 12px 24px; border: none; border-radius: 8px; font-size: 1rem; font-weight: 600; cursor: pointer; transition: all 0.3s; text-decoration: none; display: inline-block; background: #007bff; color: white; }";
    html += ".btn:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(0,0,0,0.2); }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<div class='header'><h1>📊 Lịch sử cân nặng</h1></div>";
    html += "<div class='card'>";
    html += "<div class='card-title'>Dữ liệu cân nặng gần đây</div>";
    html += "<table class='table'><tr><th>Thời gian</th><th>UID thẻ</th><th>Cân nặng</th><th>Hợp lệ</th></tr>";
    
    for (int i = 0; i < historyCount; i++) {
        int index = (historyIndex - 1 - i + MAX_WEIGHT_HISTORY) % MAX_WEIGHT_HISTORY;
        if (weightHistory[index].valid) {
            String timeStr = formatUptime(weightHistory[index].timestamp);
            
            html += "<tr><td>" + timeStr + "</td>";
            html += "<td><code>" + uidToString(weightHistory[index].uid) + "</code></td>";
            html += "<td><strong>" + String(weightHistory[index].weight, 1) + "g</strong></td>";
            if (weightHistory[index].isValidCard) {
                html += "<td><span class='badge badge-success'>Hợp lệ</span></td></tr>";
            } else {
                html += "<td><span class='badge badge-danger'>Không hợp lệ</span></td></tr>";
            }
        }
    }
    
    html += "</table><br>";
    html += "<a href='/' class='btn'>🏠 Về trang chính</a>";
    html += "</div></div></body></html>";
    sendHTMLResponse(html);
}


