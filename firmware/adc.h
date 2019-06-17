#ifndef _ADC_H_
#define _ADC_H_

#include <stdint.h>

void adc_init(void);


void adc_acquire(uint16_t *result);

#define VREFINT_CAL (*((uint16_t *)(0x1FFFF7BA)))

#endif
