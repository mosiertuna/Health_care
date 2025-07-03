/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : hx711.h
  * @brief          : Header for HX711 weight sensor module
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __HX711_H
#define __HX711_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "main.h"

// HX711 Channel and Gain Selection
#define HX711_CHANNEL_A_GAIN_128    1
#define HX711_CHANNEL_B_GAIN_32     2
#define HX711_CHANNEL_A_GAIN_64     3

// Function prototypes
void HX711_Init(void);
uint8_t HX711_IsReady(void);
uint32_t HX711_ReadRaw(uint8_t channel);
int32_t HX711_ReadValue(uint8_t channel);
float HX711_ReadWeight(void);
void HX711_SetScale(float scale);
void HX711_Tare(void);
void HX711_SetOffset(int32_t offset);
void HX711_PowerDown(void);
void HX711_PowerUp(void);

// New functions for your algorithm
void microDelay(uint16_t delay);
int32_t getHX711(void);
int weigh(void);
void HX711_Tare_New(void);
void HX711_SetCalibration(float known_weight_mg, float hx711_reading);
uint32_t HX711_GetTare(void);
float HX711_GetCalibrationCoefficient(void);
float HX711_ReadWeight_Primary(void);

// Global variables
extern float hx711_scale;
extern int32_t hx711_offset;
extern uint32_t tare;
extern float knownOriginal;
extern float knownHX711;
extern int weight;

#ifdef __cplusplus
}
#endif

#endif /* __HX711_H */
