#ifndef _I2C_SLAVE_H_
#define _I2C_SLAVE_H_

void i2c_slave_init(uint8_t i2c_addr);

void i2c_set_tx_buffer(uint8_t *tx, uint32_t len);

void i2c_set_rx_buffer(uint8_t *rx, uint32_t len);

uint32_t i2c_tx_count(void);

uint32_t i2c_rx_count(void);

#endif
