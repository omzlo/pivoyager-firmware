#ifndef _GPIO_H_
#define _GPIO_H_

#include <stdint.h>
#include <stm32f0xx.h>

#define INLINE __attribute__((always_inline)) inline

#define HIGH 1
#define LOW  0

#define GPIO_PUSH_PULL 0
#define GPIO_OPEN_DRAIN 1

#define GPIO_MODE_IN    0
#define GPIO_MODE_OUT   1
#define GPIO_MODE_AF    2
#define GPIO_MODE_AN    3

#define GPIO_SPEED_LOW      0
#define GPIO_SPEED_MEDIUM   1
#define GPIO_SPEED_HIGH     3

#define PORTA 0
#define PORTB 1
#define PORTC 2

#define GPIO_A(x) (x+32*0)
#define GPIO_B(x) (x+32*1)
#define GPIO_C(x) (x+32*2)
#define GPIO_D(x) (x+32*3)
#define GPIO_E(x) (x+32*4)
#define GPIO_F(x) (x+32*5)


#define PORT(gpio) ((GPIO_TypeDef *)(((gpio>>5)*0x0400U)+AHB2PERIPH_BASE))
#define PIN(gpio) ((gpio)&0x1F)

INLINE void gpio_enable_port_clock(uint32_t port)
{
    switch (port) {
        case 0:
            RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
        case 1:
            RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
        case 2:
            RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    }
}

INLINE void gpio_enable_output(uint32_t gpio)
{
    PORT(gpio)->MODER &= ~(0x3 << (PIN(gpio)<<1));
    PORT(gpio)->MODER |= GPIO_MODE_OUT << (PIN(gpio)<<1); 
}

INLINE void gpio_config_speed(uint32_t gpio, uint32_t speed)
{
    PORT(gpio)->OSPEEDR &= ~(0x3 << (PIN(gpio)<<1));
    PORT(gpio)->OSPEEDR |= speed << (PIN(gpio)<<1);
}

INLINE void gpio_config_output_type(uint32_t gpio, uint32_t otype)
{
    PORT(gpio)->OTYPER &= ~(1<<(PIN(gpio)));
    PORT(gpio)->OTYPER |= (otype<<PIN(gpio));
}

INLINE void gpio_set(uint32_t gpio) 
{
    PORT(gpio)->BSRR = (1<<PIN(gpio));
}

INLINE void gpio_clear(uint32_t gpio) 
{
    PORT(gpio)->BRR = (1<<PIN(gpio));
}

INLINE void gpio_toggle(uint32_t gpio) 
{
    PORT(gpio)->ODR ^= (1<<PIN(gpio));
}

#define GPIO_PULL_NONE  0
#define GPIO_PULL_UP    1
#define GPIO_PULL_DOWN  2

INLINE void gpio_config_pullupdown(uint32_t gpio, uint32_t pupd)
{
    PORT(gpio)->PUPDR &= ~(0x3 << (PIN(gpio)<<1)); 
    PORT(gpio)->PUPDR |= pupd << (PIN(gpio)<<1); 
}

INLINE void gpio_enable_input(uint32_t gpio)
{
    PORT(gpio)->MODER &= ~(0x3 << (PIN(gpio)<<1)); 
    // this clears the right bits, no need to set then to 0 next
}

INLINE uint32_t gpio_read(uint32_t gpio)
{
    return (PORT(gpio)->IDR >> PIN(gpio)) & 1;
}

INLINE void gpio_enable_analog(uint32_t gpio)
{
    PORT(gpio)->MODER &= ~(0x3 << (PIN(gpio)<<1));
    PORT(gpio)->MODER |= GPIO_MODE_AN << (PIN(gpio)<<1);
}

INLINE void gpio_enable_alternate_function(uint32_t gpio, uint32_t af)
{
    PORT(gpio)->MODER &= ~(0x3 << (PIN(gpio)<<1)); 
    PORT(gpio)->MODER |= GPIO_MODE_AF << (PIN(gpio)<<1); 
    
    PORT(gpio)->AFR[PIN(gpio) >> 3] &= ~(0xF<<((PIN(gpio) & 0x7) << 2)); 
    PORT(gpio)->AFR[PIN(gpio) >> 3] |= af << ((PIN(gpio) & 0x7) << 2); 
}

// Port A
#define GPIO_IN_PG          GPIO_A(0)
#define GPIO_IN_STAT2       GPIO_A(2)
#define GPIO_IN_STAT1       GPIO_A(3)
#define GPIO_OUT_EN         GPIO_A(4)
#define GPIO_OUT_ADC_BAT    GPIO_A(5)
#define GPIO_ADC_BAT        GPIO_A(6)
#define GPIO_IN_WATCHDOG    GPIO_A(7)
#define GPIO_USART_TX       GPIO_A(9)
#define GPIO_USART_RX       GPIO_A(10)
// Port B
#define GPIO_OUT_LED_PG     GPIO_B(0)
#define GPIO_OUT_LED_CH     GPIO_B(1)
#define GPIO_OUT_LED_ST     GPIO_B(2)
#define GPIO_I2C_SCL        GPIO_B(8)
#define GPIO_I2C_SDA        GPIO_B(9)
#define GPIO_IN_PG2         GPIO_B(14)
#define GPIO_TP2            GPIO_B(15)
// PORT C
#define GPIO_IN_BUTTON      GPIO_C(13)


#endif
