#include "adc.h"
#include "stm32f0xx.h"


/*
 */

#define NUMBER_OF_ADC_INPUTS 2

void adc_init(void)
{
    /*** SET CLOCK FOR ADC ***/
    /* (1) Enable the peripheral clock of the ADC */
    /* (2) Start HSI14 RC oscillator */ 
    /* (3) Wait HSI14 is ready */
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN; /* (1) */
    RCC->CR2 |= RCC_CR2_HSI14ON; /* (2) */
    while ((RCC->CR2 & RCC_CR2_HSI14RDY) == 0) /* (3) */
    {
        /* For robust implementation, add here time-out management */
    }  

    /*** CALIBRATE ADC ***/
    /* (1) Ensure that ADEN = 0 */
    /* (2) Clear ADEN */ 
    /* (3) Launch the calibration by setting ADCAL */
    /* (4) Wait until ADCAL=0 */
    if ((ADC1->CR & ADC_CR_ADEN) != 0) /* (1) */
    {
        ADC1->CR &= (uint32_t)(~ADC_CR_ADEN);  /* (2) */  
    }
    ADC1->CR |= ADC_CR_ADCAL; /* (3) */
    while ((ADC1->CR & ADC_CR_ADCAL) != 0) /* (4) */
    {
        /* For robust implementation, add here time-out management */
    }  


    /*** CONFIGURE GPIO PINS FOR ADC ***/
    //RCC->AHBENR |= RCC_AHBENR_GPIOAEN;  // Enabe clock GPIOA
    //GPIOA->MODER |= GPIO_MODER_MODER2;  // Analog mode on pin 2 (PA2)
    //GPIOA->MODER |= GPIO_MODER_MODER3;  // Analog mode on pin 3 (PA3)


    /*** ENABLE ADC ***/
    do 
    {
        /* For robust implementation, add here time-out management */
		ADC1->CR |= ADC_CR_ADEN; /* (1) */
    } while ((ADC1->ISR & ADC_ISR_ADRDY) == 0) /* (2) */;

    /*** CONFIGURE CHANNELS FOR ADC ***/
    ADC1->CFGR2 &= ~ADC_CFGR2_CKMODE;  // Select HSI14 by writing 00 in CKMODE (reset value)
    //ADC1->CFGR1 |= ADC_CFGR1_EXTEN_0 // 2^0 -> on rising edge (EXTEN=01) 
    //    | ADC_CFGR1_EXTSEL_1         // 2^1 -> 010 
    //    | ADC_CFGR1_EXTSEL_0         // 2^0 -> 001 
    //                                 //     =  011 -> *** TRG3, source is TIM3_TRGO *** trigger on timer3
    //    ;                            // Setting ADC_CFGR1_SCANDIR would result in channels scanning from top to bottom
    ADC1->CHSELR = ADC_CHSELR_CHSEL6    // Select ADC_IN6
        | ADC_CHSELR_CHSEL17            // Select ADC_IN17
        ;
    ADC1->SMPR |= ADC_SMPR_SMP_0 | ADC_SMPR_SMP_1 | ADC_SMPR_SMP_2; // 111 is 239.5 ADC clk

    ADC->CCR |= ADC_CCR_VREFEN;     // Enable internal voltage reference a.k.a. ADC_IN17
}

void adc_acquire(uint16_t *result)
{
    ADC1->CR |= ADC_CR_ADSTART; 
    for (int i=0; i<NUMBER_OF_ADC_INPUTS;i++)
    {
        while ((ADC1->ISR & ADC_ISR_EOC) == 0)
        {
            /* Wait */
        }
        result[i] = ADC1->DR;
    }
    
}
