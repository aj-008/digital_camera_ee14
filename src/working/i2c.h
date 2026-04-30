#ifndef I2C_H
#define I2C_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * i2c.h — bare-metal I2C1 driver, STM32L432KC
 *
 * Pins (fixed, 5V-tolerant, used by OV2640/SCCB):
 *   PB6  D5  SCL  AF4
 *   PB7  D4  SDA  AF4
 *
 * Call i2c1_init() once after SystemClock_Config().
 * camera.c's internal i2c_init_peripheral() has been removed;
 * it delegates to this driver instead.
 */

#include "stm32l432xx.h"
#include <stdint.h>
#include <stdbool.h>

void i2c1_init(void);
bool i2c1_scan(uint8_t addr);
bool i2c1_write(uint8_t addr, const uint8_t *buf, uint8_t len);
bool i2c1_write_reg(uint8_t addr, uint8_t reg, uint8_t val);
bool i2c1_read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* I2C_H */
