#include <stm32f0xx.h>
#include "i2c_slave.h"
#include "gpio.h"

/* SCL on PB8, AltFunc 1
 * SDA on PB9, AltFunc 1
 *
 */

uint8_t *i2c_buf;
uint8_t i2c_buf_len;
volatile uint32_t i2c_buf_rx_count;
volatile uint32_t i2c_buf_tx_count;

uint32_t i2c_op;
uint32_t i2c_reg;

#define I2C_OP_ADDR     0
#define I2C_OP_TX       1
#define I2C_OP_RX       2

void i2c_slave_init(uint8_t i2c_addr)
{
    gpio_enable_port_clock(PORTB);

    // the following 3 lines may be unnecessary
    gpio_enable_output(GPIO_I2C_SCL);
    gpio_config_speed(GPIO_I2C_SCL, GPIO_SPEED_HIGH);
    gpio_config_pullupdown(GPIO_I2C_SCL, GPIO_PULL_UP);

    gpio_config_output_type(GPIO_I2C_SCL, GPIO_OPEN_DRAIN);
    gpio_enable_alternate_function(GPIO_I2C_SCL, 1);

    // the following 3 lines may be unnecessary
    gpio_enable_output(GPIO_I2C_SDA);
    gpio_config_speed(GPIO_I2C_SDA, GPIO_SPEED_HIGH);
    gpio_config_pullupdown(GPIO_I2C_SDA, GPIO_PULL_UP);

    gpio_config_output_type(GPIO_I2C_SDA, GPIO_OPEN_DRAIN);
    gpio_enable_alternate_function(GPIO_I2C_SDA, 1);

    // Enable peripheral clock for I2C1
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    // TODO: what about setting sys clock as source for i2c clock ?
    // RCC->CFGR3 |= RCC_CFGR3_I2C1SW;

    // I2C time reg:
    // 0x2010 091A -> 
    //      PREDC = 2
    //      SCLDEL = 1
    //      SDADEL = 0
    //      rest is ignored in slave mode.
    I2C1->TIMINGR = 0x2010091AU;
    I2C1->CR1 = I2C_CR1_RXIE    // -> Enable RECV interrupt enable
        | I2C_CR1_ADDRIE        // -> Address match interrupt enable
        | I2C_CR1_STOPIE        // -> Stop detection interrupt enable
        //| I2C_CR1_NOSTRETCH     // -> Dissable SCL slave stretch
        ; // by default analog filter is enabled

    // Set i2c_addr and enable it.
    I2C1->OAR1 |= ((int32_t)i2c_addr)<<1;
    I2C1->OAR1 |= I2C_OAR1_OA1EN;

    I2C1->CR1 |= I2C_CR1_PE; // I2C enable


    i2c_set_buffer(0, 0);

    i2c_reg = 0;
    i2c_op = I2C_OP_ADDR;

    NVIC_SetPriority(I2C1_IRQn, 0); // Set max priority
    NVIC_EnableIRQ(I2C1_IRQn);      // Enable IRQ
}

void i2c_set_buffer(uint8_t *buf, uint32_t len)
{
    i2c_buf = buf;
    i2c_buf_len = len;
}

uint32_t i2c_rx_count(void)
{
    return i2c_buf_rx_count;
}

uint32_t i2c_tx_count(void)
{
    return i2c_buf_tx_count;
}

void I2C1_IRQHandler(void)
{
    // See page 669 in RM0091 reference manual for interrup clear/set conditions
    uint32_t I2C_InterruptStatus = I2C1->ISR; /* Get interrupt status */
    uint8_t dummy;

    if ((I2C_InterruptStatus & I2C_ISR_ADDR) == I2C_ISR_ADDR)
    {
        // Writing I2C_ICR_ADDRCF clears interrupt flag
        I2C1->ICR |= I2C_ICR_ADDRCF; /* Address match event */

        if((I2C1->ISR & I2C_ISR_DIR) == I2C_ISR_DIR) /* Check if transfer direction is read (slave transmitter) */
        {
            I2C1->ISR |= I2C_ISR_TXE;  /* flush any data in TXDR */
            I2C1->CR1 |= I2C_CR1_TXIE; /* Set transmit IT */
        }
        i2c_op = I2C_OP_ADDR;
    }
    else if ((I2C_InterruptStatus & I2C_ISR_RXNE) == I2C_ISR_RXNE)
    {
        // Slave is receiving data
        // Reading RXDR clears interrupt

        if (i2c_op == I2C_OP_ADDR)
        {
            i2c_reg = (I2C1->RXDR)&0xFF;
        }
        else
        {
            if (i2c_reg<i2c_buf_len && i2c_reg>0) {
                i2c_buf[i2c_reg] = I2C1->RXDR;
                i2c_reg++;
            } else {
                dummy = I2C1->RXDR;
            }
        }
        i2c_op = I2C_OP_RX;
    }
    else if ((I2C_InterruptStatus & I2C_ISR_TXIS) == I2C_ISR_TXIS)
    {
        // Slave is transmitting data

        if (i2c_reg<i2c_buf_len)
            I2C1->TXDR = i2c_buf[i2c_reg++];
        else
            I2C1->TXDR = 0xee;
        i2c_op = I2C_OP_TX;
    }
    else if ((I2C_InterruptStatus & I2C_ISR_STOPF) == I2C_ISR_STOPF)
    {
        // Writing I2C_ICR_STOPCF clears interrupt flag
        I2C1->ICR |= I2C_ICR_STOPCF;
        if (i2c_op == I2C_OP_TX)
            i2c_buf_tx_count++;
        if (i2c_op == I2C_OP_RX)
            i2c_buf_rx_count++;
    }
}
