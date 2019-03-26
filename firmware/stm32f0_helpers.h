#ifndef _STM32F0_HELPERS_H_

#define GPIO_PUSH_PULL 0
#define GPIO_OPEN_DRAIN 1

#define GPIO_MODE_IN    0
#define GPIO_MODE_OUT   1
#define GPIO_MODE_AF    2
#define GPIO_MODE_AN    3

#define GPIO_SPEED_LOW      0
#define GPIO_SPEED_MEDIUM   1
#define GPIO_SPEED_HIGH     3

#define GPIO_CONFIGURE_OUTPUT(gpiox, pin, speed, opendrain) \
    do { \
        gpiox->MODER &= ~(0x3 << ((uint32_t)pin<<1)); \
        gpiox->MODER |= GPIO_MODE_OUT << ((uint32_t)pin<<1); \
        gpiox->OSPEEDR &= ~(0x3 << ((uint32_t)pin<<1)); \
        gpiox->OSPEEDR |= speed << ((uint32_t)pin<<1); \
        gpiox->OTYPER &= ~((uint32_t)1<<pin); \
        gpiox->OTYPER |= ((uint32_t)opendrain<<(uint32_t)pin); \
    } while(0)

#define GPIO_CONFIGURE_INPUT(gpiox, pin) \
    do { \
        gpiox->MODER &= ~(0x3 << ((uint32_t)pin<<1)); \
    } while(0)

#define GPIO_AF_0   0
#define GPIO_AF_1   1
#define GPIO_AF_2   2
#define GPIO_AF_3   3
#define GPIO_AF_4   4
#define GPIO_AF_5   5
#define GPIO_AF_6   6
#define GPIO_AF_7   7

#define GPIO_CONFIGURE_ALTERNATE_FUNCTION(gpiox, pin, af) \
    do { \
        gpiox->MODER &= ~(0x3 << ((uint32_t)pin<<1)); \
        gpiox->MODER |= GPIO_MODE_AF << ((uint32_t)pin<<1); \
        gpiox->AFR[pin >> 3] &= ~(0xF<<(((uint32_t)pin & 0x7) << 2)); \
        gpiox->AFR[pin >> 3] |= (uint32_t)af << (((uint32_t)pin & 0x7) << 2); \
    } while(0)

#define GPIO_PULL_NONE  0
#define GPIO_PULL_UP    1
#define GPIO_PULL_DOWN  2


#define GPIO_CONFIGURE_PULL_UP_DOWN(gpiox, pin, pupd) \
    do { \
        gpiox->PUPDR &= ~(0x3 << ((uint32_t)pin<<1)); \
        gpiox->PUPDR |= pupd << ((uint32_t)pin<<1); \
    } while(0)

#endif
