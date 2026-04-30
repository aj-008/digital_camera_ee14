#ifndef I2C_H
#define I2C_H

#include "stm32l432xx.h"
#include <stdint.h>
#include <stdbool.h>

void i2c1_init(void);
bool i2c1_scan(uint8_t addr);
bool i2c1_write(uint8_t addr, const uint8_t *buf, uint8_t len);
bool i2c1_write_reg(uint8_t addr, uint8_t reg, uint8_t val);
bool i2c1_read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len);


#endif 
