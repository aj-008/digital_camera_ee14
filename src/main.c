#include "stm32l4xx_hal.h"
#include "st7789.h"
#include "ee14lib.h"
#include "ff.h"
#include "stm32_adafruit_sd.h"   // BSP_SD_Init
#include "ff.h"                   // FATFS, f_mount
#include "jpeg_display.h" 
#include "fonts.h"
#include <stdio.h>

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

//Clock config
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

/*
Display:
MOSI(SDA) - D11 - PB5 - SPI1 (shares with SD Card Reader)
SCL - D13 - PB3 - SPI1 (Shares with SD Card Reader)
RES(reset) - D9 - PA8
DC (data/command) - D6 - PB1
CS - A3 - PA4 
*/
static void display_gpio_init(void) {
    gpio_config_alternate_function(D13, 5);
    gpio_config_alternate_function (D11, 5);

    gpio_config_direction(D9, OUTPUT);
    gpio_config_direction(D6, OUTPUT);
    gpio_config_direction(A3, OUTPUT);

    gpio_write(A3, 1);
    gpio_write(D9, 1);
}

/* 
SD Card:
MOSI - D11 - PB5 - SPI1 (shares with SDA on display)
MISO - D12 - PB4 - SPI1
SCK - D13 - PB3 - SPI1 (shares with SCL on display)
CS - A0 - PA0 - Not on SPI
*/
static void sdCard_gpio_init() {
    gpio_config_alternate_function(D12, 5);

    gpio_config_direction(A0, OUTPUT);
    gpio_write(A0, 1);
}


void sd_display_test(void) {
    FIL file;
    FRESULT res;
    UINT bw, br;
   /// char write_buf[] = "Hello from STM32!";
    char read_buf[64*64] = {0};
    char status[32];

    char write_buf[64*64];


    for (int y = 0; y < 64; y++)
    {
        for (int x = 0; x < 64; x++)
        {
            if ((x + y) % 2 == 0)
                write_buf[(y*64)+x] = 0xAB; // black
            else
                write_buf[(y*64)+x] = 0xCD; // white
        }
    }

    ST7789_Fill_Color(BLACK);
    ST7789_WriteString(5, 5, "SD Card Test", Font_11x18, WHITE, BLACK);

    // --- Mount ---
    FATFS fs;
    res = f_mount(&fs, "", 1);
    snprintf(status, sizeof(status), "Mount: %s (%d)", res == FR_OK ? "OK" : "FAIL", res);
    ST7789_WriteString(5, 30, status, Font_7x10, res == FR_OK ? GREEN : RED, BLACK);
    if (res != FR_OK) return;

    // --- Write ---
    res = f_open(&file, "/TEST.TXT", FA_CREATE_ALWAYS | FA_WRITE);
    res = f_write(&file, write_buf, sizeof(write_buf), &bw);
    f_close(&file);
    snprintf(status, sizeof(status), "Write: %s (%d)", res == FR_OK ? "OK" : "FAIL", res);
    

    //ST7789_DrawImage(0, 0, 128, 128, saber);

    // --- Read ---
    res = f_open(&file, "/TEST.TXT", FA_READ);
    res = f_read(&file, read_buf, sizeof(read_buf) - 1, &br);
    f_close(&file);
    snprintf(status, sizeof(status), "Read:  %s (%d)", res == FR_OK ? "OK" : "FAIL", res);
    //ST7789_WriteString(5, 70, status, Font_7x10, res == FR_OK ? GREEN : RED, BLACK);
    ST7789_DrawImage(160, 0, 64, 64, read_buf);

    // --- Verify ---
    int match = (strcmp(read_buf, write_buf) == 0);
    ST7789_WriteString(5, 90, match ? "Data: MATCH" : "Data: MISMATCH",
                       Font_7x10, match ? GREEN : RED, BLACK);

    // Show what was read back
    ST7789_WriteString(5, 115, "Got:", Font_7x10, WHITE, BLACK);
    ST7789_WriteString(5, 130, read_buf, Font_7x10, YELLOW, BLACK);

    f_unmount("");
}

void sd_image_test(void) {
    FIL file;
    FRESULT res;
    UINT br;
    char status[32];

    #define IMG_W 128
    #define IMG_H 128
    // Read one row at a time to avoid needing 32KB of RAM at once
    static uint16_t row_buf[IMG_W];

    ST7789_Fill_Color(BLACK);
    ST7789_WriteString(5, 5, "Image Test", Font_11x18, WHITE, BLACK);

    res = f_open(&file, "/TEST.RAW", FA_READ);
    snprintf(status, sizeof(status), "Open: %s (%d)", res == FR_OK ? "OK" : "FAIL", res);
    ST7789_WriteString(5, 30, status, Font_7x10, res == FR_OK ? GREEN : RED, BLACK);
    if (res != FR_OK) return;

    // Check file size is what we expect
    FSIZE_t expected = IMG_W * IMG_H * 2;
    FSIZE_t actual   = f_size(&file);
    snprintf(status, sizeof(status), "Size: %s", actual == expected ? "OK" : "WRONG");
    ST7789_WriteString(5, 45, status, Font_7x10, actual == expected ? GREEN : RED, BLACK);

    HAL_Delay(500); // let the status text show before image overwrites it

    // Draw row by row directly to display
    for (int y = 0; y < IMG_H; y++) {
        res = f_read(&file, row_buf, sizeof(row_buf), &br);
        if (res != FR_OK || br != sizeof(row_buf)) {
            ST7789_WriteString(5, 110, "Read error!", Font_7x10, RED, BLACK);
            break;
        }
        ST7789_DrawImage(0, y, IMG_W, 1, row_buf);
    }

    f_close(&file);
    ST7789_WriteString(5, 115, "Done!", Font_7x10, GREEN, BLACK);
}


int main(void) {
    HAL_Init();
    __enable_irq();
    SystemClock_Config();

    host_serial_init();
    display_gpio_init();
    sdCard_gpio_init();
    spi_init();
    ST7789_Init();
    
   // FATFS fs;

    //BSP_SD_Init();
    SD_IO_Init();

   // sd_display_test();

   FATFS fs;
if (f_mount(&fs, "", 1) == FR_OK) {
    list_sd_root();
    jpeg_test();
} else {
    ST7789_Fill_Color(BLACK);
    ST7789_WriteString(5, 5, "Mount failed!", Font_11x18, RED, BLACK);
}

// if (f_mount(&fs, "", 1) != FR_OK) {
//     printf("FAIL: f_mount\n");
// } else {
//     printf("OK: mounted\n");
//     sd_read_write_test();
// }

   // display_jpeg("/IMAGE.JPG"); // display a JPEG from the SD card

   // f_unmount("");

   // ST7789_Test();

    while (1) {
    //ST7789_DrawImage(0, 0, 128, 128, saber);
    HAL_Delay(500);


}
}

