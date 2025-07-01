/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : config.h
  * @brief          : Configuration file for healthcare RFID system
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __CONFIG_H
#define __CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

// System timing configuration
#define CARD_CHECK_INTERVAL_MS      500     // RFID card check interval in milliseconds
#define WEIGHT_CHECK_INTERVAL_MS    1000    // Weight measurement interval in milliseconds
#define SYSTEM_INIT_DELAY_MS        2000    // System initialization delay

// HX711 configuration
#define HX711_DEFAULT_SCALE         420.0f  // Default scale factor (needs calibration)
#define HX711_TARE_SAMPLES          10      // Number of samples for taring
#define HX711_STABLE_DELAY_MS       50      // Delay between readings for stability

// RC522 configuration
#define RC522_CARD_DETECTION_DELAY_MS   1000    // Delay after card detection to avoid duplicates
#define RC522_RESET_DELAY_MS            50      // Reset delay for RC522

// ESP32 communication configuration
#define ESP32_UART_TIMEOUT_MS       1000    // UART transmission timeout
#define ESP32_MAX_RETRIES           3       // Maximum retries for failed transmissions

// Message protocol configuration
#define MSG_START_BYTE              0xAA
#define MSG_END_BYTE                0x55
#define MSG_TIMEOUT_MS              5000    // Message timeout

// System status indicators (optional LED pins)
#define ENABLE_STATUS_LEDS          0       // Set to 1 to enable LED indicators

#if ENABLE_STATUS_LEDS
#define LED_VALID_CARD_PIN          GPIO_PIN_13
#define LED_VALID_CARD_PORT         GPIOG
#define LED_INVALID_CARD_PIN        GPIO_PIN_14
#define LED_INVALID_CARD_PORT       GPIOG
#define LED_SYSTEM_READY_PIN        GPIO_PIN_15
#define LED_SYSTEM_READY_PORT       GPIOG
#endif

// Debug configuration
#define ENABLE_DEBUG_UART           0       // Set to 0 to free UART1 for ESP32 communication
#define DEBUG_USE_SEPARATE_UART     0       // Set to 1 to use separate UART for debug
#define DEBUG_UART_HANDLE           huart1  // UART handle for debug

// If DEBUG_USE_SEPARATE_UART is 1, you need to:
// 1. Configure UART2 in STM32CubeMX
// 2. Change DEBUG_UART_HANDLE to huart2
// 3. Connect debug probe to UART2 pins instead of UART1

// Note: When ENABLE_DEBUG_UART=1 and DEBUG_USE_SEPARATE_UART=0,
// debug messages will be sent via same UART as ESP32 communication.
// This may interfere with ESP32 protocol. Use only for initial testing.

// Weight sensor configuration
#define WEIGHT_FILTER_ENABLED       1       // Enable weight filtering
#define WEIGHT_FILTER_SAMPLES       5       // Number of samples for averaging
#define WEIGHT_THRESHOLD_GRAMS      1.0f    // Minimum weight change to register

// Card validation configuration
#define CARD_VALIDATION_ENABLED     1       // Enable card validation against list
#define SEND_INVALID_CARDS          1       // Send invalid card data to ESP32

#ifdef __cplusplus
}
#endif

#endif /* __CONFIG_H */
