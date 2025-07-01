/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : rc522.c
  * @brief          : RC522 RFID module implementation
  ******************************************************************************
  */
/* USER CODE END Header */

#include "rc522.h"
#include <string.h>
#include <stdio.h>

// Disable debug output to keep UART1 free for ESP32 communication
#define RC522_DEBUG_ENABLED 0

#if RC522_DEBUG_ENABLED
#define RC522_Debug_Print(data, len) HAL_UART_Transmit(&huart1, (uint8_t*)(data), (len), 1000)
#else
#define RC522_Debug_Print(data, len) // Debug disabled
#endif

extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart1;

/**
 * @brief Write data to RC522 register
 */
void RC522_WriteRegister(uint8_t addr, uint8_t val) {
    uint8_t tx_data[2];
    tx_data[0] = (addr << 1) & 0x7E;  // Address shifted and write bit
    tx_data[1] = val;
    
    HAL_GPIO_WritePin(RC522_CS_GPIO_Port, RC522_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, tx_data, 2, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(RC522_CS_GPIO_Port, RC522_CS_Pin, GPIO_PIN_SET);
}

/**
 * @brief Read data from RC522 register
 */
uint8_t RC522_ReadRegister(uint8_t addr) {
    uint8_t tx_data[2];
    uint8_t rx_data[2];
    
    tx_data[0] = ((addr << 1) & 0x7E) | 0x80;  // Address shifted and read bit
    tx_data[1] = 0x00;
    
    HAL_GPIO_WritePin(RC522_CS_GPIO_Port, RC522_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(&hspi1, tx_data, rx_data, 2, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(RC522_CS_GPIO_Port, RC522_CS_Pin, GPIO_PIN_SET);
    
    return rx_data[1];
}

/**
 * @brief Set bit mask in register
 */
void RC522_SetBitMask(uint8_t addr, uint8_t mask) {
    uint8_t val = RC522_ReadRegister(addr);
    RC522_WriteRegister(addr, val | mask);
}

/**
 * @brief Clear bit mask in register
 */
void RC522_ClearBitMask(uint8_t addr, uint8_t mask) {
    uint8_t val = RC522_ReadRegister(addr);
    RC522_WriteRegister(addr, val & (~mask));
}

/**
 * @brief Reset RC522 with improved sequence
 */
void RC522_Reset(void) {
    // Hardware reset if RST pin is defined
    #ifdef RC522_RST_Pin
    HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);  // Hold reset for 10ms
    HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(50);  // Wait for RC522 to start up
    #endif
    
    // Software reset command
    RC522_WriteRegister(RC522_REG_COMMAND, RC522_CMD_SOFT_RESET);
    HAL_Delay(RC522_RESET_DELAY_MS);
    
    // Wait for reset to complete - check if RC522 is ready
    for (int i = 0; i < 10; i++) {
        uint8_t status = RC522_ReadRegister(RC522_REG_COMMAND);
        if (status == RC522_CMD_IDLE) {
            break;  // Reset completed
        }
        HAL_Delay(10);
    }
}

/**
 * @brief Turn on antenna
 */
void RC522_AntennaOn(void) {
    uint8_t val = RC522_ReadRegister(RC522_REG_TX_CONTROL);
    if (!(val & 0x03)) {
        RC522_SetBitMask(RC522_REG_TX_CONTROL, 0x03);
    }
}

/**
 * @brief Turn off antenna
 */
void RC522_AntennaOff(void) {
    RC522_ClearBitMask(RC522_REG_TX_CONTROL, 0x03);
}

/**
 * @brief Initialize RC522 with enhanced sequence
 */
void RC522_Init(void) {
    // Initialize CS pin to high (not selected)
    HAL_GPIO_WritePin(RC522_CS_GPIO_Port, RC522_CS_Pin, GPIO_PIN_SET);
    
    // Initialize RST pin if defined
    #ifdef RC522_RST_Pin
    HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_SET);
    #endif
    
    // Wait for power stabilization
    HAL_Delay(100);
    
    // Reset RC522
    RC522_Reset();
    
    // Additional delay after reset
    HAL_Delay(100);
    
    // Configure timer
    RC522_WriteRegister(RC522_REG_T_MODE, 0x8D);
    RC522_WriteRegister(RC522_REG_T_PRESCALER, 0x3E);
    RC522_WriteRegister(RC522_REG_T_RELOAD_L, 30);
    RC522_WriteRegister(RC522_REG_T_RELOAD_H, 0);
    
    // Configure RF
    RC522_WriteRegister(RC522_REG_TX_AUTO, 0x40);
    RC522_WriteRegister(RC522_REG_MODE, 0x3D);
    
    RC522_AntennaOn();
}

/**
 * @brief Communicate with card
 */
RC522_Status RC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, 
                          uint8_t *backData, uint16_t *backLen) {
    RC522_Status status = RC522_ERROR;
    uint8_t irqEn = 0x77;
    uint8_t waitIRq = 0x30;
    uint8_t lastBits;
    uint8_t n;
    uint16_t i;
    
    switch (command) {
        case RC522_CMD_MF_AUTHENT:
            irqEn = 0x12;
            waitIRq = 0x10;
            break;
        case RC522_CMD_TRANSCEIVE:
            irqEn = 0x77;
            waitIRq = 0x30;
            break;
        default:
            break;
    }
    
    RC522_WriteRegister(RC522_REG_COMM_IEN, irqEn | 0x80);
    RC522_ClearBitMask(RC522_REG_COMM_IRQ, 0x80);
    RC522_SetBitMask(RC522_REG_FIFO_LEVEL, 0x80);
    
    RC522_WriteRegister(RC522_REG_COMMAND, RC522_CMD_IDLE);
    
    // Write data to FIFO
    for (i = 0; i < sendLen; i++) {
        RC522_WriteRegister(RC522_REG_FIFO_DATA, sendData[i]);
    }
    
    // Execute command
    RC522_WriteRegister(RC522_REG_COMMAND, command);
    if (command == RC522_CMD_TRANSCEIVE) {
        RC522_SetBitMask(RC522_REG_BIT_FRAMING, 0x80);
    }
    
    // Wait for completion
    i = 2000;
    do {
        n = RC522_ReadRegister(RC522_REG_COMM_IRQ);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & waitIRq));
    
    RC522_ClearBitMask(RC522_REG_BIT_FRAMING, 0x80);
    
    if (i != 0) {
        if (!(RC522_ReadRegister(RC522_REG_ERROR) & 0x1B)) {
            status = RC522_OK;
            
            if (n & irqEn & 0x01) {
                status = RC522_NOTAG;
            }
            
            if (command == RC522_CMD_TRANSCEIVE) {
                n = RC522_ReadRegister(RC522_REG_FIFO_LEVEL);
                lastBits = RC522_ReadRegister(RC522_REG_CONTROL) & 0x07;
                if (lastBits) {
                    *backLen = (n - 1) * 8 + lastBits;
                } else {
                    *backLen = n * 8;
                }
                
                if (n == 0) {
                    n = 1;
                }
                if (n > 16) {
                    n = 16;
                }
                
                // Read received data from FIFO
                for (i = 0; i < n; i++) {
                    backData[i] = RC522_ReadRegister(RC522_REG_FIFO_DATA);
                }
            }
        } else {
            status = RC522_ERROR;
        }
    }
    
    return status;
}

