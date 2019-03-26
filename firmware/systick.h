#ifndef _SYSTICK_H_
#define _SYSTICK_H_
    
void systick_init();

void systick_delay(uint32_t delay_ms);

uint32_t systick_now();

void active_delay(uint32_t delay_ms);

#endif
