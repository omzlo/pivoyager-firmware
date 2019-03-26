#include <stm32f0xx.h>
#include "systick.h"
//#include "usart.h"

static __IO uint32_t TimingDelay;
static __IO uint32_t Now;


void systick_init()
{
    SystemCoreClockUpdate();

    Now = 0;

    if (SysTick_Config(SystemCoreClock / 1000)) for(;;);

    //usart_printf("\nSystemCoreClock=%u\n",SystemCoreClock);
}

void systick_delay(uint32_t delay_ms)
{
    TimingDelay = delay_ms;
    while(TimingDelay != 0);
}

uint32_t systick_now()
{
    return Now;
}

void SysTick_Handler(void)
{
    if (TimingDelay != 0x00)
        TimingDelay --;
    Now++;
}

void active_delay(uint32_t delay_ms)
{
    uint32_t count = delay_ms * 4000;
    while (count-- > 0)
        asm("nop");
}

