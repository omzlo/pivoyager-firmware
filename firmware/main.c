#include <stm32f0xx.h>
#include "systick.h"
#include "usart.h"
#include "adc.h"
#include "gpio.h"
#include "i2c_slave.h"
#include "rtc.h"

struct _regs_t {
    // 0
    uint8_t MODE;   // either 'N' or 'B'
    uint8_t STAT;   // status
    uint8_t CONF;   // enable pin WD, etc., commit change
    uint8_t BOOT;   // boot reason (reset, wake, button,...)   

    // 4
    uint32_t TIME;
    uint32_t DATE;

    // 12
    uint16_t WATCH;
    uint16_t WAKE;

    // 16
    uint32_t ALARM;
    uint32_t res1;

    // 24
    uint16_t VBAT;
    uint16_t VREF;
    uint16_t VREF_CAL;
    uint16_t res2;
 } REGS, REGS_NEW;

#define STAT_PG         0x01
#define STAT_STAT1      0x02
#define STAT_STAT2      0x04
#define STAT_PG2        0x08
#define STAT_INITS      0x10
#define STAT_ALARM      0x40
#define STAT_BUTTON     0x80

#define CONF_I2C_WD         0x01
#define CONF_PIN_WD         0x02
#define CONF_WAKE_AFTER     0x04
#define CONF_WAKE_ALARM     0x08
#define CONF_WAKE_POWER     0x10
#define CONF_WAKE_BUTTON    0x20
#define CONF_CALENDAR       0x40
#define CONF_ALARM          0x80

static void halt(void) 
{
     usart_printf("Halted.\n");
     for(;;);
}

enum {
    BUTTON_NONE,
    BUTTON_PRESSED,
    BUTTON_MAINTAINED,
    BUTTON_SHORT,
    BUTTON_LONG
};

int button_state = BUTTON_NONE;

static int fetch_button_state(void)
{
    static uint32_t button_change = 0;
    uint32_t button_cur;
    uint32_t now = systick_now();
    
    button_cur = gpio_read(GPIO_IN_BUTTON);

    switch (button_state) {
        case BUTTON_NONE:
            if (button_cur==0)
            {
                button_change = now;
                button_state = BUTTON_PRESSED;
            }
            break;
        case BUTTON_PRESSED:
            if (button_cur==0)
            {
                if (now-button_change>3000)
                {
                    button_state = BUTTON_MAINTAINED;
                }
            }
            else
            {
                if (now-button_change>40) {
                    button_state = BUTTON_NONE;
                    return BUTTON_SHORT;
                }
            }
            break;
        case BUTTON_MAINTAINED:
            if (button_cur!=0)
            {
                button_state = BUTTON_NONE;
                return BUTTON_LONG;
            }
            break;
    }
    return button_state;
}

static uint8_t fetch_status(void)
{
    uint8_t stat = 0;
    if (gpio_read(GPIO_IN_PG))
        stat |= STAT_PG;
    if (gpio_read(GPIO_IN_STAT2))
        stat |= STAT_STAT2;
    if (gpio_read(GPIO_IN_STAT1))
        stat |= STAT_STAT1;
    if (gpio_read(GPIO_IN_PG2))
        stat |= STAT_PG2;
    if ((RTC->ISR & RTC_ISR_INITS)!=0)
        stat |= STAT_INITS;
    if ((RTC->ISR & RTC_ISR_ALRAF)!=0)
        stat |= STAT_ALARM;
    return stat;
}

enum {
    LED_PATTERN_OFF,
    LED_PATTERN_ON,
    LED_PATTERN_SLOW,
    LED_PATTERN_FAST
};


static void update_led(int led, int pattern)
{
    uint32_t now = systick_now();

    switch (pattern) {
        case LED_PATTERN_OFF:
            gpio_clear(led);
            break;
        case LED_PATTERN_ON:
            gpio_set(led);
            break;
        case LED_PATTERN_SLOW:
            if (((now/500)&1)==0)
                gpio_clear(led);
            else
                gpio_set(led);
            break;
        case LED_PATTERN_FAST:
            if (((now/100)&1)==0)
                gpio_clear(led);
            else
                gpio_set(led);
            break; 
    }
}

static void update_datetime(void)
{
    REGS.TIME = rtc_get_time();
    REGS.DATE = rtc_get_date();
}

static void update_adc(void) 
{
    static uint32_t last_adc = 0;
    static int trigger = 0;
    uint32_t now = systick_now();

    if (trigger == 0)
    {
        if ((now-last_adc)>=1000)
        {
            gpio_set(GPIO_OUT_ADC_BAT);
            trigger = 1;
        }
    }
    else
    {
        adc_acquire(&REGS.VBAT);
        last_adc = now;
        gpio_clear(GPIO_OUT_ADC_BAT);
        trigger = 0;
    }
}

