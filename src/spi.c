#include "spi.h"
#include "ee14lib.h"
#include "stm32l432xx.h"


EE14Lib_Err spi_init(SPI_TypeDef* spi, EE14Lib_Pin sck, EE14Lib_Pin miso, EE14Lib_Pin mosi)
{
    gpio_config_alternate_function(sck, 5);
    gpio_config_alternate_function(miso, 5);
    gpio_config_alternate_function(mosi, 5);

    gpio_config_mode(sck, ALTERNATE_FUNCTION);
    gpio_config_mode(miso, ALTERNATE_FUNCTION);
    gpio_config_mode(mosi, ALTERNATE_FUNCTION);

    // Optional: no pullups for SPI typically
    gpio_config_pullup(sck, PULL_OFF);
    gpio_config_pullup(miso, PULL_OFF);
    gpio_config_pullup(mosi, PULL_OFF);

    if (spi == SPI1) {
        RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    } else {
        return EE14Lib_ERR_INVALID_CONFIG;
    }

    if (spi == SPI1) {
        RCC->APB2RSTR |= RCC_APB2RSTR_SPI1RST;
        RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI1RST;
    }

    spi->CR1 =
        SPI_CR1_MSTR |        // Master mode
        SPI_CR1_SSI  |        // Internal slave select
        SPI_CR1_SSM  |        // Software slave management
        (0b011 << SPI_CR1_BR_Pos); // Baud rate (adjust)

    spi->CR2 =
        (7 << SPI_CR2_DS_Pos) | // 8-bit data
        SPI_CR2_FRXTH;          // RXNE when >= 8 bits

    spi->CR1 |= SPI_CR1_SPE;

    return EE14Lib_Err_OK;
}

void spi_transfer(SPI_TypeDef* spi, uint8_t* tx, uint8_t* rx, int len)
{
    for (int i = 0; i < len; i++) {

        while (!(spi->SR & SPI_SR_TXE)) {}

        spi->DR = tx ? tx[i] : 0xFF;

        while (!(spi->SR & SPI_SR_RXNE)) {}

        uint8_t data = spi->DR;

        if (rx) {
            rx[i] = data;
        }
    }

    while (spi->SR & SPI_SR_BSY) {}
}


void spi_transaction(SPI_TypeDef* spi, EE14Lib_Pin cs, uint8_t* tx, uint8_t* rx, int len)
{
    gpio_write(cs, 0);
    spi_transfer(spi, tx, rx, len);
    gpio_write(cs, 1);
}
