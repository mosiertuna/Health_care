/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "simple_protocol.h"
#include <stdio.h>
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
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
SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
// External declarations for debug UART
#if ENABLE_DEBUG_UART
void Debug_Printf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    for (int i = 0; buffer[i]; i++) {
        ITM_SendChar(buffer[i]); // Gửi qua ITM Port 0
    }
}
#endif
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
__attribute__((unused)) static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
void System_Error_Handler(const char* file, int line, const char* func) {
    // Basic error handler with debug info
    while(1) {
#if ENABLE_DEBUG_UART
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Error in %s:%d (%s)\r\n", file, line, func);
        Debug_Printf(error_msg);
#endif
        HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_13); // Toggle LED to indicate error
        HAL_Delay(200);
    }
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

uint8_t HX711_Enhanced_IsReady(void) {
    for (int retry = 0; retry < 3; retry++) {
        if (HX711_IsReady()) {
            return 1;
        }
        HAL_Delay(10);
    }
    return 0;
}

void Enhanced_Weight_Debug(void) {
    // Debug function disabled
}

void Print_Float_Debug(const char* label, float value) {
    // Debug function disabled
}

// Application variables
uint8_t card_uid[5];
float current_weight = 0.0f;
uint32_t last_card_check = 0;
uint32_t last_weight_check = 0;
uint32_t last_card_read_time = 0;
uint8_t system_ready = 0;
uint8_t last_card_uid[UID_SIZE] = {0};
uint8_t card_present = 0;

#if ENABLE_STATUS_LEDS
void Status_LED_Control(uint8_t led_type, uint8_t state);
#endif

#if ENABLE_DEBUG_UART
/**
 * @brief Send debug message
 */
void Debug_Printf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    HAL_UART_Transmit(&DEBUG_UART_HANDLE, (uint8_t*)buffer, strlen(buffer), 1000);
}
#else
/**
 * @brief Empty debug function when debug is disabled
 */
void Debug_Printf(const char* format, ...) {
    (void)format;  // Suppress unused parameter warning
}
#endif

/**
 * @brief Initialize all modules
 */
void System_Init(void) {
    Debug_Printf("Healthcare RFID System Starting...\r\n");
    
    // Initialize RC522 RFID module
    RC522_Init();
    Debug_Printf("RC522 RFID initialized\r\n");
    
    // Initialize HX711 weight sensor
    HX711_Init();
    Debug_Printf("HX711 weight sensor initialized\r\n");
    
    // Initialize Simple Protocol
    SimpleProtocol_Init();
    Debug_Printf("Simple Protocol initialized\r\n");
    
    // Enable DWT for microsecond delays (used by HX711)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    
    // Wait for system stabilization
    Debug_Printf("System stabilizing...\r\n");
    HAL_Delay(SYSTEM_INIT_DELAY_MS);
    
    // Tare the scale
    Debug_Printf("Taring scale...\r\n");
    HX711_Tare();
    
    // Debug: Check tare result
    char tare_result[100];
    snprintf(tare_result, sizeof(tare_result), "Tare completed. Offset: %ld\r\n", hx711_offset);
    Debug_Printf(tare_result);
    
    // Set scale factor (this should be calibrated for your specific load cell)
    HX711_SetScale(HX711_DEFAULT_SCALE);
    Debug_Printf("Scale factor set to %.1f\r\n", HX711_DEFAULT_SCALE);
    
    // CRITICAL: Test float printing capability
    float test_value = 123.45f;
    char float_test[80];
    snprintf(float_test, sizeof(float_test), "Float test 123.45: %.2f\r\n", test_value); // @suppress("Float formatting support")
    Debug_Printf(float_test);
    
    // Debug: Verify scale was set correctly
    char scale_verify[100];
    snprintf(scale_verify, sizeof(scale_verify), "Verified scale: %.3f\r\n", hx711_scale); // @suppress("Float formatting support")
    Debug_Printf(scale_verify);
    
    // Force fix if scale is still invalid
    if (hx711_scale == 0.0f || hx711_scale != hx711_scale) {
        Debug_Printf("ERROR: Scale still invalid after set! Force fixing...\r\n");
        hx711_scale = 420.0f; // Force assignment with known value
        char force_fix[100];
        snprintf(force_fix, sizeof(force_fix), "Force fixed scale to: %.3f\r\n", hx711_scale); // @suppress("Float formatting support")
        Debug_Printf(force_fix);
    } else {
        Debug_Printf("Scale appears to be set correctly.\r\n");
    }
    
    // Test HX711 immediately after init
    if (HX711_IsReady()) {
        uint32_t test_raw = HX711_ReadRaw(HX711_CHANNEL_A_GAIN_128);
        char test_result[150];
        snprintf(test_result, sizeof(test_result), "HX711 Test - Raw: %lu (0x%08lX)\r\n", test_raw, test_raw);
        Debug_Printf(test_result);
        
        if (test_raw == 0xFFFFFFFF || test_raw == 0x00000000) {
            Debug_Printf("WARNING: HX711 returning invalid data!\r\n");
        } else {
            Debug_Printf("HX711 seems to be working.\r\n");
        }
    } else {
        Debug_Printf("WARNING: HX711 not ready after init!\r\n");
    }
    
#if ENABLE_STATUS_LEDS
    // Initialize status LEDs
    Status_LED_Control(2, 1); // System ready LED ON
    Debug_Printf("Status LEDs initialized\r\n");
#endif
    
    system_ready = 1;
    Debug_Printf("System ready!\r\n");
}

