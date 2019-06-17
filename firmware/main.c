#include <stm32f0xx.h>
#include "systick.h"
#include "usart.h"
#include "adc.h"
#include "gpio.h"
#include "i2c_slave.h"
#include "rtc.h"
#include "time_conv.h"

typedef struct {
    // 0
    uint8_t MODE;   // either 'N' or 'B'
    uint8_t STAT;   // status
    uint8_t CONF;   // enable pin WD, etc., commit change
    uint8_t PROG;   // schedule an operation   

    // 4
    uint32_t TIME;
    uint32_t DATE;
    
    // 12
    uint32_t SET_TIME;
    uint32_t SET_DATE;

    // 20
    uint16_t WATCH;
    uint16_t WAKE;

    // 24
    uint32_t ALARM;
    uint16_t BOOT;
    uint16_t res1;

    // 32
    uint16_t VBAT;
    uint16_t VREF;
    uint16_t VREF_CAL;
    uint16_t LBO_TIMER;

    // Total size: 40 bytes
} regs_t;

static regs_t REGS;
    
static uint8_t SHADOW_CONF = 0;

static const regs_t REGS_MASK = {
  .MODE     = 0x00,
  .STAT     = 0x00,
  .CONF     = 0xFF,
  .PROG     = 0xFF,
  .TIME     = 0x00000000,
  .DATE     = 0x00000000,
  .SET_TIME = 0xFFFFFFFF,
  .SET_DATE = 0xFFFFFFFF,
  .WATCH    = 0xFFFF,
  .WAKE     = 0xFFFF,
  .ALARM    = 0xFFFFFFFF,
  .BOOT     = 0x0000,
  .res1     = 0xFFFF,
  .VBAT     = 0x0000,
  .VREF     = 0x0000,
  .VREF_CAL = 0xFFFF,
  .LBO_TIMER = 0xFFFF
};

#define STAT_PG         0x01
#define STAT_STAT1      0x02
#define STAT_STAT2      0x04
#define STAT_PG2        0x08
#define STAT_INITS      0x10      // INITialization Status of RTC (0 -> uninitialized)
#define STAT_ALARM      0x40
#define STAT_BUTTON     0x80

#define CONF_I2C_WD         0x01
#define CONF_PIN_WD         0x02
#define CONF_WAKE_AFTER     0x04
#define CONF_WAKE_ALARM     0x08
#define CONF_WAKE_POWER     0x10
#define CONF_WAKE_BUTTON    0x20
#define CONF_LBO_SHUTDOWN   0x80


#define PROG_CLEAR_ALARM    0x10
#define PROG_CLEAR_BUTTON   0x20
#define PROG_CALENDAR       0x40
#define PROG_ALARM          0x80

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

#ifdef INVERTED_LOGIC
  #define PRESSED(x) ((x)==0)
#else
  #define PRESSED(x) ((x)!=0)
#endif

static inline int fetch_button_state(void)
{
    static uint32_t button_change = 0;
    uint32_t button_cur;
    uint32_t now = systick_now();
    
    button_cur = gpio_read(GPIO_IN_BUTTON);

    switch (button_state) {
        case BUTTON_NONE:
            if (PRESSED(button_cur))
            {
                button_change = now;
                button_state = BUTTON_PRESSED;
            }
            break;
        case BUTTON_PRESSED:
            if (PRESSED(button_cur))
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
            if (!PRESSED(button_cur))
            {
                button_state = BUTTON_NONE;
                return BUTTON_LONG;
            }
            break;
    }
    return button_state;
}

static inline uint8_t fetch_status(void)
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


static inline void update_led(int led, int pattern)
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

static inline void update_datetime(void)
{
    REGS.TIME = rtc_get_time();
    REGS.DATE = rtc_get_date();
}

