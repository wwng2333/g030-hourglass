/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __TM1640_H
#define __TM1640_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

#include "main.h"

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#define BRIGHTNESS_TM1640 1
#define DELAY_TIME 100
/* USER CODE END EM */

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */
typedef struct
{
	GPIO_TypeDef *DIN_GPIOx, *CLK_GPIOx;
	uint32_t DIN_PIN, CLK_PIN;
} tm1640_hw_dat, *tm1640_hw_t;

void TM1640_display_byte(tm1640_hw_t TM1640, uint8_t Seg_N, uint8_t DispData);
void TM1640_displayOnOff(tm1640_hw_t TM1640, uint8_t OnOff);

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
