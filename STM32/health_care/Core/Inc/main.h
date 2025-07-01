/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "config.h"
#include "rc522.h"
#include "hx711.h"
#include "simple_protocol.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void System_Error_Handler(const char* file, int line, const char* function);
void Debug_Printf(const char* format, ...);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define HC_TRIG_Pin GPIO_PIN_0
#define HC_TRIG_GPIO_Port GPIOC
#define ILI9341_CS_Pin GPIO_PIN_1
#define ILI9341_CS_GPIO_Port GPIOC
#define HC_ECHO_Pin GPIO_PIN_4
#define HC_ECHO_GPIO_Port GPIOC
#define RC522_CS_Pin GPIO_PIN_0
#define RC522_CS_GPIO_Port GPIOB
// RC522 Reset pin configuration
#define RC522_RST_Pin GPIO_PIN_2
#define RC522_RST_GPIO_Port GPIOB
#define ILI9341_DC_Pin GPIO_PIN_1
#define ILI9341_DC_GPIO_Port GPIOB
#define ILI9341_RESET_Pin GPIO_PIN_2
#define ILI9341_RESET_GPIO_Port GPIOB
#define HX711_DT_Pin GPIO_PIN_11
#define HX711_DT_GPIO_Port GPIOB
#define HX711_SCK_Pin GPIO_PIN_12
#define HX711_SCK_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