static void update_led_patterns(int st_pattern)
{
    
    update_led(GPIO_OUT_LED_ST, st_pattern);

    switch (REGS.STAT & 0x7) {
        case 0:     // NA
            break;
        case 1:     // FAULT
            update_led(GPIO_OUT_LED_PG, LED_PATTERN_FAST);
            update_led(GPIO_OUT_LED_CH, LED_PATTERN_FAST);
            return;
        case 2:     // LBO
            update_led(GPIO_OUT_LED_PG, LED_PATTERN_OFF);
            update_led(GPIO_OUT_LED_CH, LED_PATTERN_FAST);
        case 3:     // Charging
            update_led(GPIO_OUT_LED_PG, LED_PATTERN_ON);
            update_led(GPIO_OUT_LED_CH, LED_PATTERN_SLOW);
            break;
        case 4:     // impossible
            break;
        case 5:     // Charge complete
            update_led(GPIO_OUT_LED_PG, LED_PATTERN_ON);
            update_led(GPIO_OUT_LED_CH, LED_PATTERN_ON);
            break;
        case 6:     // discharging
            update_led(GPIO_OUT_LED_PG, LED_PATTERN_OFF);
            update_led(GPIO_OUT_LED_CH, LED_PATTERN_OFF); 
            break;
        case 7:     // No battery
            update_led(GPIO_OUT_LED_PG, LED_PATTERN_ON);
            update_led(GPIO_OUT_LED_CH, LED_PATTERN_OFF);
            break;
    }
}

static void memzero(void *s, uint32_t len)
{
    uint8_t *c = (uint8_t *)s;

    while (len--) *c++ = 0;
}

static void init(void)
{
    int status;
    uint32_t reset_reason = RCC->CSR; 
    RCC->CSR |= RCC_CSR_RMVF; 

    usart_init(38400);  
    usart_printf("\n*******************************************");
    usart_printf("\n** PiVoyager serial debug interface      **");
    usart_printf("\n*******************************************\n");

    usart_printf("RCC_CSR: %x\n", reset_reason);
    usart_printf("PWR_CSR: %x\n", PWR->CSR);

    usart_printf("Systick init: ");
    systick_init();
    usart_printf("[OK]\n");

    usart_printf("gpio init: ");
    
    gpio_enable_port_clock(PORTA);
    gpio_enable_port_clock(PORTB);

    gpio_enable_input(GPIO_IN_PG);
    gpio_enable_input(GPIO_IN_STAT2);
    gpio_enable_input(GPIO_IN_STAT1);

    gpio_enable_output(GPIO_OUT_EN);
    gpio_set(GPIO_OUT_EN);

    gpio_enable_output(GPIO_OUT_ADC_BAT);
    gpio_clear(GPIO_OUT_ADC_BAT);

    gpio_enable_input(GPIO_IN_WATCHDOG);

    gpio_enable_output(GPIO_OUT_LED_PG);
    gpio_enable_output(GPIO_OUT_LED_CH);
    gpio_enable_output(GPIO_OUT_LED_ST);
    
    gpio_enable_input(GPIO_IN_PG2);
    gpio_enable_input(GPIO_TP2);
    gpio_config_pullupdown(GPIO_TP2, GPIO_PULL_DOWN);

    gpio_enable_input(GPIO_IN_BUTTON);
    usart_printf("[OK]\n");

    usart_printf("ADC init: ");
    gpio_enable_analog(GPIO_ADC_BAT);
    adc_init();
    usart_printf("[OK]\n");

    usart_printf("I2C init: ");

    i2c_slave_init(0x65);

    i2c_set_tx_buffer((uint8_t *)&REGS, sizeof(REGS));
    i2c_set_rx_buffer((uint8_t *)&REGS_NEW, sizeof(REGS_NEW));
    usart_printf("[OK]\n");

    usart_printf("RTC init: ");
    if ((status=rtc_init())<0)
    {
         usart_printf("[Failed] code=%i\n", status);
         halt();
    }
    if (status == 0)
        usart_printf("[OK] calendar already initialized.\n");
    else
        usart_printf("[OK] calendar was reset.\n");

    //PWR->CR = (PWR_CSR_SBF | PWR_CSR_WUF)<<2;
    usart_printf("Rechecking flags.\n");
    usart_printf("PWR_CSR: %x\n", PWR->CSR);


    memzero(&REGS, sizeof(REGS));
    memzero(&REGS_NEW, sizeof(REGS));

    REGS.MODE = 'N';
    REGS.VREF_CAL = *((uint16_t *)(0x1FFFF7BA));

    usart_printf("Init done. Entering main loop.\n");
}

static void process_usart(void)
{
    if (usart_getc()=='d')
    {
        for (int i=0; i<sizeof(REGS); i++)
        {
            if ((i&7)==0) usart_printf("\n");
            usart_printf("%x ", ((uint8_t *)&REGS)[i]);
        }
        usart_printf("\n");
    }
    else
    {
        usart_printf("?\n");
    }
}

