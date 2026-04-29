#include "stm32l4xx_hal.h"
#include "st7789.h"
#include "ee14lib.h"

#define SCL D13  // PB3 - SPI1_SCK  AF5
#define SDA D11  // PB5 - SPI1_MOSI AF5
#define RES D9   // PA8
#define DC  D6   // PB1
#define CS  A3   // PA4

int _write(int file, char *data, int len) {
    serial_write(USART2, data, len);
    return len;
}

void SysTick_Handler(void) {
    HAL_IncTick();
}

SPI_HandleTypeDef hspi1;

/*
Some of this code is just repeated from GPIO definitions, but I deleted some and everything
stopped working so didn't want to try, but will clean at some point
*/
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();

    // PB3 = SPI1_SCK, PB5 = SPI1_MOSI, both AF5
    GPIO_InitStruct.Pin       = GPIO_PIN_3 | GPIO_PIN_5;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

static void SystemClock_Config(void) {

    //This RCC code basically makes it 80MHz, was there a better way we learned in class to do this?
    //Idk whats even happening, code still works without it but loads frames slower (4 MHz vs 80 MHz?)
    //Can maybe be fixed by shortening HAL_Delay() calls?
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState            = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = 0;
    RCC_OscInitStruct.MSIClockRange       = RCC_MSIRANGE_6;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_MSI;
    RCC_OscInitStruct.PLL.PLLM            = 1;
    RCC_OscInitStruct.PLL.PLLN            = 40;
    RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
   HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);

    //THIS IS NEEDED, makes systick work :)
    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000);
    HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
}

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
    HAL_SPI_Init(&hspi1);  // this triggers HAL_SPI_MspInit above
}

static void display_gpio_init(void) {
    gpio_config_alternate_function(D13, 5);
    gpio_config_alternate_function (D11, 5);

    gpio_config_direction(D9, OUTPUT);
    gpio_config_direction(D6, OUTPUT);
    gpio_config_direction(A3, OUTPUT);

    gpio_write(A3, 1);
    gpio_write(D9, 1);
}

int main(void) {
    HAL_Init();
    __enable_irq();
    SystemClock_Config();

    host_serial_init();
    display_gpio_init();
    spi_init();

    ST7789_Init();
    ST7789_Fill_Color(RED);

    // while (1) {
    //     ST7789_Fill_Color(RED);
    // }

    while (1) {
    ST7789_Init();
    ST7789_Fill_Color(RED);

    printf("reach here\n");
    HAL_Delay(500);
    ST7789_Fill_Color(BLUE);
    HAL_Delay(500);
}
}
