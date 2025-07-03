/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : hx711.c
  * @brief          : HX711 weight sensor implementation
  ******************************************************************************
  */
/* USER CODE END Header */

#include "hx711.h"
#include "main.h"

// Global variables for your algorithm
uint32_t tare = 0; 
float knownOriginal = 1000.0f;  // Known weight in milligrams (1000mg = 1g)
float knownHX711 = 1.0f;        // Will be calibrated
int weight;

// Legacy global variables (kept for compatibility)
float hx711_scale = 1.0f;
int32_t hx711_offset = 0;

// External timer handle
extern TIM_HandleTypeDef htim2;

/**
 * @brief Microsecond delay using timer
 */
void microDelay(uint16_t delay) {
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while (__HAL_TIM_GET_COUNTER(&htim2) < delay);
}

/**
 * @brief Get raw data from HX711 using your algorithm
 */
int32_t getHX711(void) {
    uint32_t data = 0;
    uint32_t startTime = HAL_GetTick();
    
    // Wait for HX711 to be ready with timeout
    while(HAL_GPIO_ReadPin(HX711_DT_GPIO_Port, HX711_DT_Pin) == GPIO_PIN_SET) {
        if(HAL_GetTick() - startTime > 200)
            return 0;
    }
    
    // Read 24 bits
    for(int8_t len = 0; len < 24; len++) {
        HAL_GPIO_WritePin(HX711_SCK_GPIO_Port, HX711_SCK_Pin, GPIO_PIN_SET);
        microDelay(1);
        data = data << 1;
        HAL_GPIO_WritePin(HX711_SCK_GPIO_Port, HX711_SCK_Pin, GPIO_PIN_RESET);
        microDelay(1);
        if(HAL_GPIO_ReadPin(HX711_DT_GPIO_Port, HX711_DT_Pin) == GPIO_PIN_SET)
            data++;
    }
    
    data = data ^ 0x800000;
    
    // Extra pulse for next reading
    HAL_GPIO_WritePin(HX711_SCK_GPIO_Port, HX711_SCK_Pin, GPIO_PIN_SET);
    microDelay(1);
    HAL_GPIO_WritePin(HX711_SCK_GPIO_Port, HX711_SCK_Pin, GPIO_PIN_RESET);
    microDelay(1);
    
    return data;
}

/**
 * @brief Weigh function using your algorithm - optimized for speed
 */
int weigh() {
    int32_t total = 0;
    int32_t samples = 10;  // Reduced from 50 to 10 for faster response
    int milligram;
    float coefficient;
    
    for(uint16_t i = 0; i < samples; i++) {
        int32_t reading = getHX711();
        if (reading != 0) {  // Only add valid readings
            total += reading;
        }
    }
    
    int32_t average = (int32_t)(total / samples);
    coefficient = knownOriginal / knownHX711;
    milligram = (int)(average - tare) * coefficient;
    
    return milligram;
}

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
 * @brief Tare function using your algorithm - optimized for speed
 */
void HX711_Tare_New(void) {
    int32_t total = 0;
    int32_t samples = 5;  // Reduced from 10 to 5 for faster tare
    
    for(uint16_t i = 0; i < samples; i++) {
        int32_t reading = getHX711();
        if (reading != 0) {
            total += reading;
        }
        HAL_Delay(20);  // Reduced from 50ms to 20ms
    }
    
    tare = total / samples;
}

/**
 * @brief Set calibration values
 */
void HX711_SetCalibration(float known_weight_mg, float hx711_reading) {
    knownOriginal = known_weight_mg;
    knownHX711 = hx711_reading;
}

/**
 * @brief Get current tare value
 */
uint32_t HX711_GetTare(void) {
    return tare;
}

/**
 * @brief Get calibration coefficient
 */
float HX711_GetCalibrationCoefficient(void) {
    if (knownHX711 == 0) return 1.0f;
    return knownOriginal / knownHX711;
}

/**
 * @brief Read weight using your primary algorithm
 */
float HX711_ReadWeight_Primary(void) {
    int weight_mg = weigh();
    return (float)weight_mg / 1000.0f; // Convert mg to grams
}

// Legacy functions for compatibility
uint32_t HX711_ReadRaw(uint8_t channel) {
    (void)channel; // Suppress unused parameter warning
    return (uint32_t)getHX711();
}

int32_t HX711_ReadValue(uint8_t channel) {
    return (int32_t)HX711_ReadRaw(channel) - hx711_offset;
}

float HX711_ReadWeight(void) {
    int32_t value = HX711_ReadValue(HX711_CHANNEL_A_GAIN_128);
    return (float)value / hx711_scale;
}

void HX711_SetScale(float scale) {
    hx711_scale = scale;
}

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

void HX711_SetOffset(int32_t offset) {
    hx711_offset = offset;
}

void HX711_PowerDown(void) {
    HAL_GPIO_WritePin(HX711_SCK_GPIO_Port, HX711_SCK_Pin, GPIO_PIN_SET);
    microDelay(60);
}

void HX711_PowerUp(void) {
    HAL_GPIO_WritePin(HX711_SCK_GPIO_Port, HX711_SCK_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
}
