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
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "TM1640.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* User-adjustable settings. Duration is nominal gate time, not a precision timer. */
/* 1: scan physical rows/columns; 0: run the hourglass. */
#define HG_DISPLAY_TEST      0
#define HG_GRAIN_COUNT       32U
#define HG_DURATION_MS       60000UL
#define HG_ANIMATION_MS      40UL
#define HG_DEBOUNCE_MS       80UL
#define HG_GATE_MS           (HG_DURATION_MS / HG_GRAIN_COUNT)

#if HG_GRAIN_COUNT < 1 || HG_GRAIN_COUNT > 64
#error HG_GRAIN_COUNT_must_be_1_to_64
#endif
#if HG_DURATION_MS < HG_GRAIN_COUNT
#error HG_DURATION_MS_is_too_short
#endif
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* Kept visible for debugger inspection: raw inputs and accepted orientation. */
volatile uint8_t PA4, PB5, last_state = 0;
volatile uint32_t hourglass_ms = 0;
/* Mapping test: axis 0 = ROW, 1 = COL; index 1..8, 0 = blank gap. */
volatile uint8_t hg_test_axis = 0;
volatile uint8_t hg_test_index = 0;

/* Bit c of sand[module][r] is ROW(r+1)/COL(c+1). */
static uint8_t sand[2][8];
static uint8_t initialized;
static uint8_t candidate;
static uint8_t gravity;
static uint32_t candidate_since;
static uint32_t previous_ms;
static uint32_t animation_ms;
static uint32_t gate_ms;
static uint32_t random_state = 0x13579BDFUL;

enum { HG_PAUSED = 0, HG_DOWN = 1, HG_UP = 2 };
    
tm1640_hw_dat tm1640_m1 = {
	.DIN_GPIOx = GPIOA,
	.CLK_GPIOx = GPIOA,
	.DIN_PIN = LL_GPIO_PIN_3,
	.CLK_PIN = LL_GPIO_PIN_2,
};

