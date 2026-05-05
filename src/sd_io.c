#include "stm32_adafruit_sd.h"
#include "spi.h"
#include "ee14lib.h"


#define SD_CS  D2

void SD_IO_Init(void) {
    gpio_config_mode(SD_CS, OUTPUT);
    spi_cs_high(SD_CS);
    spi3_init();
    /* dummy clocks with CS high to release SD MISO */
    for (int i = 0; i < 10; i++)
        spi3_transfer(0xFF);
}

void SD_IO_CSState(uint8_t state) {
    if (state) spi_cs_high(SD_CS);
    else        spi_cs_low(SD_CS);
}

uint8_t SD_IO_WriteByte(uint8_t data) {
    return spi3_transfer(data);
}

void SD_IO_WriteReadData(const uint8_t *DataIn, uint8_t *DataOut, uint16_t DataLength) {
    spi3_transfer_buf(DataIn, DataOut, DataLength);
}
