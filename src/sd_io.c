#include "stm32l4xx_hal.h"
#include "stm32_adafruit_sd.h"
#include "ee14lib.h"

extern SPI_HandleTypeDef hspi1;  // match your CubeMX/your spi.c

#define SD_CS_GPIO_PORT   GPIOA
#define SD_CS_PIN         GPIO_PIN_0  // D4 on Nucleo L432KC

void SD_IO_Init(void) {
    HAL_GPIO_WritePin(SD_CS_GPIO_PORT, SD_CS_PIN, GPIO_PIN_SET);
    for (int i = 0; i < 10; i++) {
        SD_IO_WriteByte(0xFF);
    }
}

void SD_IO_CSState(uint8_t state) {
    HAL_GPIO_WritePin(SD_CS_GPIO_PORT, SD_CS_PIN,
                      state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

uint8_t SD_IO_WriteByte(uint8_t Data) {
    uint8_t rx;
    HAL_SPI_TransmitReceive(&hspi1, &Data, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

void SD_IO_WriteReadData(const uint8_t *DataIn, uint8_t *DataOut, uint16_t DataLength) {
    HAL_SPI_TransmitReceive(&hspi1, (uint8_t*)DataIn, DataOut, DataLength, HAL_MAX_DELAY);
}

// void HAL_Delay(__IO uint32_t Delay) {
//     HAL_Delay(Delay);  // already provided by HAL, but required by the header
// }