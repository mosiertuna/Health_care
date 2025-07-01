/**
 * Simple UART protocol implementation
 */

#include "simple_protocol.h"
#include "main.h"
#include <string.h>

#define MAX_REGISTERED_CARDS 20
#define RX_BUFFER_SIZE 32

extern UART_HandleTypeDef huart1;  // Back to UART1 but disable debug output

static uint8_t registered_cards[MAX_REGISTERED_CARDS][4];
static uint8_t registered_count = 0;
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static uint8_t rx_index = 0;

static uint8_t IsCardRegistered(uint8_t* uid) {
    for (uint8_t i = 0; i < registered_count; i++) {
        if (memcmp(registered_cards[i], uid, 4) == 0) {
            return 1;
        }
    }
    return 0;
}

static uint8_t RegisterCard(uint8_t* uid) {
    if (registered_count >= MAX_REGISTERED_CARDS) return 0;
    if (IsCardRegistered(uid)) return 1;
    
    memcpy(registered_cards[registered_count], uid, 4);
    registered_count++;
    return 1;
}

void SimpleProtocol_SendRegisteredCard(uint8_t* uid, float weight) {
    uint8_t buffer[11];
    buffer[0] = PROTOCOL_START_BYTE;
    buffer[1] = MSG_TYPE_CARD_REGISTERED;
    memcpy(&buffer[2], uid, 4);
    memcpy(&buffer[6], &weight, 4);
    buffer[10] = PROTOCOL_END_BYTE;
    
    HAL_UART_Transmit(&huart1, buffer, 11, 1000);
}

void SimpleProtocol_SendUnregisteredCard(uint8_t* uid) {
    uint8_t buffer[7];
    buffer[0] = PROTOCOL_START_BYTE;
    buffer[1] = MSG_TYPE_CARD_UNREGISTERED;
    memcpy(&buffer[2], uid, 4);
    buffer[6] = PROTOCOL_END_BYTE;
    
    HAL_UART_Transmit(&huart1, buffer, 7, 1000);
}

void SimpleProtocol_ProcessCardDetection(uint8_t* uid, float weight) {
    if (IsCardRegistered(uid)) {
        SimpleProtocol_SendRegisteredCard(uid, weight);
    } else {
        SimpleProtocol_SendUnregisteredCard(uid);
    }
}

/**
 * @brief Process received data from ESP32
 */
