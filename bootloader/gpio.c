#include "gpio.h"

extern void gpio_enable_port_clock(uint32_t port);

extern void gpio_enable_output(uint32_t gpio);

    extern void gpio_config_speed(uint32_t gpio, uint32_t speed);

    extern void gpio_config_output_type(uint32_t gpio, uint32_t otype);

    extern void gpio_set(uint32_t gpio);

    extern void gpio_clear(uint32_t gpio);
    
    extern void gpio_toggle(uint32_t gpio);

extern void gpio_enable_input(uint32_t gpio);

    extern uint32_t gpio_read(uint32_t gpio);

    extern void gpio_config_pullupdown(uint32_t gpio, uint32_t pupd);

extern void gpio_enable_analog(uint32_t gpio);

extern void gpio_enable_alternate_function(uint32_t gpio, uint32_t af);



