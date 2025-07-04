/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "tm_stm32f4_mfrc522.h"
#include "HX711.h"
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi4;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
HX711 hx;
uint32_t no_card_counter = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI4_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Function to send card data to ESP32 in the required format
void SendCardDataToESP32(uint8_t* cardId, int32_t weight) {
    uint8_t data[11];

    // Protocol: AA 01 UID[4] WEIGHT[4] 55
    data[0] = 0xAA;      // Start byte
    data[1] = 0x01;      // Type (card data)

    // Sao chép 4 byte UID
    memcpy(&data[2], cardId, 4);

    // Gửi weight dưới dạng big-endian
    data[6] = (weight >> 24) & 0xFF; // Byte cao nhất
    data[7] = (weight >> 16) & 0xFF;
    data[8] = (weight >> 8) & 0xFF;
    data[9] = weight & 0xFF;         // Byte thấp nhất

    data[10] = 0x55;     // End byte

    // Debug dữ liệu gửi đi



    // Gửi qua UART2 (kết nối với ESP32)
    HAL_UART_Transmit(&huart1, data, 11, 1000);
}

void Test_SPI_Connection(void) {
    char debug_buf[100];

    // Test SPI by reading multiple registers
    sprintf(debug_buf, "=== SPI Communication Test ===\r\n");
    HAL_UART_Transmit(&huart1, (const uint8_t*)debug_buf, strlen(debug_buf), 1000);

    // Test CS pin control
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_SET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_SET);

    sprintf(debug_buf, "CS Pin Test: OK\r\n");
    HAL_UART_Transmit(&huart1, (const uint8_t*)debug_buf, strlen(debug_buf), 1000);
}

void Test_HX711_Connection(void) {
    char debug_buf[150];

    sprintf(debug_buf, "=== HX711 Test ===\r\n");
    HAL_UART_Transmit(&huart1, (const uint8_t*)debug_buf, strlen(debug_buf), 1000);

    // Test HX711 ready state
    if (HX711_is_ready(&hx)) {
        sprintf(debug_buf, "HX711 Ready: YES\r\n");
    } else {
        sprintf(debug_buf, "HX711 Ready: NO (Check DT pin PD1)\r\n");
    }
    HAL_UART_Transmit(&huart1, (const uint8_t*)debug_buf, strlen(debug_buf), 1000);

    // Test raw reading
    long raw_value = HX711_read(&hx);
    sprintf(debug_buf, "HX711 Raw Value: %ld\r\n", raw_value);
    HAL_UART_Transmit(&huart1, (const uint8_t*)debug_buf, strlen(debug_buf), 1000);

    // Test get_value function
    float get_value = HX711_get_value(&hx, 1);
    sprintf(debug_buf, "HX711 Get Value: %.2f\r\n", get_value);
    HAL_UART_Transmit(&huart1, (const uint8_t*)debug_buf, strlen(debug_buf), 1000);

    // Test get_units function
    float get_units = HX711_get_units(&hx, 1);
    sprintf(debug_buf, "HX711 Get Units: %.2f\r\n", get_units);
    HAL_UART_Transmit(&huart1, (const uint8_t*)debug_buf, strlen(debug_buf), 1000);

    // Check scale and offset
    float scale = HX711_get_scale(&hx);
    long offset = HX711_get_offset(&hx);
    sprintf(debug_buf, "HX711 Scale: %.2f, Offset: %ld\r\n", scale, offset);
    HAL_UART_Transmit(&huart1, (const uint8_t*)debug_buf, strlen(debug_buf), 1000);

    // Test SCK pin toggle
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_RESET);
    sprintf(debug_buf, "SCK Pin Toggle: OK\r\n");
    HAL_UART_Transmit(&huart1, (const uint8_t*)debug_buf, strlen(debug_buf), 1000);
}