/**
 * @brief Request card
 */
RC522_Status RC522_Request(uint8_t reqMode, uint8_t *TagType) {
    RC522_Status status;
    uint16_t backBits;
    
    RC522_WriteRegister(RC522_REG_BIT_FRAMING, 0x07);
    
    TagType[0] = reqMode;
    status = RC522_ToCard(RC522_CMD_TRANSCEIVE, TagType, 1, TagType, &backBits);
    
    if ((status != RC522_OK) || (backBits != 0x10)) {
        status = RC522_ERROR;
    }
    
    return status;
}

/**
 * @brief Anticollision
 */
RC522_Status RC522_Anticoll(uint8_t *serNum) {
    RC522_Status status;
    uint8_t i;
    uint8_t serNumCheck = 0;
    uint16_t unLen;
    
    RC522_WriteRegister(RC522_REG_BIT_FRAMING, 0x00);
    
    serNum[0] = PICC_CMD_SEL_CL1;
    serNum[1] = 0x20;
    
    status = RC522_ToCard(RC522_CMD_TRANSCEIVE, serNum, 2, serNum, &unLen);
    
    if (status == RC522_OK) {
        // Check serial number
        for (i = 0; i < 4; i++) {
            serNumCheck ^= serNum[i];
        }
        if (serNumCheck != serNum[i]) {
            status = RC522_ERROR;
        }
    }
    
    return status;
}

/**
 * @brief Select card
 */
RC522_Status RC522_SelectTag(uint8_t *serNum) {
    RC522_Status status;
    uint8_t i;
    uint8_t buffer[9];
    uint16_t backBits;
    
    buffer[0] = PICC_CMD_SEL_CL1;
    buffer[1] = 0x70;
    
    for (i = 0; i < 5; i++) {
        buffer[i + 2] = *(serNum + i);
    }
    
    status = RC522_ToCard(RC522_CMD_TRANSCEIVE, buffer, 7, buffer, &backBits);
    
    if ((status == RC522_OK) && (backBits == 0x18)) {
        status = RC522_OK;
    } else {
        status = RC522_ERROR;
    }
    
    return status;
}

/**
 * @brief Read UID from card
 */
RC522_Status RC522_ReadUID(uint8_t *uid) {
    RC522_Status status;
    uint8_t TagType[2];
    
    status = RC522_Request(PICC_CMD_REQA, TagType);
    if (status == RC522_OK) {
        status = RC522_Anticoll(uid);
    }
    
    return status;
}

/**
 * @brief Comprehensive RC522 diagnostics - DISABLED
 */
void RC522_Diagnostics(void) {
    // Diagnostics disabled to keep UART1 free for ESP32 communication
    // Enable RC522_DEBUG_ENABLED to see diagnostics via separate UART
}

/**
 * @brief Test different SPI settings - DISABLED  
 */
void RC522_TestSPISettings(void) {
    // SPI settings test disabled to keep UART1 free for ESP32 communication
    // Enable RC522_DEBUG_ENABLED to see test results via separate UART
}
