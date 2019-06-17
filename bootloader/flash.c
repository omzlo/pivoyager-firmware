#include "flash.h"
#include <stm32f0xx.h>

void flash_open(void)
{
  /* (1) Wait till no operation is on going */
  /* (2) Check that the Flash is unlocked */
  /* (3) Perform unlock sequence */

  while ((FLASH->SR & FLASH_SR_BSY) != 0);
  if ((FLASH->CR & FLASH_CR_LOCK) != 0) /* (2) */
  {
    FLASH->KEYR = FLASH_FKEY1; /* (3) */
    FLASH->KEYR = FLASH_FKEY2;
  }
}

int flash_read_block(uint32_t flash_addr, uint16_t *data, uint16_t word_count)
{
  while ((FLASH->SR & FLASH_SR_BSY) != 0);
  while (word_count-->0)
  {
    *data++ = *(volatile uint16_t*)(flash_addr);
    flash_addr += 2;
  }
  return 0;
}

int flash_write_block(uint32_t flash_addr, const uint16_t *data, uint16_t word_count)
{
  /* (1) Set the PG bit in the FLASH_CR register to enable programming */
  /* (2) Perform the data write (half-word) at the desired address */
  /* (3) Wait until the BSY bit is reset in the FLASH_SR register */
  /* (4) Check the EOP flag in the FLASH_SR register */
  /* (5) clear it by software by writing it at 1 */
  /* (6) Reset the PG Bit to disable programming */

  FLASH->CR |= FLASH_CR_PG;

  while (word_count-->0)
  {
    *(volatile uint16_t*)(flash_addr) = *data++;
    while ((FLASH->SR & FLASH_SR_BSY) != 0);

    if ((FLASH->SR & FLASH_SR_EOP) != 0)  /* (4) */
    { 
      FLASH->SR |= FLASH_SR_EOP; /* (5) */
    }
    else if ((FLASH->SR & FLASH_SR_PGERR) != 0) /* Check Programming error */
    {      
      FLASH->SR |= FLASH_SR_PGERR; /* Clear it by software by writing EOP at 1*/
      return -1;
    }
    else if ((FLASH->SR & FLASH_SR_WRPERR) != 0) /* Check write protection */
    {      
      FLASH->SR |= FLASH_SR_WRPERR; /* Clear it by software by writing it at 1*/
      return -2;
    }
    else
    {
      return -3;
    }
    flash_addr+=2;
  }
  FLASH->CR &= ~FLASH_CR_PG;
  return 0;
}

int flash_erase_page(uint32_t page_addr)
{
  /* (1) Set the PER bit in the FLASH_CR register to enable page erasing */
  /* (2) Program the FLASH_AR register to select a page to erase */
  /* (3) Set the STRT bit in the FLASH_CR register to start the erasing */
  /* (4) Wait until the BSY bit is reset in the FLASH_SR register */
  /* (5) Check the EOP flag in the FLASH_SR register */
  /* (6) Clear EOP flag by software by writing EOP at 1 */
  /* (7) Reset the PER Bit to disable the page erase */
  FLASH->CR |= FLASH_CR_PER; /* (1) */    
  FLASH->AR =  page_addr; /* (2) */    
  FLASH->CR |= FLASH_CR_STRT; /* (3) */    
  while ((FLASH->SR & FLASH_SR_BSY) != 0) /* (4) */ 
  {
    /* For robust implementation, add here time-out management */
  }  
  if ((FLASH->SR & FLASH_SR_EOP) != 0)  /* (5) */
  {  
    FLASH->SR |= FLASH_SR_EOP; /* (6)*/
  }    
  /* Manage the error cases */
  else if ((FLASH->SR & FLASH_SR_WRPERR) != 0) /* Check Write protection error */
  {
    FLASH->SR |= FLASH_SR_WRPERR; /* Clear the flag by software by writing it at 1*/
    return -1;
  }
  else
  {
    return -3;
  }
  FLASH->CR &= ~FLASH_CR_PER; /* (7) */
  return 0;
}


int flash_start_main_application(void)
{
  uint32_t *VectorTable = (uint32_t *)0x20000000;
  uint32_t vecIndex;

  __disable_irq();

  for(vecIndex = 0; vecIndex < 48; vecIndex++){
    VectorTable[vecIndex] = *(volatile uint32_t*)(FLASH_APP_START + (vecIndex << 2));
  }
  SYSCFG->CFGR1 &= ~(SYSCFG_CFGR1_MEM_MODE);
  SYSCFG->CFGR1 |= (SYSCFG_CFGR1_MEM_MODE_0 | SYSCFG_CFGR1_MEM_MODE_1);

  __enable_irq();

  volatile uint32_t *appBegin = (volatile uint32_t*) FLASH_APP_START;
  uint32_t stack = appBegin[0];
  uint32_t start = appBegin[1];
 
  __asm volatile(
        "msr MSP, %0\n\t"        // set stack
        "bx  %1\n\t"             // start the app
        :
        : "r"(stack), "r"(start)
      );
  return 0;
}
