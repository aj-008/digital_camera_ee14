#include "stm32l432xx.h"
#include "ee14lib.h"
#include "spi.h"
#include "camera.h"
#include "st7789.h"
#include "ff.h"
#include "stm32_adafruit_sd.h"
#include "tjpgd.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>

/* HAL tick counter — used only by HAL_GetTick() if anything calls it.
   We drive it from SysTick ourselves. */
static volatile uint32_t g_tick = 0;
uint32_t HAL_GetTick(void) { return g_tick; }

/* -----------------------------------------------------------------------
 * Pin assignments (quick reference)
 *
 *  SPI1 — camera + display (PA5/PA6/PA7, AF5)
 *    SCK   PA5  A4
 *    MISO  PA6  A5
 *    MOSI  PA7  A6
 *    CAM_CS   PA8  D9
 *    LCD_CS   PA11 D10
 *
 *  SPI3 — SD card only (PB3/PB4/PB5, AF6)
 *    SCK   PB3  D13
 *    MISO  PB4  D12
 *    MOSI  PB5  D11
 *    SD_CS    PA12 D2
 *
 *  LCD control
 *    LCD_DC   PB0  D3
 *    LCD_RST  PB1  D6
 *
 *  I2C1 (OV2640 SCCB)
 *    SCL  PB6  D5  AF4
 *    SDA  PB7  D4  AF4
 *
 *  UART2 (debug / JPEG host dump)
 *    TX   PA2  A7  AF7  -> ST-Link VCP
 *    RX   PA15     AF3
 *
 *  Shutter button
 *    A0   PA0       active-low, internal pull-up
 * ----------------------------------------------------------------------- */

#define SHUTTER_PIN  A0

/* -----------------------------------------------------------------------
 * SysTick — bare-metal 1 ms tick
 * ----------------------------------------------------------------------- */
void SysTick_Handler(void) { g_tick++; }

/* -----------------------------------------------------------------------
 * Serial helpers
 * ----------------------------------------------------------------------- */
static void print(const char *msg) {
    serial_write(USART2, msg, (int)strlen(msg));
}

/* Retarget printf → USART2 */
int _write(int file, char *data, int len) {
    (void)file;
    serial_write(USART2, data, len);
    return len;
}

/* -----------------------------------------------------------------------
 * Delay — uses SysTick g_tick (1 ms resolution)
 * ----------------------------------------------------------------------- */
static void delay_ms(uint32_t ms) {
    uint32_t start = g_tick;
    while ((g_tick - start) < ms);
}

/* -----------------------------------------------------------------------
 * Clock — 80 MHz via MSI PLL, bare-metal registers
 *
 * MSI @ 4 MHz (range 6) → PLL → SYSCLK = 4 * 40 / (1*2) = 80 MHz
 * HCLK = PCLK1 = PCLK2 = 80 MHz, Flash latency = 4 WS
 * ----------------------------------------------------------------------- */
static void SystemClock_Config(void) {
    /* 1. Boost Flash latency to 4 WS before increasing clock speed */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_4WS;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_4WS);

    /* 2. Enable MSI, set range 6 (4 MHz) */
    RCC->CR |= RCC_CR_MSION;
    while (!(RCC->CR & RCC_CR_MSIRDY));
    RCC->CR  = (RCC->CR & ~RCC_CR_MSIRANGE) | RCC_CR_MSIRANGE_6 | RCC_CR_MSIRGSEL;

    /* 3. Configure PLL: source=MSI, M=1, N=40, R=2 → 80 MHz */
    RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_MSI          /* MSI source */
                 | (0U  << RCC_PLLCFGR_PLLM_Pos)   /* M = 1 (field = 0) */
                 | (40U << RCC_PLLCFGR_PLLN_Pos)   /* N = 40 */
                 | (0U  << RCC_PLLCFGR_PLLR_Pos)   /* R = 2 (field = 0) */
                 | RCC_PLLCFGR_PLLREN;              /* enable R output */

    /* 4. Enable PLL and wait */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    /* 5. Switch SYSCLK to PLL */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

    /* 6. Update SystemCoreClock global used by serial.c for BRR calculation */
    SystemCoreClock = 80000000UL;

    /* 7. SysTick at 1 ms */
    SysTick_Config(SystemCoreClock / 1000);
}

/* -----------------------------------------------------------------------
 * TJpgDec integration
 *
 * We decode the JPEG that lives in the ArduChip FIFO directly via SPI,
 * streaming through a 512-byte input buffer, and blit each MCU block
 * straight to the ST7789 via ST7789_DrawImage().
 * ----------------------------------------------------------------------- */

/* RAM buffer for JPEG — fits largest expected frame (320x240 JPEG ~8KB max) */
static uint8_t jpeg_buf[10240];
static uint32_t jpeg_buf_len;
static uint32_t jpeg_buf_pos;

