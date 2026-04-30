#ifndef CAMERA_H
#define CAMERA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l432xx.h"
#include <stdint.h>

/*
 * camera.h — ArduChip/OV2640 driver for STM32L432KC
 *
 * SPI (ArduChip register bus): shared SPI1, see spi.h
 *   CAM_CS  PA8  D9   — camera chip-select (GPIO output)
 *
 * I2C (OV2640 SCCB sensor config): shared I2C1, see i2c.h
 *   SCL     PB6  D5   AF4  (5V-tolerant)
 *   SDA     PB7  D4   AF4  (5V-tolerant)
 *
 * Call i2c1_init() BEFORE camera_init().
 * camera_init() calls spi_init() internally.
 */

/* CS pin — PA8 / D9 */
#define CAM_CS  D9   /* PA8 */

/* I2C pins — PA9/PA10 (D1/D0) */
#define CAM_SCL  D1
#define CAM_SDA  D0

/* OV2640 7-bit I2C address */
#define OV2640_I2C_ADDR  0x30

/* ArduChip register map */
#define ARDUCHIP_TEST1       0x00
#define ARDUCHIP_FIFO        0x04
#define ARDUCHIP_TRIG        0x41
#define ARDUCHIP_BURST_FIFO  0x3C

#define FIFO_CLEAR_MASK      0x01
#define FIFO_START_MASK      0x02
#define FIFO_RDPTR_RST_MASK  0x10
#define FIFO_WRPTR_RST_MASK  0x20
#define CAP_DONE_MASK        0x08

/* sensor_reg struct used by ov2640_regs.h */
#ifndef SENSOR_REG_DEFINED
#define SENSOR_REG_DEFINED
struct sensor_reg {
    uint16_t reg;
    uint16_t val;
};
#endif

#ifndef PROGMEM
#define PROGMEM
#endif

int      camera_init(void);
int      camera_capture(void);
uint32_t camera_fifo_length(void);
int      camera_dump_uart(USART_TypeDef *uart);
void     camera_fifo_reset_rdptr(void);

#ifdef __cplusplus
}
#endif

#endif /* CAMERA_H */
