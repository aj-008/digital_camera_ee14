#include "stm32l4xx_hal.h"
#include "st7789.h"
#include "ee14lib.h"

#define SCL D13
#define SDA D11
#define RES D9
#define DC  D6
#define CS  A3

int _write(int file, char *data, int len) {
    serial_write(USART2, data, len);
    return len;
}

SPI_HandleTypeDef hspi1;


static void spi_init(void) {
    __HAL_RCC_SPI1_CLK_ENABLE();
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; 
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    HAL_SPI_Init(&hspi1);
}

void display_gpio_init() {
    gpio_config_direction(CS, OUTPUT);
    gpio_config_direction(DC, OUTPUT);
    gpio_config_direction(SCL, OUTPUT);
    gpio_config_direction(SDA, OUTPUT);
    gpio_config_direction(RES, OUTPUT);


    gpio_config_pullup(RES, PULL_UP); // maybe reset active low
}

int main(void) {
    host_serial_init();
    display_gpio_init();

    HAL_Init();
    spi_init();


    ST7789_Init();
    ST7789_Fill_Color(RED);

    while (1) {}
}
