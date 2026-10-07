#include "main.h"
#include "TM1640.h"

/* fac_us为SysTick时钟的频率倍数，在FreeRTOS中为AHB时钟频率
 * 比如为168MHz，那么fac_us=168
 */

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
            else tcnt += reload + 1U - tnow + told;  //计数器向下溢出
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

/* Logical row bits map to the schematic COL1..COL8 wiring. */
uint8_t TM1640_row_generate(uint8_t dat)
{
    static const uint8_t masks[8] = {
        0x04, 0x20, 0x40, 0x10, 0x80, 0x08, 0x02, 0x01
    };
    uint8_t out = 0;
    for (uint8_t c = 0; c < 8; ++c)
    {
        if (dat & (1U << c)) out |= masks[c];
    }
    return out;
}

void TM1640_displayOnOff(tm1640_hw_t dev, uint8_t on)
{
    TM1640_Generate_START(dev);
    TM1640_WriteData(dev, (uint8_t)(0x80U | (on ? 0x08U : 0U) |
                                  (BRIGHTNESS_TM1640 & 0x07U)));
    TM1640_Generate_STOP(dev);
}

/* rows[0..7] = ROW1..ROW8, each byte bit 0..7 = COL1..COL8. */
void TM1640_display_frame(tm1640_hw_t dev, const uint8_t rows[8])
{
    static const uint8_t row_addr[8] = {
        0x0D, 0x0F, 0x00, 0x0E, 0x0A, 0x01, 0x09, 0x02
    };
    uint8_t frame[16] = {0};
    for (uint8_t r = 0; r < 8; ++r)
    {
        frame[row_addr[r]] = TM1640_row_generate(rows[r]);
    }

    TM1640_Generate_START(dev);
    TM1640_WriteData(dev, 0x40); /* Auto-increment address mode. */
    TM1640_Generate_STOP(dev);

    TM1640_Generate_START(dev);
    TM1640_WriteData(dev, 0xC0);
    for (uint8_t i = 0; i < 16; ++i) TM1640_WriteData(dev, frame[i]);
    TM1640_Generate_STOP(dev);
    TM1640_displayOnOff(dev, 1);
}

/* Retained diagnostic API: raw SEG bits written to all eight connected rows. */
void TM1640_display_byte(tm1640_hw_t dev, uint8_t Seg_N, uint8_t DispData)
{
    static const uint8_t connected[16] = {
        1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1
    };
    (void)Seg_N;
    TM1640_Generate_START(dev);
    TM1640_WriteData(dev, 0x40);
    TM1640_Generate_STOP(dev);
    TM1640_Generate_START(dev);
    TM1640_WriteData(dev, 0xC0);
    for (uint8_t i = 0; i < 16; ++i)
        TM1640_WriteData(dev, connected[i] ? DispData : 0);
    TM1640_Generate_STOP(dev);
    TM1640_displayOnOff(dev, 1);
}

/* Schematic coordinates 1..8; invalid coordinates clear the display. */
void TM1640_test_pixel(tm1640_hw_t dev, uint8_t row, uint8_t col)
{
    uint8_t rows[8] = {0};
    if (row >= 1 && row <= 8 && col >= 1 && col <= 8)
        rows[row - 1] = (uint8_t)(1U << (col - 1));
    TM1640_display_frame(dev, rows);
}
