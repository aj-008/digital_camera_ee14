#ifndef SPI_H
#define SPI_H

/*
 * spi.h — dual SPI driver, STM32L432KC
 *
 * SPI1 (camera + display):
 *   SCK  PA5  A4  AF5
 *   MISO PA6  A5  AF5
 *   MOSI PA7  A6  AF5
 *
 * SPI3 (SD card):
 *   SCK  PB3  D13  AF6
 *   MISO PB4  D12  AF6
 *   MOSI PB5  D11  AF6
 */

#include "stm32l432xx.h"
#include "ee14lib.h"
#include <stdint.h>

/* SPI1 — camera + display */
void    spi_init(void);
uint8_t spi_transfer(uint8_t tx);
void    spi_transfer_buf(const uint8_t *tx, uint8_t *rx, uint16_t len);

/* SPI3 — SD card */
void    spi3_init(void);
uint8_t spi3_transfer(uint8_t tx);
void    spi3_transfer_buf(const uint8_t *tx, uint8_t *rx, uint16_t len);

static inline void spi_cs_low (EE14Lib_Pin cs) { gpio_write(cs, false); }
static inline void spi_cs_high(EE14Lib_Pin cs) { gpio_write(cs, true);  }

#endif /* SPI_H */