/* TJpgDec input function: read from RAM buffer (SPI already closed) */
static size_t jpeg_input(JDEC *jd, uint8_t *buf, size_t ndata) {
    (void)jd;
    if (jpeg_buf_pos >= jpeg_buf_len) return 0;
    if (ndata > jpeg_buf_len - jpeg_buf_pos)
        ndata = jpeg_buf_len - jpeg_buf_pos;
    memcpy(buf, jpeg_buf + jpeg_buf_pos, ndata);
    jpeg_buf_pos += ndata;
    return ndata;
}

/* TJpgDec output function: blit MCU block to display.
 * TJpgDec produces RGB565 as little-endian uint16_t on ARM (low byte first
 * in memory). ST7789 expects big-endian over SPI (high byte first).
 * Swap each pixel's bytes before writing.                               */
static int jpeg_output(JDEC *jd, void *bitmap, JRECT *rect) {
    (void)jd;
    uint16_t *px = (uint16_t *)bitmap;
    uint32_t npix = (uint32_t)(rect->right  - rect->left + 1)
                  * (uint32_t)(rect->bottom - rect->top  + 1);
    for (uint32_t i = 0; i < npix; i++) {
        uint16_t v = px[i];
        px[i] = (v << 8) | (v >> 8);  /* swap bytes */
    }
    ST7789_DrawImage(rect->left, rect->top,
                     rect->right  - rect->left + 1,
                     rect->bottom - rect->top  + 1,
                     px);
    return 1;
}

/* Decode JPEG from ArduChip FIFO and display it */
static void display_jpeg_from_fifo(void) {
    /* Reset read pointer, then read length twice to ensure stability */
    camera_fifo_reset_rdptr();
    uint32_t length = camera_fifo_length();
    uint32_t length2 = camera_fifo_length();
    /* If the two reads disagree, take the larger one */
    if (length2 > length) length = length2;

    /* Print length regardless of validity */
    {
        char lb[12];
        lb[0]='L'; lb[1]='E'; lb[2]='N'; lb[3]='=';
        uint32_t v = length;
        for (int i = 9; i >= 4; i--) { lb[i]='0'+(v%10); v/=10; }
        lb[10]='\r'; lb[11]='\n';
        serial_write(USART2, lb, 12);
    }

    if (length == 0 || length > sizeof(jpeg_buf) - 1) {
        print("ERR: bad fifo length\r\n");
        return;
    }

    /* Read entire FIFO into RAM.
     * We read length+1 bytes: slot 0 reserved for synthetic FF if needed,
     * slots 1..length hold raw FIFO bytes.
     * CS deasserted before any display SPI transactions.                */
    uint32_t read_len = length;
    if (read_len > sizeof(jpeg_buf) - 1) read_len = sizeof(jpeg_buf) - 1;

    spi_cs_low(CAM_CS);
    spi_transfer(ARDUCHIP_BURST_FIFO);
    spi_transfer(0x00);  /* dummy byte */
    spi_transfer_buf(NULL, jpeg_buf + 1, (uint16_t)read_len);
    spi_cs_high(CAM_CS);

    /* Check alignment and fix 1-byte offset */
    uint8_t *jpeg_start;
    if (jpeg_buf[1] == 0xFF && jpeg_buf[2] == 0xD8) {
        jpeg_start = jpeg_buf + 1;
        jpeg_buf_len = read_len;
    } else if (jpeg_buf[1] == 0xD8 && jpeg_buf[2] == 0xFF) {
        jpeg_buf[0] = 0xFF;
        jpeg_start = jpeg_buf;
        jpeg_buf_len = read_len + 1;
    } else {
        char dbg[20];
        dbg[0]='H'; dbg[1]='D'; dbg[2]='R'; dbg[3]='=';
        const char *hx = "0123456789ABCDEF";
        for (int i = 0; i < 4; i++) {
            dbg[4+i*3]   = hx[jpeg_buf[1+i]>>4];
            dbg[4+i*3+1] = hx[jpeg_buf[1+i]&0xF];
            dbg[4+i*3+2] = ' ';
        }
        dbg[16]='\r'; dbg[17]='\n';
        serial_write(USART2, dbg, 18);
        print("ERR: no JPEG SOI marker\r\n");
        return;
    }

    /* Set up RAM-based input */
    jpeg_buf_pos = 0;
    /* Temporarily point jpeg_input to jpeg_start */
    /* We use a static pointer since jpeg_input reads from globals */
    /* Shift so jpeg_buf always starts at index 0 for the input fn */
    if (jpeg_start != jpeg_buf) {
        memmove(jpeg_buf, jpeg_start, jpeg_buf_len);
    }
    jpeg_buf_pos = 0;

    /* Decode */
    static uint8_t work[8192];
    JDEC jdec;
    JRESULT res = jd_prepare(&jdec, jpeg_input, work, sizeof(work), NULL);
    if (res != JDR_OK) {
        print("ERR: jd_prepare failed\r\n");
        return;
    }
    res = jd_decomp(&jdec, jpeg_output, 0);
    if (res != JDR_OK) {
        char errmsg[24] = "ERR: jd_decomp rc=0x";
        const char *hx2 = "0123456789ABCDEF";
        errmsg[20] = hx2[(res>>4)&0xF];
        errmsg[21] = hx2[res&0xF];
        errmsg[22] = '\r'; errmsg[23] = '\n';
        serial_write(USART2, errmsg, 24);
    }
}

