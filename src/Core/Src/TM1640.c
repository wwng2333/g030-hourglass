#include "main.h"
#include "TM1640.h"

/* fac_us为SysTick时钟的频率倍数，在FreeRTOS中为AHB时钟频率
 * 比如为168MHz，那么fac_us=168
 */
#define fac_us 16

void TM1640_Delay(uint32_t nus)
{
    uint32_t ticks;
    uint32_t told, tnow, tcnt = 0;
    uint32_t reload = SysTick->LOAD;    //LOAD的值
    ticks = nus * fac_us;   //需要的节拍数
    told = SysTick->VAL;    //刚进入时的计数器的值
    while (1)
    {
        tnow = SysTick->VAL;
        if(tnow != told)
        {
            if(tnow < told) tcnt += told - tnow;    //这里注意一下SYSTICK是一个递减计数器
            else tcnt += reload - tnow + told;  //计数器向下溢出
            told = tnow;
            if(tcnt >= ticks) break;    //时间超过/等于要延迟的时间，则退出
        }
    }
}

void TM1640_Generate_START(tm1640_hw_t TM1640)
{
	LL_GPIO_SetOutputPin(TM1640->CLK_GPIOx, TM1640->CLK_PIN); //CLK_HIGH
	LL_GPIO_SetOutputPin(TM1640->DIN_GPIOx, TM1640->DIN_PIN); //DIN_HIGH
	TM1640_Delay(DELAY_TIME);
	LL_GPIO_ResetOutputPin(TM1640->DIN_GPIOx, TM1640->DIN_PIN); //DIN_LOW
}

void TM1640_Generate_STOP(tm1640_hw_t TM1640)
{
	LL_GPIO_ResetOutputPin(TM1640->CLK_GPIOx, TM1640->CLK_PIN); //CLK_LOW
	TM1640_Delay(DELAY_TIME);
	LL_GPIO_ResetOutputPin(TM1640->DIN_GPIOx, TM1640->DIN_PIN); //DIN_LOW
	TM1640_Delay(DELAY_TIME);
	LL_GPIO_SetOutputPin(TM1640->CLK_GPIOx, TM1640->CLK_PIN); //CLK_HIGH
	TM1640_Delay(DELAY_TIME);
	LL_GPIO_SetOutputPin(TM1640->DIN_GPIOx, TM1640->DIN_PIN); //DIN_HIGH
}

void TM1640_WriteData(tm1640_hw_t TM1640, uint8_t dat)
{
	for(uint8_t i=0; i<8; i++)
	{
		LL_GPIO_ResetOutputPin(TM1640->CLK_GPIOx, TM1640->CLK_PIN); //CLK_LOW
		TM1640_Delay(DELAY_TIME);
		if(dat & (1 << i))
		{
			LL_GPIO_SetOutputPin(TM1640->DIN_GPIOx, TM1640->DIN_PIN); //DIN_HIGH
		}
		else
		{
			LL_GPIO_ResetOutputPin(TM1640->DIN_GPIOx, TM1640->DIN_PIN); //DIN_LOW
		}
		TM1640_Delay(DELAY_TIME);
		LL_GPIO_SetOutputPin(TM1640->CLK_GPIOx, TM1640->CLK_PIN); //CLK_HIGH
		TM1640_Delay(DELAY_TIME);
	}
	LL_GPIO_ResetOutputPin(TM1640->CLK_GPIOx, TM1640->CLK_PIN); //CLK_LOW
	TM1640_Delay(DELAY_TIME);
	LL_GPIO_ResetOutputPin(TM1640->DIN_GPIOx, TM1640->DIN_PIN); //DIN_LOW
	while(LL_GPIO_IsOutputPinSet(TM1640->DIN_GPIOx, TM1640->DIN_PIN)); // while DIN_HIGH
}

void TM1640_display_byte(tm1640_hw_t TM1640, uint8_t Seg_N, uint8_t DispData)
{
	TM1640_Generate_START(TM1640);                                
	TM1640_WriteData(TM1640, 0x44);
	TM1640_Generate_STOP(TM1640);
	
	TM1640_Generate_START(TM1640);
	TM1640_WriteData(TM1640, 0xC0);
	TM1640_WriteData(TM1640, DispData);
	TM1640_WriteData(TM1640, DispData);
	TM1640_WriteData(TM1640, DispData);
	TM1640_WriteData(TM1640, DispData);
	TM1640_WriteData(TM1640, DispData);
	TM1640_WriteData(TM1640, DispData);
	TM1640_WriteData(TM1640, DispData);
	TM1640_WriteData(TM1640, DispData);
	TM1640_WriteData(TM1640, DispData);
	TM1640_WriteData(TM1640, DispData);
	TM1640_WriteData(TM1640, DispData);
	TM1640_WriteData(TM1640, DispData);
	TM1640_WriteData(TM1640, DispData);
	TM1640_WriteData(TM1640, DispData);
	TM1640_WriteData(TM1640, DispData);
	TM1640_WriteData(TM1640, DispData);
	TM1640_Generate_STOP(TM1640);
	
	TM1640_Generate_START(TM1640);
	TM1640_WriteData(TM1640, 0x88);
	TM1640_Generate_STOP(TM1640);
}

void TM1640_displayOnOff(tm1640_hw_t TM1640, uint8_t OnOff)
{
	TM1640_Generate_START(TM1640);                                
	TM1640_WriteData(TM1640, 0x44);
	TM1640_Generate_STOP(TM1640);
	
	TM1640_Generate_START(TM1640);  
	TM1640_WriteData(TM1640, OnOff ? (BRIGHTNESS_TM1640 | 1<<3 ) : ( BRIGHTNESS_TM1640 & ~( 1<<3 )) );
	TM1640_Generate_STOP(TM1640);
}