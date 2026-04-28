#ifndef SPI_H
#define SPI_H

#include "ee14lib.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>


/*
 * Initialize an SPI peripheral in master mode.
 *
 * spi  : SPI instance (SPI1, SPI2, etc.)
 * sck  : clock pin
 * miso : master-in slave-out pin
 * mosi : master-out slave-in pin
 *
 * Returns:
 *   EE14Lib_Err_OK on success
 *   EE14Lib_ERR_INVALID_CONFIG on bad peripheral
 */
EE14Lib_Err spi_init(SPI_TypeDef* spi, EE14Lib_Pin sck, EE14Lib_Pin miso, EE14Lib_Pin mosi);


void spi_transfer(SPI_TypeDef* spi, uint8_t* tx, uint8_t* rx, int len);


static inline void spi_write(SPI_TypeDef* spi, uint8_t* tx, int len)
{
    spi_transfer(spi, tx, NULL, len);
}


/*
 * Sends dummy bytes (0xFF) to clock data in.
 */
static inline void spi_read(SPI_TypeDef* spi, uint8_t* rx, int len)
{
    spi_transfer(spi, NULL, rx, len);
}



static inline void spi_transaction(SPI_TypeDef* spi, EE14Lib_Pin cs, uint8_t* tx, uint8_t* rx, int len);

#endif
