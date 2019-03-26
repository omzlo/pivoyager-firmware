#include "rtc.h"
#include <stm32f0xx.h>
#include "gpio.h"

extern inline uint8_t TO_BCD(uint8_t b);
extern inline uint8_t FROM_BCD(uint8_t b);

void rtc_disable_write_protection(void)
{
    RTC->WPR = 0xCA;
    RTC->WPR = 0x53;
}

void rtc_enable_write_protection(void)
{
    RTC->WPR = 0XFF;
}

uint32_t rtc_read_backup_register(uint32_t addr)
{
    uint32_t *regs = (uint32_t *)(&(RTC->BKP0R));
    return regs[addr];
}

void rtc_write_backup_register(uint32_t addr, uint32_t value)
{
    uint32_t *regs = (uint32_t *)(&(RTC->BKP0R));
    regs[addr] = value;
}

time_t bcd_time(uint32_t hours, uint32_t minutes, uint32_t seconds)
{
    return (hours<<16) | (minutes<<8) | (seconds);
}

date_t bcd_date(uint32_t weekday, uint32_t day, uint32_t month, uint32_t year)
{
    return (year<<16) | (weekday<<13) | (month<<8) | (day);
}

void rtc_set_time(time_t tm)
{
    uint32_t reg = RTC->TR & 0xFF808080U;
    RTC->TR = reg | tm;
}

time_t rtc_get_time(void)
{
    return RTC->TR & ~0xFF808080U;
}

void rtc_set_date(date_t dt)
{
    uint32_t reg = RTC->DR & 0xFF0000C0U; // preserve reserved bits (not sure necessary)
    RTC->DR = reg | dt;
}

date_t rtc_get_date(void)
{
    return RTC->DR & ~0xFF0000C0U;
}

void rtc_enable_calendar_init(void) 
{
  RTC->ISR |= RTC_ISR_INIT;
  while ((RTC->ISR & RTC_ISR_INITF) != RTC_ISR_INITF) {}
}

void rtc_disable_calendar_init(void)
{
  RTC->ISR &=~ RTC_ISR_INIT;
}

void rtc_set_alarm(uint32_t alarm)
{
  RTC->ALRMAR = alarm;
}

void rtc_enable_alarm(void)
{
  RTC->CR |= RTC_CR_ALRAE;
}

void rtc_disable_alarm(void)
{
  RTC->CR &= ~(RTC_CR_ALRAE);
  while ((RTC->ISR & RTC_ISR_ALRAWF) != RTC_ISR_ALRAWF);
}

int rtc_init(void)
{
    int32_t timeout;

    // enable the power 
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;

    // Enable access to backup domain
    // In reset state, the RTC and backup registers are protected against parasitic write access. This
    // bit must be set to enable write access to these registers.
    PWR->CR |= PWR_CR_DBP;

    // Init the clock value if it has not been initialized
    //if ((RTC->ISR & RTC_ISR_INITS) != RTC_ISR_INITS)
    if ((PWR->CSR & PWR_CSR_SBF)==0)
    {
        // Reset RTC control domain register (Backup Domain Register).
        RCC->BDCR |= RCC_BDCR_BDRST;
        // Stop reset 
        RCC->BDCR &= ~RCC_BDCR_BDRST;

        // Set LSE drive capability to low (00).
        RCC->BDCR &= ~(RCC_BDCR_LSEDRV_0 | RCC_BDCR_LSEDRV_1);
        //RCC->BDCR |= RCC_BDCR_LSEDRV_0 | RCC_BDCR_LSEDRV_1;

        // Enable LSE
        RCC->BDCR |= RCC_BDCR_LSEON;

        // Wait until LSE is ready
        // LSERDY goes high when ready
        timeout = 1000000;
        while ((RCC->BDCR & RCC_BDCR_LSERDY) != RCC_BDCR_LSERDY)
        {
            if (--timeout==0) return -1;
        }

        // Set clock source to low speed external osc. at 32.768Hz
        RCC->BDCR = (RCC->BDCR & ~RCC_BDCR_RTCSEL) | RCC_BDCR_RTCSEL_LSE;

        // Enable the RTC
        RCC->BDCR |= RCC_BDCR_RTCEN;

        rtc_disable_write_protection();

        // Enable RTC init phase
        RTC->ISR |= RTC_ISR_INIT; 

        // Wait for init phase to complete
        timeout = 1000000;
        while ((RTC->ISR & RTC_ISR_INITF) != RTC_ISR_INITF)
        {
            if (--timeout==0) return -2;
        }

        // Set clock presacler to asynch = 127 and sync = 255, i.e. div 32767 
        RTC->PRER = 0x007F00FF;

        // Set hour format to 24h
        RTC->CR &= ~RTC_CR_FMT;

        // Set BCD time to 0
        //rtc_set_time(bcd_time(0,0,0));

        // Set BCD date to Jan 1, 01
        //rtc_set_date(bcd_date(1, 1, 1, 1));

        // Diable RTC init phase
        RTC->ISR &= ~RTC_ISR_INIT; 

        rtc_enable_write_protection();
        return 1;
    }
    else
    {
        PWR->CR = (PWR_CR_CSBF | PWR_CR_CWUF);
    }
    return 0;
}