/**
 * @brief Process RFID card reading - Simple protocol version
 */
void Process_RFID(void) {
    RC522_Status status;
    uint32_t current_time = HAL_GetTick();
    static uint8_t card_sent = 0;  // Flag to prevent multiple sends
    
    status = RC522_ReadUID(card_uid);
    
    if (status == RC522_OK) {
        // Check if this is the same card as before
        uint8_t same_card = (memcmp(card_uid, last_card_uid, UID_SIZE) == 0);
        
        // Only process if it's a new card or enough time has passed
        if (!same_card || (current_time - last_card_read_time > RC522_CARD_DETECTION_DELAY_MS)) {
            
            // Reset card sent flag for new card
            if (!same_card) {
                card_sent = 0;
            }
            
            // Only send once per card detection
            if (!card_sent) {
                // Get current weight if HX711 is ready
                float weight = 0.0f;
                if (HX711_Enhanced_IsReady()) {
                    uint32_t raw_data = HX711_ReadRaw(HX711_CHANNEL_A_GAIN_128);
                    if (raw_data != 0xFFFFFFFF && raw_data != 0x00000000) {
                        // Simple weight calculation
                        int32_t value_with_offset = (int32_t)raw_data - hx711_offset;
                        weight = (float)value_with_offset / hx711_scale;
                        weight *= 1000.0f; // Convert to grams
                    }
                }
                
                // Send data using simple protocol
                SimpleProtocol_ProcessCardDetection(card_uid, weight);
                
                // Update status
                memcpy(last_card_uid, card_uid, UID_SIZE);
                last_card_read_time = current_time;
                card_present = 1;
                card_sent = 1;  // Mark as sent
            }
        }
    } else {
        // No card detected
        if (card_present) {
            // Card was removed - reset flag
            card_present = 0;
            card_sent = 0;
        }
    }
}

/**
 * @brief Process weight measurement with filtering
 */
void Process_Weight(void) {
    static float weight_buffer[WEIGHT_FILTER_SAMPLES] = {0};
    static uint8_t weight_index = 0;
    static uint8_t buffer_filled = 0;
    static float last_stable_weight = 0.0f;
    
    if (HX711_IsReady()) {
        float raw_weight = HX711_ReadWeight();
        // Apply correct formula: weight_in_grams = (raw_value / 10000) - 600
        float new_weight = (raw_weight / 10000.0f) - 600.0f;
        
#if WEIGHT_FILTER_ENABLED
        // Add to circular buffer
        weight_buffer[weight_index] = new_weight;
        weight_index = (weight_index + 1) % WEIGHT_FILTER_SAMPLES;
        
        if (!buffer_filled && weight_index == 0) {
            buffer_filled = 1;
        }
        
        // Calculate average if buffer is filled
        if (buffer_filled) {
            float sum = 0;
            for (int i = 0; i < WEIGHT_FILTER_SAMPLES; i++) {
                sum += weight_buffer[i];
            }
            current_weight = sum / WEIGHT_FILTER_SAMPLES;
        } else {
            current_weight = new_weight;
        }
#else
        current_weight = new_weight;
#endif
        
        // Check for significant weight change
        float weight_diff = current_weight - last_stable_weight;
        if (weight_diff < 0) weight_diff = -weight_diff; // Absolute value
        
        if (weight_diff > WEIGHT_THRESHOLD_GRAMS) {
            last_stable_weight = current_weight;
            
            // Optional: Send weight-only data to ESP32 for monitoring
            // ESP32_SendCardData(NULL, 0, current_weight);
        }
    }
}

#if ENABLE_STATUS_LEDS
/**
 * @brief Control status LEDs
 * @param led_type: 0=Valid Card, 1=Invalid Card, 2=System Ready
 * @param state: 0=OFF, 1=ON
 */
