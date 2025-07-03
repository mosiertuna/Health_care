/**
 * Simple UART protocol implementation
 */

#include "simple_protocol.h"
#include "main.h"
#include <string.h>

#define MAX_REGISTERED_CARDS 20
#define RX_BUFFER_SIZE 32

extern UART_HandleTypeDef huart1;

static uint8_t registered_cards[MAX_REGISTERED_CARDS][4];
static uint8_t registered_count = 0;
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static uint8_t rx_index = 0;
uint8_t rx_byte; // For interrupt receive, now exported

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
    buffer[0] = PROTOCOL_START_BYTE;         // 0xAA
    buffer[1] = MSG_TYPE_CARD_DETECTED;      // 0x01 (use unified type)
    memcpy(&buffer[2], uid, 4);              // 4 bytes UID
    memcpy(&buffer[6], &weight, 4);          // 4 bytes weight (float)
    buffer[10] = PROTOCOL_END_BYTE;          // 0x55
    
    HAL_UART_Transmit(&huart1, buffer, 11, 1000);
}

void SimpleProtocol_SendUnregisteredCard(uint8_t* uid, float weight) {
    uint8_t buffer[11];
    buffer[0] = PROTOCOL_START_BYTE;         // 0xAA
    buffer[1] = MSG_TYPE_CARD_DETECTED;      // 0x01 (use unified type)
    memcpy(&buffer[2], uid, 4);              // 4 bytes UID
    memcpy(&buffer[6], &weight, 4);          // 4 bytes weight (float)
    buffer[10] = PROTOCOL_END_BYTE;          // 0x55
    
    HAL_UART_Transmit(&huart1, buffer, 11, 1000);
}

void SimpleProtocol_SendACK(uint8_t msgType) {
    uint8_t buffer[5];
    buffer[0] = PROTOCOL_START_BYTE;
    buffer[1] = MSG_TYPE_ACK;
    buffer[2] = msgType; // Type of message being ACKed
    buffer[3] = PROTOCOL_END_BYTE;
    
    HAL_UART_Transmit(&huart1, buffer, 4, 1000);
}

void SimpleProtocol_ProcessCardDetection(uint8_t* uid, float weight) {
    // Send all cards with same format - ESP32 will validate
    SimpleProtocol_SendRegisteredCard(uid, weight);
}

void SimpleProtocol_PushReceivedByte(uint8_t byte) {
    rx_byte = byte;
    SimpleProtocol_UART_RxCpltCallback();
}

void SimpleProtocol_StartRxInterrupt(void) {
    // Reset UART state and buffer
    HAL_UART_AbortReceive(&huart1);
    rx_index = 0;
    memset(rx_buffer, 0, sizeof(rx_buffer));
    
    // Start receiving first byte
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

void SimpleProtocol_UART_RxCpltCallback(void) {
    // Process received byte
    if (rx_index == 0 && rx_byte != PROTOCOL_START_BYTE) {
        // Invalid start byte, wait for next byte
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
        return;
    }

    // Add byte to buffer
    rx_buffer[rx_index++] = rx_byte;

    // Start receiving next byte
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

    // Check for complete message
    if (rx_index >= 2) {
        uint8_t msg_type = rx_buffer[1];
        uint8_t expected_length = 0;

        switch (msg_type) {
            case MSG_TYPE_REGISTER_CARD:
                expected_length = 7; // AA 03 UID[4] 55
                break;
            case MSG_TYPE_VALID_CARDS:
                if (rx_index >= 3) {
                    uint8_t data_length = rx_buffer[2];
                    expected_length = 6 + data_length; // AA 04 LENGTH DATA[data_length] CHECKSUM 55
                }
                break;
            default:
                // Unknown message type, reset
                rx_index = 0;
                return;
        }

        // Process complete message
        if (expected_length > 0 && rx_index >= expected_length) {
            if (rx_buffer[expected_length - 1] == PROTOCOL_END_BYTE) {
                // Process valid message
                if (msg_type == MSG_TYPE_REGISTER_CARD) {
                    uint8_t* uid = &rx_buffer[2];
                    if (RegisterCard(uid)) {
                        SimpleProtocol_SendACK(MSG_TYPE_REGISTER_CARD);
                    }
                } else if (msg_type == MSG_TYPE_VALID_CARDS) {
                    uint8_t data_length = rx_buffer[2];
                    uint8_t num_cards = data_length / 4;
                    
                    registered_count = 0;
                    for (uint8_t i = 0; i < num_cards && i < MAX_REGISTERED_CARDS; i++) {
                        uint8_t* uid = &rx_buffer[3 + (i * 4)];
                        memcpy(registered_cards[registered_count], uid, 4);
                        registered_count++;
                    }
                    SimpleProtocol_SendACK(MSG_TYPE_VALID_CARDS);
                }
            }
            // Reset buffer after processing
            rx_index = 0;
        }
    }

    // Prevent buffer overflow
    if (rx_index >= RX_BUFFER_SIZE) {
        rx_index = 0;
        memset(rx_buffer, 0, sizeof(rx_buffer));
    }
}

void SimpleProtocol_Init(void) {
    // Add some default registered cards for testing
    uint8_t default_card1[4] = {0x12, 0x34, 0x56, 0x78};
    uint8_t default_card2[4] = {0xAB, 0xCD, 0xEF, 0x01};
    
    RegisterCard(default_card1);
    RegisterCard(default_card2);
    
    // Initialize UART receive interrupt
    HAL_Delay(2000); // Wait for ESP32 to initialize
    SimpleProtocol_StartRxInterrupt();
    
    // Send a test unregistered card message
    uint8_t test_uid[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    SimpleProtocol_SendUnregisteredCard(test_uid);
}
