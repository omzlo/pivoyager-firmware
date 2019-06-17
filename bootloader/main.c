#include <stm32f0xx.h>
#include <stdint.h>
#include "flash.h"
#include "gpio.h"
#include "i2c_slave.h"
#include "usart.h"

typedef struct {
  uint8_t MODE;   // 'B' for bootloader
  uint8_t VERSION;
  int8_t ERR;   // errors are reported here
  uint8_t PROG;
  uint32_t MCUID;
  uint32_t ADDR;
  uint16_t DATA[32];
} regs_t;

static regs_t REGS;

enum { 
  PROG_NONE       = 0,
  PROG_ERASE_PAGE = 1,
  PROG_READ       = 2,
  PROG_WRITE      = 3,
  PROG_EXIT       = 4
};

static __IO uint32_t Now;

void systick_init()
{
    SystemCoreClockUpdate();

    Now = 0;

    if (SysTick_Config(SystemCoreClock / 10)) for(;;);
}

void SysTick_Handler(void)
{
    Now++;
}

void handle_leds() 
{
  static uint32_t last = 0;
  uint32_t cur = ((Now/3)%3);

  if (last!=cur) {
    switch (cur) {
      case 0:
        gpio_clear(GPIO_OUT_LED_ST);  
        gpio_set(GPIO_OUT_LED_PG);
        break; 
      case 1:  
        gpio_clear(GPIO_OUT_LED_PG);
        gpio_set(GPIO_OUT_LED_CH);
        break;
      case 2:  
        gpio_clear(GPIO_OUT_LED_CH);
        gpio_set(GPIO_OUT_LED_ST);
        break;
    }
    last = cur;
  }
}

static int8_t validate_address(void)
{
  /* TODO: actually we should probably substract 64 to the end address. */
  if ((REGS.ADDR<FLASH_APP_START) || (REGS.ADDR>FLASH_APP_END)) {
    return -4;
  }
  return 0;
}

int main(void)
{
  uint32_t last_i2c = 0;
  uint32_t next_i2c;

  gpio_enable_port_clock(PORTB);
  gpio_enable_input(GPIO_IN_BUTTON);

  /************************************************************************ 
   * We only start the bootloader if this is a cold reset AND the button is 
   * pressed. Otherwise, jump to app.
   */
  if ((RCC->CSR & RCC_CSR_PORRSTF)==0 || gpio_read(GPIO_IN_BUTTON)==0) {
    flash_start_main_application();
  }

  gpio_enable_port_clock(PORTB);
  gpio_enable_output(GPIO_OUT_LED_PG);
  gpio_enable_output(GPIO_OUT_LED_CH);
  gpio_enable_output(GPIO_OUT_LED_ST);

  systick_init();

  REGS.MODE = 'B';
  REGS.ERR = 0;
  REGS.PROG = 0;
  REGS.VERSION = 0;
  REGS.MCUID = DBGMCU->IDCODE;
  REGS.ADDR = FLASH_APP_START;
  for (int i=0; i<32; i++) REGS.DATA[i]=0;

  //usart_init(38400);
  //usart_printf("Bootloader\r\n");

  i2c_slave_init(0x65);
  i2c_set_buffer((uint8_t *)&REGS, sizeof(REGS));

  flash_open();

  for (;;)
  {
      next_i2c = i2c_rx_count();

      if (last_i2c != next_i2c)
      {
        last_i2c = next_i2c;

        if (REGS.PROG != 0) {
          //usart_printf("PROG=%x\r\n",REGS.PROG);
          switch (REGS.PROG) {
            case PROG_ERASE_PAGE:
              if ((REGS.ERR = validate_address()) == 0)
              {
                REGS.ERR = flash_erase_page(REGS.ADDR);
              }
              break;
            case PROG_READ:
              if ((REGS.ERR = validate_address()) == 0)
              {
                REGS.ERR = flash_read_block(REGS.ADDR, REGS.DATA, 32);
              }
              REGS.ADDR += 64;
              break;
            case PROG_WRITE:
              if ((REGS.ERR = validate_address()) == 0)
              {
                REGS.ERR = flash_write_block(REGS.ADDR, REGS.DATA, 32);
              }
              REGS.ADDR += 64;
              break;
            case PROG_EXIT:
              NVIC_SystemReset();
              break;
            default:
              REGS.ERR = -100;
          }
          //usart_printf("DONE with %x\r\n",REGS.PROG);
          REGS.PROG = 0;
        }
      } 
      else
      {
        handle_leds();
      }
  }
  return 0;
}