void Status_LED_Control(uint8_t led_type, uint8_t state) {
    switch (led_type) {
        case 0: // Valid card LED
            HAL_GPIO_WritePin(LED_VALID_CARD_PORT, LED_VALID_CARD_PIN, 
                            state ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;
        case 1: // Invalid card LED
            HAL_GPIO_WritePin(LED_INVALID_CARD_PORT, LED_INVALID_CARD_PIN, 
                            state ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;
        case 2: // System ready LED
            HAL_GPIO_WritePin(LED_SYSTEM_READY_PORT, LED_SYSTEM_READY_PIN, 
                            state ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;
    }
}
#endif

/**
 * @brief Calibrate HX711 scale
 * Call this function during development to determine scale factor
 */
void HX711_Calibrate(void) {
    Debug_Printf("=== HX711 Calibration ===\r\n");
    Debug_Printf("1. Remove all weight from scale\r\n");
    Debug_Printf("2. Press any key to tare...\r\n");
    
    // Wait for input (you can modify this for your needs)
    HAL_Delay(5000);
    
    // Tare the scale
    HX711_Tare();
    Debug_Printf("Scale tared. Zero offset: %ld\r\n", hx711_offset);
    
    Debug_Printf("3. Place a known weight (e.g., 1000g) on scale\r\n");
    Debug_Printf("4. Waiting 5 seconds...\r\n");
    HAL_Delay(5000);
    
    // Read raw value with known weight
    int32_t raw_with_weight = HX711_ReadValue(HX711_CHANNEL_A_GAIN_128);
    Debug_Printf("Raw value with weight: %ld\r\n", raw_with_weight);
    
    // Calculate scale factor for 1000g
    float calculated_scale = (float)raw_with_weight / 1000.0f;
    Debug_Printf("Calculated scale factor: %.2f\r\n", calculated_scale);
    Debug_Printf("Update HX711_DEFAULT_SCALE in config.h to: %.2f\r\n", calculated_scale);
    
    // Temporarily set the calculated scale for immediate testing
    HX711_SetScale(calculated_scale);
}

/**
 * @brief Test all system components
 */
void System_Test(void) {
    Debug_Printf("=== System Test ===\r\n");
    
    // Test RC522
    Debug_Printf("Testing RC522...\r\n");
    uint8_t version = RC522_ReadRegister(RC522_REG_VERSION);
    Debug_Printf("RC522 Version: 0x%02X (should be 0x91, 0x92, or 0xB2)\r\n", version);
    
    if (version == 0x91 || version == 0x92 || version == 0xB2) {
        Debug_Printf("RC522 test: PASSED\r\n");
    } else {
        Debug_Printf("RC522 test: FAILED - Check connections\r\n");
    }
    
    // Test HX711
    Debug_Printf("Testing HX711...\r\n");
    if (HX711_IsReady()) {
        float raw_weight = HX711_ReadWeight();
        // Apply correct formula: weight_in_grams = (raw_value / 10000) - 600
        float weight = (raw_weight / 10000.0f) - 600.0f;
        Debug_Printf("Raw weight: %.0f, Converted: %.2f g\r\n", raw_weight, weight);
        Debug_Printf("HX711 test: PASSED\r\n");
        
        // Use weight variable to avoid warning
        (void)weight;
    } else {
        Debug_Printf("HX711 not ready!\r\n");
        Debug_Printf("HX711 test: FAILED - Check connections\r\n");
    }
    
    // Test Simple Protocol communication
    Debug_Printf("Testing Simple Protocol communication...\r\n");
    uint8_t test_uid[] = {0x12, 0x34, 0x56, 0x78};
    SimpleProtocol_SendRegisteredCard(test_uid, 123.45f);
    Debug_Printf("Test message sent to ESP32\r\n");
    
    Debug_Printf("=== Test Complete ===\r\n");
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();
  SystemClock_Config();
  
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */
  System_Init();
  SimpleProtocol_Init(); // This will start UART interrupt

  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    Process_RFID();
    HAL_Delay(100);
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
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
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 84-1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, HC_TRIG_Pin|ILI9341_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, RC522_CS_Pin|ILI9341_DC_Pin|ILI9341_RESET_Pin|HX711_SCK_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : HC_TRIG_Pin ILI9341_CS_Pin */
  GPIO_InitStruct.Pin = HC_TRIG_Pin|ILI9341_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : HC_ECHO_Pin */
  GPIO_InitStruct.Pin = HC_ECHO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(HC_ECHO_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : RC522_CS_Pin ILI9341_DC_Pin ILI9341_RESET_Pin HX711_SCK_Pin */
  GPIO_InitStruct.Pin = RC522_CS_Pin|ILI9341_DC_Pin|ILI9341_RESET_Pin|HX711_SCK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : HX711_DT_Pin */
  GPIO_InitStruct.Pin = HX711_DT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(HX711_DT_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        SimpleProtocol_UART_RxCpltCallback();
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  System_Error_Handler(__FILE__, __LINE__, __FUNCTION__);
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