/* -----------------------------------------------------------------------
 * FatFS + SD: save JPEG to file
 * ----------------------------------------------------------------------- */
static FATFS fs;
static uint32_t file_counter = 0;

static int sd_mount(void) {
    FRESULT fr = f_mount(&fs, "", 1);
    return (fr == FR_OK) ? 0 : -1;
}

static int save_jpeg_to_sd(void) {
    /* jpeg_buf is already populated by display_jpeg_from_fifo — write it directly */
    if (jpeg_buf_len == 0) return -1;

    char fname[24];
    snprintf(fname, sizeof(fname), "IMG%04lu.JPG", (unsigned long)file_counter++);

    FIL fil;
    FRESULT fr = f_open(&fil, fname, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK) return -1;

    UINT bw;
    fr = f_write(&fil, jpeg_buf, (UINT)jpeg_buf_len, &bw);
    f_close(&fil);

    if (fr == FR_OK && bw == (UINT)jpeg_buf_len) {
        print("saved: "); print(fname); print("\r\n");
        return 0;
    }
    return -1;
}

/* -----------------------------------------------------------------------
 * Capture, display, and save
 * ----------------------------------------------------------------------- */
static void do_capture(int sd_ok) {
    print("capturing...\r\n");
    if (camera_capture() != 0) {
        print("ERR: capture timeout\r\n");
        return;
    }
    print("captured. decoding...\r\n");

    /* Reads FIFO into jpeg_buf, decodes to display */
    display_jpeg_from_fifo();

    /* jpeg_buf now holds the complete JPEG — save it */
    if (sd_ok)
        save_jpeg_to_sd();

    print("done.\r\n");
}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
int main(void) {
    __enable_irq();
    SystemClock_Config();
    host_serial_init();
    print("boot\r\n");

    /* ── Pre-deassert all SPI CS lines BEFORE spi_init() ────────────────*/
    gpio_config_mode(CAM_CS,     OUTPUT); spi_cs_high(CAM_CS);
    gpio_config_mode(LCD_CS_PIN, OUTPUT); spi_cs_high(LCD_CS_PIN);
    gpio_config_mode(SD_CS,      OUTPUT); spi_cs_high(SD_CS);

    /* ── Bus init ────────────────────────────────────────────────────── */
    spi_init();    /* SPI1: PA5/PA6/PA7 — camera + display */
    /* SPI3 (SD) is initialised inside SD_IO_Init() via sd_io.c         */

    /* ── Camera first — self-test before SD touches the bus ─────────── */
    if (camera_init() != 0) {
        print("ERR: camera_init failed\r\n");
        while (1);
    }
    print("camera ok\r\n");

    /* ── Display ─────────────────────────────────────────────────────── */
    ST7789_GPIO_Init();
    ST7789_Init();
    ST7789_Fill_Color(BLACK);
    print("display ok\r\n");

    /* ── SD card last — BSP_SD_Init clocks out the 80 dummy bytes that
     * release MISO. Camera is already verified before this runs.        */
    int sd_ok = 0;
    if (sd_mount() == 0) {
        sd_ok = 1;
        print("SD ok\r\n");
    } else {
        print("SD not found — continuing without SD\r\n");
    }

    /* ── Shutter button ──────────────────────────────────────────────── */
    gpio_config_mode(SHUTTER_PIN, INPUT);
    gpio_config_pullup(SHUTTER_PIN, PULL_UP);
    print("ready. press A0 to capture.\r\n");

    /* ── Main loop ───────────────────────────────────────────────────── */
    while (1) {
        if (!gpio_read(SHUTTER_PIN)) {
            delay_ms(50);                        /* debounce */
            if (gpio_read(SHUTTER_PIN)) continue;
            do_capture(sd_ok);
            while (!gpio_read(SHUTTER_PIN));     /* wait for release */
            delay_ms(500);                       /* anti-bounce hold-off */
        }
    }
}
