#include "spi.h"
#include "stm32l432xx.h"

/*
 * SPI1 — camera + display
 * PA5=SCK, PA6=MISO, PA7=MOSI, all AF5, ~2.5 MHz (/32)
 */
void spi_init(void) {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    GPIOA->MODER   &= ~((0x3 << (5*2)) | (0x3 << (6*2)) | (0x3 << (7*2)));
    GPIOA->MODER   |=  ((0x2 << (5*2)) | (0x2 << (6*2)) | (0x2 << (7*2)));
    GPIOA->OSPEEDR |=  ((0x3 << (5*2)) | (0x3 << (6*2)) | (0x3 << (7*2)));
    GPIOA->PUPDR   &= ~((0x3 << (5*2)) | (0x3 << (6*2)) | (0x3 << (7*2)));
    GPIOA->PUPDR   |=  (0x1 << (6*2));   /* pull-up on MISO PA6 */
    GPIOA->AFR[0]  &= ~((0xF << (5*4)) | (0xF << (6*4)) | (0xF << (7*4)));
    GPIOA->AFR[0]  |=  ((0x5 << (5*4)) | (0x5 << (6*4)) | (0x5 << (7*4)));

    SPI1->CR1 &= ~SPI_CR1_SPE;
    SPI1->CR1  = SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_MSTR
               | (0x4 << SPI_CR1_BR_Pos);  /* /32 -> ~2.5 MHz */
    SPI1->CR2  = (0x7 << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH;
    SPI1->CR1 |= SPI_CR1_SPE;
}

uint8_t spi_transfer(uint8_t tx) {
    while (!(SPI1->SR & SPI_SR_TXE));
    *(volatile uint8_t *)&SPI1->DR = tx;
    while (!(SPI1->SR & SPI_SR_RXNE));
    return *(volatile uint8_t *)&SPI1->DR;
}

void spi_transfer_buf(const uint8_t *tx, uint8_t *rx, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        uint8_t in = spi_transfer(tx ? tx[i] : 0x00);
        if (rx) rx[i] = in;
    }
}

/*
 * SPI3 — SD card only
 * PB3=SCK, PB4=MISO, PB5=MOSI, all AF6, ~2.5 MHz (/32)
 */
void spi3_init(void) {
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOBEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_SPI3EN;

    GPIOB->MODER   &= ~((0x3 << (3*2)) | (0x3 << (4*2)) | (0x3 << (5*2)));
    GPIOB->MODER   |=  ((0x2 << (3*2)) | (0x2 << (4*2)) | (0x2 << (5*2)));
    GPIOB->OSPEEDR |=  ((0x3 << (3*2)) | (0x3 << (4*2)) | (0x3 << (5*2)));
    GPIOB->PUPDR   &= ~((0x3 << (3*2)) | (0x3 << (4*2)) | (0x3 << (5*2)));
    GPIOB->PUPDR   |=  (0x1 << (4*2));   /* pull-up on MISO PB4 */
    GPIOB->AFR[0]  &= ~((0xF << (3*4)) | (0xF << (4*4)) | (0xF << (5*4)));
    GPIOB->AFR[0]  |=  ((0x6 << (3*4)) | (0x6 << (4*4)) | (0x6 << (5*4)));

    SPI3->CR1 &= ~SPI_CR1_SPE;
    SPI3->CR1  = SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_MSTR
               | (0x4 << SPI_CR1_BR_Pos);  /* /32 -> ~2.5 MHz */
    SPI3->CR2  = (0x7 << SPI_CR2_DS_Pos) | SPI_CR2_FRXTH;
    SPI3->CR1 |= SPI_CR1_SPE;
}

uint8_t spi3_transfer(uint8_t tx) {
    while (!(SPI3->SR & SPI_SR_TXE));
    *(volatile uint8_t *)&SPI3->DR = tx;
    while (!(SPI3->SR & SPI_SR_RXNE));
    return *(volatile uint8_t *)&SPI3->DR;
}

void spi3_transfer_buf(const uint8_t *tx, uint8_t *rx, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        uint8_t in = spi3_transfer(tx ? tx[i] : 0x00);
        if (rx) rx[i] = in;
    }
}