void MFRC522_Debug(void) {
    char debug_buf[150];

    sprintf(debug_buf, "=== MFRC522 Debug ===\r\n");
    HAL_UART_Transmit(&huart1, (const uint8_t*)debug_buf, strlen(debug_buf), 1000);

    // Test MFRC522 communication
    uint8_t version = TM_MFRC522_ReadRegister(0x37); // Version register
    sprintf(debug_buf, "MFRC522 Version: 0x%02X (Expected: 0x91 or 0x92)\r\n", version);
    HAL_UART_Transmit(&huart1, (const uint8_t*)debug_buf, strlen(debug_buf), 1000);

    // Test antenna
    uint8_t antenna = TM_MFRC522_ReadRegister(0x14); // TxControlReg
    sprintf(debug_buf, "Antenna Status: 0x%02X\r\n", antenna);
    HAL_UART_Transmit(&huart1, (const uint8_t*)debug_buf, strlen(debug_buf), 1000);

    // Test CommandReg
    uint8_t command = TM_MFRC522_ReadRegister(0x01); // CommandReg
    sprintf(debug_buf, "Command Reg: 0x%02X\r\n", command);
    HAL_UART_Transmit(&huart1, (const uint8_t*)debug_buf, strlen(debug_buf), 1000);

    // Test Status1Reg
    uint8_t status1 = TM_MFRC522_ReadRegister(0x07); // Status1Reg
    sprintf(debug_buf, "Status1 Reg: 0x%02X\r\n", status1);
    HAL_UART_Transmit(&huart1, (const uint8_t*)debug_buf, strlen(debug_buf), 1000);

    if (version == 0x00 || version == 0xFF) {
        sprintf(debug_buf, "ERROR: No communication with MFRC522!\r\n");
        sprintf(debug_buf + strlen(debug_buf), "Check connections:\r\n");
        sprintf(debug_buf + strlen(debug_buf), "- VCC: 3.3V (NOT 5V!)\r\n");
        sprintf(debug_buf + strlen(debug_buf), "- SDA: PE4\r\n");
        sprintf(debug_buf + strlen(debug_buf), "- SCK: PE2\r\n");
        sprintf(debug_buf + strlen(debug_buf), "- MISO: PE5\r\n");
        sprintf(debug_buf + strlen(debug_buf), "- MOSI: PE6\r\n");
        HAL_UART_Transmit(&huart1, (const uint8_t*)debug_buf, strlen(debug_buf), 1000);
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI4_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  // Declare buffer here
  char buf[200];

  // Send initialization message
  sprintf(buf, "=== System Diagnostic ===\r\n");
  HAL_UART_Transmit(&huart1, (const uint8_t*)buf, strlen(buf), 1000);

  // Test SPI connection first
  Test_SPI_Connection();

  // Initialize MFRC522
  TM_MFRC522_Init();

  // Debug MFRC522
  MFRC522_Debug();

  // Initialize HX711
  HX711_begin(&hx, GPIOD, GPIO_PIN_0, GPIOD, GPIO_PIN_1, 128);

  // Test HX711 connection BEFORE configuration
  Test_HX711_Connection();

  // Configure HX711
  HX711_set_scale(&hx, 2); // Set scale to 2 for testing
  sprintf(buf, "HX711 scale set to 2 for testing\r\n");
  HAL_UART_Transmit(&huart1, (const uint8_t*)buf, strlen(buf), 1000);

  // Don't tare yet - let's see raw values first
  // HX711_tare(&hx, 10);

  sprintf(buf, "=== Initialization Complete ===\r\n");
  HAL_UART_Transmit(&huart1, (const uint8_t*)buf, strlen(buf), 1000);

  HAL_Delay(2000);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    uint8_t CardID[5];
    uint8_t status;
    int weight = 0;
    long raw_value = 0;

    // Clear CardID array
    memset(CardID, 0, sizeof(CardID));

    // Always try to read weight first
    if (HX711_is_ready(&hx)) {
        raw_value = HX711_read(&hx);
        weight = HX711_get_units(&hx, 1); // Single reading for speed
    }

    // Check for RFID card
    status = TM_MFRC522_Check(CardID);

    if (status == MI_OK) {
        // Card detected successfully
        if (HX711_is_ready(&hx)) {
            raw_value = HX711_read(&hx);
            weight = HX711_get_units(&hx, 3); // More readings for accuracy

        }
        weight = weight/ 100 -5114 + 2557;
        // Format and send card ID + weight with debug info
        sprintf(buf, "*** CARD DETECTED ***\r\nID: %02X%02X%02X%02X%02X\r\nRaw: %ld | Weight: %d g\r\n==================\r\n",
                CardID[0], CardID[1], CardID[2], CardID[3], CardID[4], raw_value, weight);

        HAL_UART_Transmit(&huart1, (const uint8_t*)buf, strlen(buf), 1000);


        SendCardDataToESP32(CardID, weight);

        // Turn on LED to indicate card read
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_SET);
        HAL_Delay(500);
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_RESET);

        // Reset counter
        no_card_counter = 0;

        // Wait to avoid multiple reads
        HAL_Delay(500);

    } else {
        // No card detected - only show occasionally
        no_card_counter++;

        if (no_card_counter >= 10) { // Show every 10th cycle (5 seconds)
            sprintf(buf, "Waiting for card... | Raw: %ld | Weight: %d g | MFRC522 Status: 0x%02X\r\n", raw_value, weight, status);
            HAL_UART_Transmit(&huart1, (const uint8_t*)buf, strlen(buf), 1000);
            HAL_UART_Transmit(&huart2, 'hello', 5, 1000);
            no_card_counter = 0;
        }

        HAL_Delay(500);
    }

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI4_Init(void)
{

  /* USER CODE BEGIN SPI4_Init 0 */

  /* USER CODE END SPI4_Init 0 */

  /* USER CODE BEGIN SPI4_Init 1 */

  /* USER CODE END SPI4_Init 1 */
  /* SPI4 parameter configuration*/
  hspi4.Instance = SPI4;
  hspi4.Init.Mode = SPI_MODE_MASTER;
  hspi4.Init.Direction = SPI_DIRECTION_2LINES;
  hspi4.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi4.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi4.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi4.Init.NSS = SPI_NSS_SOFT;
  hspi4.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32; // Reduced from 2 to 32
  hspi4.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi4.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi4.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi4.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI4_Init 2 */

  /* USER CODE END SPI4_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{
  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_SET); // CS pin HIGH initially

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PE4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : PA2 PA3 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PD0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : PD1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : PG13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