static inline void update_adc(void) 
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

    // STAT2 | STAT1 | PG
    // ------+-------+--------------------------
    // 0     | 0     | 0    : NA
    // 0     | 0     | 1    : FAULT
    // 0     | 1     | 0    : impossible
    // 0     | 1     | 1    : Charge complete
    // 1     | 0     | 0    : LBO
    // 1     | 0     | 1    : Charging
    // 1     | 1     | 0    : Discharging
    // 1     | 1     | 1    : No battery

    switch (REGS.STAT & 0x7) {
        case 0:     // NA
            break;
        case 1:     // FAULT
        case 2:     // impossible
            update_led(GPIO_OUT_LED_PG, LED_PATTERN_FAST);
            update_led(GPIO_OUT_LED_CH, LED_PATTERN_FAST);
            return;
        case 3:     // Charge complete
            update_led(GPIO_OUT_LED_PG, LED_PATTERN_ON);
            update_led(GPIO_OUT_LED_CH, LED_PATTERN_ON);
            break;
        case 4:     // LBO
            update_led(GPIO_OUT_LED_PG, LED_PATTERN_OFF);
            update_led(GPIO_OUT_LED_CH, LED_PATTERN_FAST);
           break;
        case 5:     // Charging
            update_led(GPIO_OUT_LED_PG, LED_PATTERN_ON);
            update_led(GPIO_OUT_LED_CH, LED_PATTERN_SLOW);
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

const char *screen[10]={
  "",
  "  ____  ___     __                                ",
  " |  _ \\(_) \\   / /__  _   _  __ _  __ _  ___ _ __ ",
  " | |_) | |\\ \\ / / _ \\| | | |/ _` |/ _` |/ _ \\ '__|",
  " |  __/| | \\ V / (_) | |_| | (_| | (_| |  __/ |   ",
  " |_|   |_|  \\_/ \\___/ \\__, |\\__,_|\\__, |\\___|_|   ",
  "                      |___/       |___/           ",
  "",
  "              Serial debug interface by OMZLO.COM ",
  ""
};

static void init(void)
{
    int status;
    uint32_t pwr_csr = PWR->CSR;
    
    RCC->CSR |= RCC_CSR_RMVF; 

    usart_init(38400);  
    for (int i=0; i<10;i++)
      usart_printf("%s\n",screen[i]);

    /* PWR CSR */
    usart_printf("PWR_CSR: 0x%x\n", pwr_csr);

    /* SYSTICK INIT */
    usart_printf("Systick init: ");
    systick_init();
    usart_printf("[OK]\n");

    /* GPIO INIT */
    usart_printf("gpio init: ");
    
    gpio_enable_port_clock(PORTA);
    gpio_enable_port_clock(PORTB);

    gpio_enable_input(GPIO_IN_PG);
    gpio_enable_input(GPIO_IN_STAT2);
    gpio_enable_input(GPIO_IN_STAT1);

    gpio_enable_output(GPIO_OUT_EN);

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

    /* BUTTON STATUS */
    usart_printf("Check button status: ");
    while (PRESSED(gpio_read(GPIO_IN_BUTTON)));
    usart_printf("[OK]\n");

    /* ADC INIT */
    usart_printf("ADC init: ");
    gpio_enable_analog(GPIO_ADC_BAT);
    adc_init();
    usart_printf("[OK]\n");

    /* I2C INIT */
    usart_printf("I2C init: ");

    i2c_slave_init(0x65);

    i2c_set_buffer((uint8_t *)&REGS, (const uint8_t *)&REGS_MASK, sizeof(REGS));
    usart_printf("[OK]\n");

    /* RTC INIT */
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

    memzero(&REGS, sizeof(REGS));

    REGS.MODE = 'N';
    REGS.VREF_CAL = *((uint16_t *)(0x1FFFF7BA));
    REGS.CONF = SHADOW_CONF = (CONF_WAKE_BUTTON | CONF_LBO_SHUTDOWN);
    REGS.BOOT = pwr_csr;
    REGS.LBO_TIMER = 60;

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
  PWR->CR  |= PWR_CR_CWUF;

  /* Power down raspberry-pi */
  gpio_clear(GPIO_OUT_EN);

  /* Wake on button press? */
  if ((SHADOW_CONF & CONF_WAKE_BUTTON)!=0)
  {
    PWR->CSR |= PWR_CSR_EWUP2;
    usart_printf("Wake on button enabled (button=%i)\n",gpio_read(GPIO_IN_BUTTON));
  }

  /* Wake on power good? */
  if ((SHADOW_CONF & CONF_WAKE_POWER)!=0 && !gpio_read(GPIO_IN_PG))
  {
    PWR->CSR |= PWR_CSR_EWUP1;
  }

  /* Wake on timer? */
  if ((SHADOW_CONF & CONF_WAKE_AFTER)!=0)
  {
    uint32_t time = rtc_get_time();
    uint32_t date = rtc_get_date();
    uint32_t alarm, seconds;
    seconds = calendar_to_seconds(date, time);
    usart_printf("Now:  date=0x%x time=0x%x sec=%u delta=%u.\n", date, time, seconds, REGS.WAKE);
    seconds += REGS.WAKE;
    seconds_to_calendar(seconds, &date, &time);
    alarm = calendar_to_alarm(date, time);
    usart_printf("Wake: date=0x%x time=0x%x alarm=0x%x.\n", date, time, alarm);

    rtc_disable_write_protection();
    rtc_disable_alarm();
    rtc_set_alarm(alarm);
    rtc_enable_alarm();
    RTC->CR |= RTC_CR_ALRAIE;
    rtc_enable_write_protection();
  }

  /* Wake on alarm? */
  if ((SHADOW_CONF & CONF_WAKE_ALARM)!=0 && (RTC->ISR & RTC_ISR_ALRAF)==0)
  {
    rtc_disable_write_protection();
    RTC->CR |= RTC_CR_ALRAIE;
    rtc_enable_write_protection();
  }

  /* Select STANDBY mode */
  PWR->CR |= PWR_CR_PDDS;
  /* Set SLEEPDEEP bit of Cortex-M0 System Control Register */
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

  /* See Errata for STM32F0xx */ 
  PWR->CR  |= PWR_CR_CWUF;

  /* Request Wait For Interrupt */
  __WFI();
}

int main(void)
{
    // SystemInit() is called before main() from startup_stm32f0xx.c 
    init();

    uint8_t BUTTON_STAT = 0;
    uint32_t led_pattern = LED_PATTERN_ON; 
    uint32_t rx_count = i2c_rx_count();
    uint32_t tx_count = i2c_tx_count();
    uint32_t last_event = 0;
    uint8_t stat;
    uint32_t now = 0;
    uint32_t lbo_start;
    int lbo = 0;


    for (;;)
    {
      REGS.STAT = fetch_status();
      
      if ((REGS.STAT&7)!=STAT_STAT2)
        break;

      update_led_patterns(0);

      if ((now&7)==STAT_STAT2) {
        usart_printf("Battery too low to start.\n");
      }

      systick_delay(500);
      now++;
    }

    gpio_set(GPIO_OUT_EN);

    for(;;)
    {
        stat = fetch_status();
        now = systick_now();
        
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

            if (SHADOW_CONF != REGS.CONF)
            {
                // synchronize here
                SHADOW_CONF = REGS.CONF;
            }

            if (REGS.PROG != 0)
            {
                if ((REGS.PROG & PROG_CLEAR_BUTTON) != 0) {
                    BUTTON_STAT = 0;
                }
                if ((REGS.PROG & PROG_CLEAR_ALARM) != 0) {
                    RTC->ISR &= ~RTC_ISR_ALRAF;
                }
                if ((REGS.PROG & PROG_CALENDAR) != 0) {
                    usart_printf("Calendar update: ");
                    rtc_disable_write_protection();
                    if (rtc_enable_calendar_init()==0) {
                      rtc_set_time(REGS.SET_TIME);
                      rtc_set_date(REGS.SET_DATE);
                      rtc_disable_calendar_init();
                      usart_printf("[OK]\n");
                    } else {
                      usart_printf("[FAIL]\n");
                    }
                    rtc_enable_write_protection();
                }
                if ((REGS.PROG & PROG_ALARM) != 0) {
                    usart_printf("Alarm update: ");
                    rtc_disable_write_protection();
                    rtc_disable_alarm();
                    rtc_set_alarm(REGS.ALARM);
                    rtc_enable_alarm();
                    rtc_enable_write_protection();
                    usart_printf("[OK]\n");
                }
                __disable_irq();
                REGS.PROG = 0;
                __enable_irq(); 
            }

            if ((SHADOW_CONF & CONF_I2C_WD)!=0)
                last_event = now;
        }

        if (i2c_tx_count()!=tx_count)
        {
            tx_count = i2c_tx_count();
            if ((SHADOW_CONF & CONF_I2C_WD)!=0)
                last_event = now;
        }

        if ((SHADOW_CONF & CONF_PIN_WD)!=0 && gpio_read(GPIO_IN_WATCHDOG)!=0)
        {
            last_event = now;
        }

        update_adc();

        REGS.STAT = stat | BUTTON_STAT;

        update_datetime();

        update_led_patterns(led_pattern);

        if ((SHADOW_CONF & (CONF_I2C_WD | CONF_PIN_WD))!=0)
        {
            if (now-last_event>(uint32_t)REGS.WATCH*1000)
            {
              usart_printf("Going on standby because of watchdog: now=%u last=%u watch=%u\n", now, last_event, REGS.WATCH);    
              go_to_standby_mode();
            }
        }
        
        if ((SHADOW_CONF & CONF_LBO_SHUTDOWN)!=0)
        {
          if (lbo==0)
          {
            if ((stat&7)==STAT_STAT2)
            {
              lbo = 1;
              lbo_start = now;
              usart_printf("Low battery with programmed shutdown in %us\n", REGS.LBO_TIMER);
            }
          }
          else
          {
            if ((stat&7)!=STAT_STAT2)
            {
              lbo = 0;
              usart_printf("Low battery status ended.\n");
            }
            else
            {
              if (now-lbo_start>(uint32_t)REGS.LBO_TIMER*1000)
              {
                usart_printf("Going on standby because VBAT (%u) too low for %u seconds.\n", REGS.VBAT, REGS.LBO_TIMER);
                go_to_standby_mode(); 
              }
            }
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

