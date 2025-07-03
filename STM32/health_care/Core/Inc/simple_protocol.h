/**
 * Simple UART protocol for STM32-ESP32 communication
 */

#ifndef __SIMPLE_PROTOCOL_H
#define __SIMPLE_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Protocol constants
#define PROTOCOL_START_BYTE     0xAA
#define PROTOCOL_END_BYTE       0x55
#define MSG_TYPE_CARD_REGISTERED    0x01
#define MSG_TYPE_CARD_UNREGISTERED  0x02
#define MSG_TYPE_REGISTER_CARD      0x03
#define MSG_TYPE_VALID_CARDS        0x04
#define MSG_TYPE_ACK                0x05
#define MSG_TYPE_NACK               0x06
#define UID_SIZE                4
#define CARD_STATUS_VALID       0x01
#define CARD_STATUS_INVALID     0x00

// External variables
extern uint8_t rx_byte; // For interrupt receive

// Function declarations
void SimpleProtocol_Init(void);
void SimpleProtocol_ProcessReceivedData(void);
void SimpleProtocol_SendRegisteredCard(uint8_t* uid, float weight);
void SimpleProtocol_SendUnregisteredCard(uint8_t* uid);
void SimpleProtocol_ProcessCardDetection(uint8_t* uid, float weight);
void SimpleProtocol_SendACK(uint8_t msgType);
void SimpleProtocol_StartRxInterrupt(void);
void SimpleProtocol_UART_RxCpltCallback(void);
void SimpleProtocol_PushReceivedByte(uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* __SIMPLE_PROTOCOL_H */
