/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : rc522.h
  * @brief          : Header for RC522 RFID module
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __RC522_H
#define __RC522_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "main.h"

// RC522 Commands
#define RC522_CMD_IDLE          0x00
#define RC522_CMD_MEM           0x01
#define RC522_CMD_GENERATE_RANDOM_ID    0x02
#define RC522_CMD_CALC_CRC      0x03
#define RC522_CMD_TRANSMIT      0x04
#define RC522_CMD_NO_CMD_CHANGE 0x07
#define RC522_CMD_RECEIVE       0x08
#define RC522_CMD_TRANSCEIVE    0x0C
#define RC522_CMD_MF_AUTHENT    0x0E
#define RC522_CMD_SOFT_RESET    0x0F

// RC522 Registers
#define RC522_REG_COMMAND       0x01
#define RC522_REG_COMM_IEN      0x02
#define RC522_REG_DIV_IEN       0x03
#define RC522_REG_COMM_IRQ      0x04
#define RC522_REG_DIV_IRQ       0x05
#define RC522_REG_ERROR         0x06
#define RC522_REG_STATUS1       0x07
#define RC522_REG_STATUS2       0x08
#define RC522_REG_FIFO_DATA     0x09
#define RC522_REG_FIFO_LEVEL    0x0A
#define RC522_REG_WATER_LEVEL   0x0B
#define RC522_REG_CONTROL       0x0C
#define RC522_REG_BIT_FRAMING   0x0D
#define RC522_REG_COLL          0x0E
#define RC522_REG_MODE          0x11
#define RC522_REG_TX_MODE       0x12
#define RC522_REG_RX_MODE       0x13
#define RC522_REG_TX_CONTROL    0x14
#define RC522_REG_TX_AUTO       0x15
#define RC522_REG_TX_SEL        0x16
#define RC522_REG_RX_SEL        0x17
#define RC522_REG_RX_THRESHOLD  0x18
#define RC522_REG_DEMOD         0x19
#define RC522_REG_MF_TX         0x1C
#define RC522_REG_MF_RX         0x1D
#define RC522_REG_SERIALSPEED   0x1F
#define RC522_REG_CRC_RESULT_M  0x21
#define RC522_REG_CRC_RESULT_L  0x22
#define RC522_REG_MOD_WIDTH     0x24
#define RC522_REG_RF_CFG        0x26
#define RC522_REG_GS_N          0x27
#define RC522_REG_CW_GS_P       0x28
#define RC522_REG_MOD_GS_P      0x29
#define RC522_REG_T_MODE        0x2A
#define RC522_REG_T_PRESCALER   0x2B
#define RC522_REG_T_RELOAD_H    0x2C
#define RC522_REG_T_RELOAD_L    0x2D
#define RC522_REG_T_COUNTER_VAL_H 0x2E
#define RC522_REG_T_COUNTER_VAL_L 0x2F
#define RC522_REG_VERSION       0x37

// Diagnostic function
void RC522_Diagnostics(void);
void RC522_TestSPISettings(void);

// PICC Commands
#define PICC_CMD_REQA           0x26
#define PICC_CMD_WUPA           0x52
#define PICC_CMD_CT             0x88
#define PICC_CMD_SEL_CL1        0x93
#define PICC_CMD_SEL_CL2        0x95
#define PICC_CMD_SEL_CL3        0x97
#define PICC_CMD_HLTA           0x50

// Status codes
typedef enum {
    RC522_OK,
    RC522_NOTAG,
    RC522_ERROR
} RC522_Status;

// Function prototypes
void RC522_Init(void);
void RC522_Reset(void);
uint8_t RC522_ReadRegister(uint8_t addr);
void RC522_WriteRegister(uint8_t addr, uint8_t val);
void RC522_SetBitMask(uint8_t addr, uint8_t mask);
void RC522_ClearBitMask(uint8_t addr, uint8_t mask);
void RC522_AntennaOn(void);
void RC522_AntennaOff(void);
RC522_Status RC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint16_t *backLen);
RC522_Status RC522_Request(uint8_t reqMode, uint8_t *TagType);
RC522_Status RC522_Anticoll(uint8_t *serNum);
RC522_Status RC522_SelectTag(uint8_t *serNum);
RC522_Status RC522_ReadUID(uint8_t *uid);

#ifdef __cplusplus
}
#endif

#endif /* __RC522_H */