tm1640_hw_dat tm1640_m2 = {
	.DIN_GPIOx = GPIOA,
	.CLK_GPIOx = GPIOA,
	.DIN_PIN = LL_GPIO_PIN_1,
	.CLK_PIN = LL_GPIO_PIN_0,
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint8_t HG_ReadOrientation(void)
{
    PA4 = (uint8_t)LL_GPIO_IsInputPinSet(GPIOA, LL_GPIO_PIN_4);
    PB5 = (uint8_t)LL_GPIO_IsInputPinSet(GPIOB, LL_GPIO_PIN_5);
    /* Direction polarity corrected from the assembled hardware test. */
    if (!PA4 && PB5) return HG_UP;
    if (PA4 && !PB5) return HG_DOWN;
    return HG_PAUSED;
}

static uint8_t HG_RandomBit(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return (uint8_t)(random_state & 1U);
}

static uint8_t HG_TryMove(uint8_t module, int r, int c, int nr, int nc)
{
    if (nr < 0 || nr >= 8 || nc < 0 || nc >= 8) return 0;
    if (sand[module][nr] & (1U << nc)) return 0;
    sand[module][r] &= (uint8_t)~(1U << c);
    sand[module][nr] |= (uint8_t)(1U << nc);
    return 1;
}

static uint8_t HG_StepChamber(uint8_t module, uint8_t direction)
{
    int dr = (direction == HG_DOWN) ? 1 : -1;
    int dc = -dr;
    uint8_t changed = 0;

    /* Visit the gravity-facing edge first: a moved grain cannot move twice. */
    for (int ri = 0; ri < 8; ++ri)
    {
        int r = (direction == HG_DOWN) ? 7 - ri : ri;
        for (int ci = 0; ci < 8; ++ci)
        {
            int c = (direction == HG_DOWN) ? ci : 7 - ci;
            if (!(sand[module][r] & (1U << c))) continue;
            if (HG_TryMove(module, r, c, r + dr, c + dc))
            {
                changed = 1;
                continue;
            }
            /* Both alternatives are physically downhill in the diamond. */
            if (HG_RandomBit())
            {
                changed |= HG_TryMove(module, r, c, r + dr, c) ||
                           HG_TryMove(module, r, c, r, c + dc);
            }
            else
            {
                changed |= HG_TryMove(module, r, c, r, c + dc) ||
                           HG_TryMove(module, r, c, r + dr, c);
            }
        }
    }
    return changed;
}

static void HG_Seed(uint8_t direction)
{
    uint8_t module = (direction == HG_DOWN) ? 0 : 1;
    uint8_t count = 0;

    /* Fill the lowest cells of the upper chamber, starting at its outlet. */
    for (int level = 7; level >= -7; --level)
    {
        for (int r = 0; r < 8; ++r)
        {
            int c = r - level;
            if (c < 0 || c >= 8) continue;
            int rr = (direction == HG_DOWN) ? r : 7 - r;
            int cc = (direction == HG_DOWN) ? c : 7 - c;
            sand[module][rr] |= (uint8_t)(1U << cc);
            if (++count == HG_GRAIN_COUNT) return;
        }
    }
}

static uint8_t HG_Transfer(uint8_t direction)
{
    /* A(8,1) <-> B(1,8); transfer only when the destination is empty. */
    if (direction == HG_DOWN)
    {
        if (!(sand[0][7] & 0x01U) || (sand[1][0] & 0x80U)) return 0;
        sand[0][7] &= (uint8_t)~0x01U;
        sand[1][0] |= 0x80U;
    }
    else
    {
        if (!(sand[1][0] & 0x80U) || (sand[0][7] & 0x01U)) return 0;
        sand[1][0] &= (uint8_t)~0x80U;
        sand[0][7] |= 0x01U;
    }
    return 1;
}

static void HG_Render(void)
{
    /* Logical chambers A/B are routed to M2/M1 after hardware feedback. */
    TM1640_display_frame(&tm1640_m2, sand[0]);
    TM1640_display_frame(&tm1640_m1, sand[1]);
}

#if HG_DISPLAY_TEST
static void HG_DisplayMappingTest(void)
{
    uint8_t rows[8];
    for (uint8_t axis = 0; axis < 2; ++axis)
    {
        hg_test_axis = axis;
        for (uint8_t index = 0; index < 8; ++index)
        {
            for (uint8_t r = 0; r < 8; ++r)
                rows[r] = (axis == 0) ? ((r == index) ? 0xFFU : 0U)
                                     : (uint8_t)(1U << index);
            TM1640_display_frame(&tm1640_m1, rows);
            TM1640_display_frame(&tm1640_m2, rows);
            hg_test_index = index + 1U;
            LL_mDelay(1500);

            for (uint8_t r = 0; r < 8; ++r) rows[r] = 0;
            TM1640_display_frame(&tm1640_m1, rows);
            TM1640_display_frame(&tm1640_m2, rows);
            hg_test_index = 0;
            LL_mDelay(300);
        }
        LL_mDelay(2000);
    }
}
#endif

static void HG_Update(void)
{
    uint32_t now = hourglass_ms;
    uint32_t elapsed = now - previous_ms;
    uint8_t raw = HG_ReadOrientation();
    uint8_t changed;
    previous_ms = now;

    if (raw != candidate)
    {
        candidate = raw;
        candidate_since = now;
    }

    /* Freeze immediately on an uncertain reading; resume after debounce. */
    if (raw == HG_PAUSED || (uint32_t)(now - candidate_since) < HG_DEBOUNCE_MS)
    {
        last_state = HG_PAUSED;
        return;
    }

    if (last_state != raw) elapsed = 0;
    last_state = raw;
    if (gravity != raw)
    {
        gravity = raw;
        gate_ms = 0;
        animation_ms = 0;
    }

    if (!initialized)
    {
        HG_Seed(gravity);
        initialized = 1;
        HG_Render();
        previous_ms = hourglass_ms;
        return;
    }

    /* Pause preserves partial gate time. Reversal starts a new gate interval. */
    if (gate_ms < HG_GATE_MS)
    {
        uint32_t remaining = HG_GATE_MS - gate_ms;
        gate_ms += (elapsed < remaining) ? elapsed : remaining;
    }
    animation_ms += elapsed;
    if (animation_ms < HG_ANIMATION_MS) return;
    animation_ms %= HG_ANIMATION_MS;

    changed = HG_StepChamber(0, gravity);
    changed |= HG_StepChamber(1, gravity);
    /* Transfer after movement so a new arrival moves only on the next frame. */
    if (gate_ms >= HG_GATE_MS && HG_Transfer(gravity))
    {
        gate_ms = 0;
        changed = 1;
    }
    if (changed) HG_Render();
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
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

  /* SysTick_IRQn interrupt configuration */
  NVIC_SetPriority(SysTick_IRQn, 3);

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  //EventRecorderInitialize(EventRecordAll, 1);
  //EventRecorderStart();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */
  /* SysTick already runs at 1 ms; enable its interrupt for application time. */
  LL_SYSTICK_EnableIT();
  HG_Render();
  previous_ms = hourglass_ms;
  candidate_since = previous_ms;
/* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
/* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
#if HG_DISPLAY_TEST
    HG_DisplayMappingTest();
#else
    HG_Update();
    LL_mDelay(1);
#endif
  }
/* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  /* HSI configuration and activation */
  LL_RCC_HSI_Enable();
  while(LL_RCC_HSI_IsReady() != 1)
  {
  }

  /* Set AHB prescaler*/
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);

  /* Sysclk activation on the HSI */
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
  {
  }

  /* Set APB1 prescaler*/
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);

  LL_Init1msTick(16000000);

  /* Update CMSIS variable (which can be updated also through SystemCoreClockUpdate function) */
  LL_SetSystemCoreClock(16000000);
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
