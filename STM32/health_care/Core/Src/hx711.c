/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : hx711.c
  * @brief          : HX711 weight sensor implementation
  ******************************************************************************
  */
/* USER CODE END Header */

#include "hx711.h"

// Global variables
float hx711_scale = 1.0f;
int32_t hx711_offset = 0;

/**
 * @brief Initialize HX711
 */
void HX711_Init(void) {
    // Set SCK low
    HAL_GPIO_WritePin(HX711_SCK_GPIO_Port, HX711_SCK_Pin, GPIO_PIN_RESET);
    HAL_Delay(100);
}

/**
 * @brief Check if HX711 is ready for reading
 */
uint8_t HX711_IsReady(void) {
    return (HAL_GPIO_ReadPin(HX711_DT_GPIO_Port, HX711_DT_Pin) == GPIO_PIN_RESET);
}

/**
 * @brief Read raw data from HX711
 */
uint32_t HX711_ReadRaw(uint8_t channel) {
    uint32_t data = 0;
    uint8_t i;
    uint8_t pulses;
    
    // Wait for HX711 to be ready
    while (!HX711_IsReady()) {
        HAL_Delay(1);
    }
    
    // Read 24 bits
    for (i = 0; i < 24; i++) {
        HAL_GPIO_WritePin(HX711_SCK_GPIO_Port, HX711_SCK_Pin, GPIO_PIN_SET);
        HAL_Delay_us(1);
        data <<= 1;
        if (HAL_GPIO_ReadPin(HX711_DT_GPIO_Port, HX711_DT_Pin) == GPIO_PIN_SET) {
            data |= 1;
        }
        HAL_GPIO_WritePin(HX711_SCK_GPIO_Port, HX711_SCK_Pin, GPIO_PIN_RESET);
        HAL_Delay_us(1);
    }
    
    // Set channel and gain for next reading
    switch (channel) {
        case HX711_CHANNEL_A_GAIN_128:
            pulses = 1;
            break;
        case HX711_CHANNEL_B_GAIN_32:
            pulses = 2;
            break;
        case HX711_CHANNEL_A_GAIN_64:
            pulses = 3;
            break;
        default:
            pulses = 1;
            break;
    }
    
    for (i = 0; i < pulses; i++) {
        HAL_GPIO_WritePin(HX711_SCK_GPIO_Port, HX711_SCK_Pin, GPIO_PIN_SET);
        HAL_Delay_us(1);
        HAL_GPIO_WritePin(HX711_SCK_GPIO_Port, HX711_SCK_Pin, GPIO_PIN_RESET);
        HAL_Delay_us(1);
    }
    
    // Convert to signed 24-bit value
    if (data & 0x800000) {
        data |= 0xFF000000;
    }
    
    return data;
}

/**
 * @brief Read value with offset compensation
 */
int32_t HX711_ReadValue(uint8_t channel) {
    return (int32_t)HX711_ReadRaw(channel) - hx711_offset;
}

/**
 * @brief Read weight in grams
 */
float HX711_ReadWeight(void) {
    int32_t value = HX711_ReadValue(HX711_CHANNEL_A_GAIN_128);
    return (float)value / hx711_scale;
}

/**
 * @brief Set scale factor
 */
void HX711_SetScale(float scale) {
    hx711_scale = scale;
}

/**
 * @brief Tare the scale (set current reading as zero)
 */
void HX711_Tare(void) {
    uint32_t sum = 0;
    uint8_t i;
    
    // Take average of multiple readings
    for (i = 0; i < HX711_TARE_SAMPLES; i++) {
        sum += HX711_ReadRaw(HX711_CHANNEL_A_GAIN_128);
        HAL_Delay(HX711_STABLE_DELAY_MS);
    }
    
    hx711_offset = sum / HX711_TARE_SAMPLES;
}

/**
 * @brief Set offset manually
 */
void HX711_SetOffset(int32_t offset) {
    hx711_offset = offset;
}

/**
 * @brief Power down HX711
 */
void HX711_PowerDown(void) {
    HAL_GPIO_WritePin(HX711_SCK_GPIO_Port, HX711_SCK_Pin, GPIO_PIN_SET);
    HAL_Delay_us(60);
}

/**
 * @brief Power up HX711
 */
void HX711_PowerUp(void) {
    HAL_GPIO_WritePin(HX711_SCK_GPIO_Port, HX711_SCK_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
}

/**
 * @brief Microsecond delay function
 * @note This is a simple implementation, for more precise timing use DWT or TIM
 */
void HAL_Delay_us(uint32_t us) {
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000);
    
    while ((DWT->CYCCNT - start) < cycles) {
        // Wait
    }
}