void SimpleProtocol_ProcessReceivedData(void) {
    static uint32_t last_timeout_check = 0;
    uint32_t current_time = HAL_GetTick();
    
    // Process all available bytes using UART1 (debug disabled)
    uint8_t received_byte;
    while (HAL_UART_Receive(&huart1, &received_byte, 1, 0) == HAL_OK) {
        rx_buffer[rx_index] = received_byte;
        rx_index++;
        last_timeout_check = current_time;
        
        // Disable all debug output to free UART1 for ESP32 communication
        // Debug temporarily disabled
        // if (rx_index == 1) {
        //     Debug_Printf("STM32_RX: Start=0x%02X ", received_byte);
        // } else if (rx_index == 2) {
        //     Debug_Printf("Type=0x%02X ", received_byte);
        // } else if (rx_index <= 7) {
        //     Debug_Printf("0x%02X ", received_byte);
        // }
        // if (rx_index == 7) {
        //     Debug_Printf("\r\n");
        // }
        
        // Check for start byte
        if (rx_index == 1 && received_byte != PROTOCOL_START_BYTE) {
            rx_index = 0;
            continue;
        }
        
        // We need at least 2 bytes to determine message type
        if (rx_index >= 2) {
            uint8_t msg_type = rx_buffer[1];
            uint8_t expected_length = 0;
            
            switch (msg_type) {
                case MSG_TYPE_REGISTER_CARD:
                    expected_length = 7; // AA 03 UID[4] 55
                    break;
                case MSG_TYPE_VALID_CARDS:
                    // Variable length message: AA 04 LENGTH DATA... CHECKSUM 55
                    if (rx_index >= 3) {
                        uint8_t data_length = rx_buffer[2];
                        expected_length = 6 + data_length; // AA 04 LENGTH DATA[data_length] CHECKSUM 55
                    }
                    break;
                default:
                    // Unknown message type, reset and try again
                    rx_index = 0;
                    continue;
            }
            
            // Check if we have complete message
            if (expected_length > 0 && rx_index >= expected_length) {
                // Verify end byte
                if (rx_buffer[expected_length - 1] == PROTOCOL_END_BYTE) {
                    // Process the message
                    if (msg_type == MSG_TYPE_REGISTER_CARD) {
                        // Extract UID from message: AA 03 UID[4] 55
                        uint8_t* uid = &rx_buffer[2];
                        
                        // Register the new card
                        if (RegisterCard(uid)) {
                            // Debug disabled to free UART1 
                            // Debug_Printf("STM32: Card registered: %02X:%02X:%02X:%02X (Total: %d)\r\n", 
                            //            uid[0], uid[1], uid[2], uid[3], registered_count);
                        }
                    } else if (msg_type == MSG_TYPE_VALID_CARDS) {
                        // Process valid cards list from ESP32
                        uint8_t data_length = rx_buffer[2];
                        uint8_t num_cards = data_length / 4; // Each card is 4 bytes
                        
                        // Clear existing registered cards and load new ones
                        registered_count = 0;
                        
                        for (uint8_t i = 0; i < num_cards && i < MAX_REGISTERED_CARDS; i++) {
                            uint8_t* uid = &rx_buffer[3 + (i * 4)];
                            memcpy(registered_cards[registered_count], uid, 4);
                            registered_count++;
                        }
                        
                        // Debug disabled to free UART1
                        // Debug_Printf("STM32: Loaded %d cards from ESP32\r\n", registered_count);
                    }
                }
                
                // Reset buffer after processing (successful or not)
                rx_index = 0;
                memset(rx_buffer, 0, sizeof(rx_buffer));
            }
        }
        
        // Prevent buffer overflow
        if (rx_index >= sizeof(rx_buffer)) {
            rx_index = 0;
            memset(rx_buffer, 0, sizeof(rx_buffer));
        }
    }
    
    // Timeout check: if we have partial data but no new bytes for too long, reset
    if (rx_index > 0 && (current_time - last_timeout_check) > 200) {
        rx_index = 0;
        memset(rx_buffer, 0, sizeof(rx_buffer));
    }
}

/**
 * @brief Initialize protocol with some default registered cards
 */
void SimpleProtocol_Init(void) {
    // Add some default registered cards for testing
    uint8_t default_card1[4] = {0x12, 0x34, 0x56, 0x78};
    uint8_t default_card2[4] = {0xAB, 0xCD, 0xEF, 0x01};
    
    RegisterCard(default_card1);
    RegisterCard(default_card2);
    
    // Send test message to ESP32 to verify connection
    HAL_Delay(2000); // Wait for ESP32 to initialize
    
    // Send a test unregistered card message to verify UART works
    uint8_t test_uid[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    SimpleProtocol_SendUnregisteredCard(test_uid);
    
    // Debug disabled to free UART1
    // Debug_Printf("STM32: Protocol initialized with %d default cards\r\n", registered_count);
}

/**
 * @brief Debug function to print all registered cards
 */
void SimpleProtocol_PrintRegisteredCards(void) {
    Debug_Printf("STM32: === REGISTERED CARDS (%d) ===\r\n", registered_count);
    for (uint8_t i = 0; i < registered_count; i++) {
        Debug_Printf("STM32: Card %d: %02X:%02X:%02X:%02X\r\n", i+1,
                     registered_cards[i][0], registered_cards[i][1], 
                     registered_cards[i][2], registered_cards[i][3]);
    }
    Debug_Printf("STM32: ========================\r\n");
}