static void go_to_standby_mode(void)
{
    gpio_clear(GPIO_OUT_EN);

    //PWR->CR  |= PWR_CR_CWUF;

    /* Wake on button press */
    //PWR->CSR |= PWR_CSR_EWUP2;  
    /* Wake on power good */
    if (!gpio_read(GPIO_IN_PG))
      PWR->CSR |= PWR_CSR_EWUP1;

#if 0
    if ((REGS.CONF & CONF_WAKE_AFTER)!=0)
    {
      //PWR->CR|= PWR_CR_DBP;
      RTC->WPR = 0xCA;
      RTC->WPR = 0x53;
      /* disable wakeup interrupts */
      RTC->CR &= ~RTC_CR_WUTE;
      /* Wait until ready */
      while ((RTC->ISR & RTC_ISR_WUTWF) != RTC_ISR_WUTWF) {}
      /* Set timer */
      RTC->WUTR = REGS.WAKE;
      /* Select 1Hz clock source */
      RTC->CR &= ~7;
      RTC->CR |= RTC_CR_WUCKSEL_2;
      /* enable wakeup interrupt + enable wakeup timer */
      RTC-> CR |= RTC_CR_WUTIE | RTC_CR_WUTE;
      RTC->WPR = 0xFE;
      RTC->WPR = 0x64;
      usart_printf("Setting a %x wakeup\r\n", RTC->WUTR);
    
    }
#endif

    /* Select STANDBY mode */
    PWR->CR |= PWR_CR_PDDS;
    /* Set SLEEPDEEP bit of Cortex-M0 System Control Register */
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    /* See Errata for STM32F072 */ 
    PWR->CR  |= PWR_CR_CWUF;

    /* Request Wait For Interrupt */
    __WFI();
}

int main(void)
{
    // SystemInit() is called before main() from 
    init();

    uint8_t BUTTON_STAT = 0;
    uint32_t led_pattern = LED_PATTERN_ON; 
    uint32_t rx_count = i2c_rx_count();
    uint32_t tx_count = i2c_tx_count();
    uint32_t last_event = 0;

    for(;;)
    {
        uint8_t stat = fetch_status();
        
        switch (fetch_button_state())
        {
            case BUTTON_SHORT:
                BUTTON_STAT = STAT_BUTTON;
                usart_printf("Short press...\n");
                break;
            case BUTTON_MAINTAINED:
                led_pattern = LED_PATTERN_FAST;
                break;
            case BUTTON_LONG:
                led_pattern = LED_PATTERN_ON;
                usart_printf("Long press...\n");

                systick_delay(100);

                usart_printf("Sleeping.");

                go_to_standby_mode();
                break;
        }

        if (i2c_rx_count()!=rx_count)
        {
            rx_count = i2c_rx_count();
            if ((REGS_NEW.STAT & STAT_BUTTON)!=0)
            {
                REGS_NEW.STAT = 0;
                BUTTON_STAT = 0;
            }
            if (REGS_NEW.CONF != REGS.CONF)
            {
                if ((REGS_NEW.CONF & CONF_CALENDAR) != 0) {
                    rtc_disable_write_protection();
                    rtc_enable_calendar_init();
                    rtc_set_time(REGS_NEW.TIME);
                    rtc_set_date(REGS_NEW.DATE);
                    rtc_disable_calendar_init();
                    rtc_enable_write_protection();
                    usart_printf("Date changed\n");
                }
                if ((REGS_NEW.CONF & CONF_ALARM) != 0) {
                    rtc_disable_write_protection();
                    rtc_disable_alarm();
                    rtc_set_alarm(REGS_NEW.ALARM);
                    rtc_enable_alarm();
                    rtc_enable_write_protection();
                    usart_printf("Alarm set\n");
                }
            }
            REGS.CONF = REGS_NEW.CONF & ~(CONF_CALENDAR|CONF_ALARM);
            
            REGS.WATCH = REGS_NEW.WATCH;
            REGS.WAKE = REGS_NEW.WAKE;
            REGS.ALARM = REGS_NEW.ALARM;

            if ((REGS.CONF & CONF_I2C_WD)!=0)
                last_event = systick_now();

            usart_printf("REGS.CONF updated %x <- %x\n", REGS.CONF, REGS_NEW.CONF);
        }
        if (i2c_tx_count()!=tx_count)
        {
            tx_count = i2c_tx_count();
            if ((REGS.CONF & CONF_I2C_WD)!=0)
                last_event = systick_now();
        }

        update_adc();

        REGS.STAT = stat | BUTTON_STAT;

        update_datetime();

        update_led_patterns(led_pattern);

        if ((REGS.CONF & (CONF_I2C_WD | CONF_PIN_WD))!=0)
        {
            if (systick_now()-last_event>(uint32_t)REGS.WATCH*1000)
            {
                go_to_standby_mode();
            }
        }

        if (usart_available())
            process_usart();
    }
}

/*
 * Debug help -- if asserts are enabled, assertion failure
 * from standard peripheral  library ends up here 
 */


#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* Infinite loop */
  /* Use GDB to find out why we're here */
  while (1)
  {
  }
}
#endif

