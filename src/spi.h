#ifndef SPI_H
#define SPI_H


#include "stm32l432xx.h"
#include "ee14lib.h"
#include <stdint.h>

void    spi_init(void);
uint8_t spi_transfer(uint8_t tx);
void    spi_transfer_buf(const uint8_t *tx, uint8_t *rx, uint16_t len);

void    spi3_init(void);
uint8_t spi3_transfer(uint8_t tx);
void    spi3_transfer_buf(const uint8_t *tx, uint8_t *rx, uint16_t len);

static inline void spi_cs_low (EE14Lib_Pin cs) { gpio_write(cs, false); }
static inline void spi_cs_high(EE14Lib_Pin cs) { gpio_write(cs, true);  }

#endif 
