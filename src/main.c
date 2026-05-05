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

/* -----------------------------------------------------------------------
 * Pin assignments
 *
 *  SPI1 — camera + display (PA5/A4, PA6/A5, PA7/A6)
 *  SPI3 — SD card (PB3/D13, PB4/D12, PB5/D11)
 *  CAM_CS  PA8  D9 
 *  LCD_DC  PB0  D3  
 *  I2C1    PB6/D5=SCL  PB7/D4=SDA
 *  BTN_A   PA0  A0   
 *  BTN_B   PA3  A3  
 * ----------------------------------------------------------------------- */

#define BTN_A  A0
#define BTN_B  A3

/* -----------------------------------------------------------------------
 * System
 * ----------------------------------------------------------------------- */
static volatile uint32_t g_tick = 0;
void SysTick_Handler(void) { g_tick++; }
uint32_t HAL_GetTick(void) { return g_tick; }

static void delay_ms(uint32_t ms) {
    uint32_t s = g_tick; while ((g_tick - s) < ms);
}

static void print(const char *msg) {
    serial_write(USART2, msg, (int)strlen(msg));
}

int _write(int file, char *data, int len) {
    (void)file; serial_write(USART2, data, len); return len;
}

static void SystemClock_Config(void) {
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_4WS;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_4WS);
    RCC->CR |= RCC_CR_MSION;
    while (!(RCC->CR & RCC_CR_MSIRDY));
    RCC->CR = (RCC->CR & ~RCC_CR_MSIRANGE) | RCC_CR_MSIRANGE_6 | RCC_CR_MSIRGSEL;
    RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_MSI
                 | (0U  << RCC_PLLCFGR_PLLM_Pos)
                 | (40U << RCC_PLLCFGR_PLLN_Pos)
                 | (0U  << RCC_PLLCFGR_PLLR_Pos)
                 | RCC_PLLCFGR_PLLREN;
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
    SystemCoreClock = 80000000UL;
    SysTick_Config(SystemCoreClock / 1000);
}

/* -----------------------------------------------------------------------
 * Button helpers
 * ----------------------------------------------------------------------- */
#define NO_BTN  ((EE14Lib_Pin)0xFF)

static char wait_btn(EE14Lib_Pin a, EE14Lib_Pin b) {
    int has_a = (a != NO_BTN);
    int has_b = (b != NO_BTN);
    /* Wait for both to be released first */
    while ((has_a && !gpio_read(a)) || (has_b && !gpio_read(b)));
    delay_ms(20);
    while (1) {
        if (has_a && !gpio_read(a)) {
            delay_ms(30);
            if (!gpio_read(a)) {
                while (!gpio_read(a));
                delay_ms(20);
                return 'A';
            }
        }
        if (has_b && !gpio_read(b)) {
            delay_ms(30);
            if (!gpio_read(b)) {
                while (!gpio_read(b));
                delay_ms(20);
                return 'B';
            }
        }
    }
}

/* -----------------------------------------------------------------------
 * JPEG / TJpgDec
 * ----------------------------------------------------------------------- */
static uint8_t  jpeg_buf[12288];
static uint32_t jpeg_buf_len;
static uint32_t jpeg_buf_pos;

static size_t jpeg_input(JDEC *jd, uint8_t *buf, size_t ndata) {
    (void)jd;
    if (jpeg_buf_pos >= jpeg_buf_len) return 0;
    if (ndata > jpeg_buf_len - jpeg_buf_pos)
        ndata = jpeg_buf_len - jpeg_buf_pos;
    memcpy(buf, jpeg_buf + jpeg_buf_pos, ndata);
    jpeg_buf_pos += ndata;
    return ndata;
}

static int jpeg_output(JDEC *jd, void *bitmap, JRECT *rect) {
    (void)jd;
    uint16_t *px = (uint16_t *)bitmap;
    uint32_t npix = (uint32_t)(rect->right  - rect->left + 1)
                  * (uint32_t)(rect->bottom - rect->top  + 1);
    for (uint32_t i = 0; i < npix; i++) {
        uint16_t v = px[i]; px[i] = (v << 8) | (v >> 8);
    }
    ST7789_DrawImage(rect->left, rect->top,
                     rect->right  - rect->left + 1,
                     rect->bottom - rect->top  + 1, px);
    return 1;
}

static int decode_to_display(void) {
    static uint8_t work[8192];
    jpeg_buf_pos = 0;
    JDEC jdec;
    if (jd_prepare(&jdec, jpeg_input, work, sizeof(work), NULL) != JDR_OK)
        return -1;
    if (jd_decomp(&jdec, jpeg_output, 0) != JDR_OK)
        return -1;
    return 0;
}

static int load_fifo(void) {
    uint32_t length = camera_fifo_length();
    if (length == 0 || length > sizeof(jpeg_buf) - 4) return -1;

    camera_fifo_reset_rdptr();

    spi_cs_low(CAM_CS);
    spi_transfer(ARDUCHIP_BURST_FIFO);
    spi_transfer(0x00);  /* dummy byte */
    spi_transfer_buf(NULL, jpeg_buf + 1, (uint16_t)length);
    spi_cs_high(CAM_CS);

    if      (jpeg_buf[1] == 0xFF && jpeg_buf[2] == 0xD8) {
        /* shift down by 1 */
        memmove(jpeg_buf, jpeg_buf + 1, length);
        jpeg_buf_len = length;
    } else if (jpeg_buf[1] == 0xD8 && jpeg_buf[2] == 0xFF) {
        /* 1-byte offset if it's weird*/
        jpeg_buf[0] = 0xFF;
        jpeg_buf_len = length + 1;
    } else {
        return -1;
    }
    jpeg_buf_pos = 0;
    return 0;
}

/* -----------------------------------------------------------------------
 * SD helpers
 * ----------------------------------------------------------------------- */
static FATFS fs;
static uint32_t file_counter = 0;

static int sd_mount(void) {
    return (f_mount(&fs, "", 1) == FR_OK) ? 0 : -1;
}

static int save_to_sd(void) {
    if (jpeg_buf_len == 0) return -1;
    char fname[24];
    snprintf(fname, sizeof(fname), "IMG%04lu.JPG", (unsigned long)file_counter++);
    FIL fil;
    if (f_open(&fil, fname, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) return -1;
    UINT bw;
    FRESULT fr = f_write(&fil, jpeg_buf, (UINT)jpeg_buf_len, &bw);
    f_close(&fil);
    if (fr == FR_OK && bw == (UINT)jpeg_buf_len) {
        print("saved: "); print(fname); print("\r\n");
        return 0;
    }
    return -1;
}

static int load_from_sd(const char *fname) {
    FIL fil;
    if (f_open(&fil, fname, FA_READ) != FR_OK) return -1;
    UINT br;
    FRESULT fr = f_read(&fil, jpeg_buf, sizeof(jpeg_buf), &br);
    f_close(&fil);
    if (fr != FR_OK || br == 0) return -1;
    jpeg_buf_len = br;
    jpeg_buf_pos = 0;
    return 0;
}

/* -----------------------------------------------------------------------
 * Display UI helpers
 * ----------------------------------------------------------------------- */

/* Tiny 5x7 font */
static const uint8_t font5x7[96][5] = {
    {0,0,0,0,0},       
    {0,0,0x5F,0,0},   
    {0,7,0,7,0},     
    {0x14,0x7F,0x14,0x7F,0x14}, 
    {0x24,0x2A,0x7F,0x2A,0x12},
    {0x23,0x13,8,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50},
    {0,5,3,0,0},
    {0,0x1C,0x22,0x41,0},
    {0,0x41,0x22,0x1C,0},
    {0x14,8,0x3E,8,0x14},
    {8,8,0x3E,8,8},
    {0,0x50,0x30,0,0},
    {8,8,8,8,8},
    {0,0x60,0x60,0,0},
    {0x20,0x10,8,4,2},
    {0x3E,0x51,0x49,0x45,0x3E}, 
    {0,0x42,0x7F,0x40,0},
    {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},
    {1,0x71,9,5,3},
    {0x36,0x49,0x49,0x49,0x36},
    {6,0x49,0x49,0x29,0x1E},
    {0,0x36,0x36,0,0},
    {0,0x56,0x36,0,0},
    {8,0x14,0x22,0x41,0},
    {0x14,0x14,0x14,0x14,0x14},
    {0,0x41,0x22,0x14,8},
    {2,1,0x51,9,6},
    {0x32,0x49,0x79,0x41,0x3E},
    {0x7E,0x11,0x11,0x11,0x7E},
    {0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41},
    {0x7F,9,9,9,1},
    {0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,8,8,8,0x7F},
    {0,0x41,0x7F,0x41,0},
    {0x20,0x40,0x41,0x3F,1},
    {0x7F,8,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,2,0xC,2,0x7F},
    {0x7F,4,8,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,9,9,9,6},
    {0x3E,0x41,0x51,0x21,0x5E},
    {0x7F,9,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},
    {1,1,0x7F,1,1},
    {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},
    {0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,8,0x14,0x63},
    {7,8,0x70,8,7},
    {0x61,0x51,0x49,0x45,0x43},
    {0,0x7F,0x41,0x41,0},
    {2,4,8,0x10,0x20},
    {0,0x41,0x41,0x7F,0},
    {4,2,1,2,4},
    {0x40,0x40,0x40,0x40,0x40},
    {0,1,2,4,0},
    {0x20,0x54,0x54,0x54,0x78}, 
    {0x7F,0x48,0x44,0x44,0x38},
    {0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7F},
    {0x38,0x54,0x54,0x54,0x18},
    {8,0x7E,9,1,2},
    {0xC,0x52,0x52,0x52,0x3E},
    {0x7F,8,4,4,0x78},
    {0,0x44,0x7D,0x40,0},
    {0x20,0x40,0x44,0x3D,0},
    {0x7F,0x10,0x28,0x44,0},
    {0,0x41,0x7F,0x40,0},
    {0x7C,4,0x18,4,0x78},
    {0x7C,8,4,4,0x78},
    {0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,8},
    {8,0x14,0x14,0x18,0x7C},
    {0x7C,8,4,4,8},
    {0x48,0x54,0x54,0x54,0x20},
    {4,0x3F,0x44,0x40,0x20},
    {0x3C,0x40,0x40,0x20,0x7C},
    {0x1C,0x20,0x40,0x20,0x1C},
    {0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44},
    {0xC,0x50,0x50,0x50,0x3C},
    {0x44,0x64,0x54,0x4C,0x44},
    {0,8,0x36,0x41,0},
    {0,0,0x7F,0,0},
    {0,0x41,0x36,8,0},
    {2,1,2,4,2},
    {0x3C,0x26,0x23,0x26,0x3C},
};

static void draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale) {
    if (c < 32 || c > 127) c = '?';
    const uint8_t *glyph = font5x7[c - 32];
    for (int col = 0; col < 5; col++) {
        uint8_t coldata = glyph[col];
        for (int row = 0; row < 7; row++) {
            uint16_t color = (coldata & (1 << row)) ? fg : bg;
            if (scale == 1) {
                ST7789_DrawPixel(x + col, y + row, color);
            } else {
                ST7789_Fill(x + col*scale, y + row*scale,
                            x + col*scale + scale - 1,
                            y + row*scale + scale - 1, color);
            }
        }
    }
}

static void draw_string(uint16_t x, uint16_t y, const char *s,
                         uint16_t fg, uint16_t bg, uint8_t scale) {
    while (*s) {
        draw_char(x, y, *s++, fg, bg, scale);
        x += (5 + 1) * scale;
    }
}



/* -----------------------------------------------------------------------
 * Menu screen
 * ----------------------------------------------------------------------- */
typedef enum { MODE_MENU, MODE_CAMERA, MODE_LIBRARY } AppMode;

static void draw_icon_gallery(uint16_t x, uint16_t y, uint16_t col) {
    /* Back photo */
    ST7789_Fill(x+8,  y+6,  x+58, y+46, 0x4208);
    ST7789_DrawRectangle(x+8, y+6, x+58, y+46, col);
    /* Front photo */
    ST7789_Fill(x+2,  y+14, x+52, y+54, 0x2945);
    ST7789_DrawRectangle(x+2, y+14, x+52, y+54, col);
    /* Mountain shape inside front photo */
    ST7789_Fill(x+6,  y+36, x+48, y+50, 0x07E0);  /* green ground */
    /* sky blue fill */
    ST7789_Fill(x+6,  y+18, x+48, y+36, 0x865F);
    /* sun */
    ST7789_Fill(x+36, y+20, x+44, y+28, YELLOW);
    /* mountain peak */
    ST7789_DrawLine(x+16, y+36, x+27, y+22, WHITE);
    ST7789_DrawLine(x+27, y+22, x+38, y+36, WHITE);
}

static void draw_icon_camera(uint16_t x, uint16_t y, uint16_t col) {
    /* Camera body */
    ST7789_Fill(x+2,  y+16, x+58, y+54, 0x4208);
    ST7789_DrawRectangle(x+2, y+16, x+58, y+54, col);
    /* Viewfinder bump */
    ST7789_Fill(x+18, y+10, x+42, y+18, 0x4208);
    ST7789_DrawRectangle(x+18, y+10, x+42, y+18, col);
    /* Lens outer */
    ST7789_DrawCircle(x+30, y+35, 14, col);
    ST7789_Fill(x+17, y+22, x+43, y+48, 0x2945);
    ST7789_DrawCircle(x+30, y+35, 14, col);
    /* Lens inner */
    ST7789_DrawFilledCircle(x+30, y+35, 9, 0x035F);
    ST7789_DrawCircle(x+30, y+35, 9, col);
    /* Sheen dot */
    ST7789_Fill(x+24, y+27, x+28, y+31, 0xFFFF);
    /* Flash dot */
    ST7789_Fill(x+48, y+20, x+54, y+26, YELLOW);
}

static void draw_menu(int sd_ok) {
    ST7789_Fill_Color(BLACK);

    draw_string(82, 12, "DIGICAM", WHITE, BLACK, 3);

    uint16_t gcol = sd_ok ? WHITE : GRAY;


    ST7789_DrawRectangle(176, 52, 306, 168, gcol);
    draw_icon_gallery(196, 58, gcol);
    draw_string(188, 148, "GALLERY", gcol, BLACK, 2);
    draw_string(20, 166, "PRESS ", gcol, BLACK, 1);
    draw_string(216, 166, "RED", RED, BLACK, 1);
    draw_string(80, 166, " BUTTON", gcol, BLACK, 1);

    ST7789_DrawRectangle(14, 52, 144, 168, WHITE);
    draw_icon_camera(34, 58, WHITE);
    draw_string(28, 148, "CAMERA", WHITE, BLACK, 2);
    draw_string(180, 166, "PRESS ", WHITE, BLACK, 1);
    draw_string(56, 166, "BLUE", sd_ok ? BLUE : GRAY, BLACK, 1);
    draw_string(245, 166, " BUTTON", WHITE, BLACK, 1);
}

/* -----------------------------------------------------------------------
 * Camera mode
 * ----------------------------------------------------------------------- */
static void draw_camera_ready(void) {
    ST7789_Fill_Color(BLACK);
    ST7789_DrawRectangle(0, 0, ST7789_WIDTH-1, ST7789_HEIGHT-1, 0x07E0);
    draw_string(4, ST7789_HEIGHT-10, "A0:SHOOT  A3:MENU", GRAY, BLACK, 1);
}

static void run_camera_mode(int sd_ok) {
    draw_camera_ready();

    while (1) {
        char btn = wait_btn(BTN_A, BTN_B);
        if (btn == 'B') return;

        ST7789_DrawRectangle(0, 0, ST7789_WIDTH-1, ST7789_HEIGHT-1, WHITE);
        delay_ms(80);
        ST7789_DrawRectangle(0, 0, ST7789_WIDTH-1, ST7789_HEIGHT-1, BLACK);
        ST7789_Fill_Color(BLACK);
        draw_string(140, 108, "...", YELLOW, BLACK, 3);

        int ok = 0;
        while (!ok) {
            if (camera_capture() != 0) {
                print("cap timeout\r\n");
                continue;
            }
            if (load_fifo() == 0) {
                ok = 1;
            } else {
                print("fifo err, recapturing\r\n");
            }
        }

        decode_to_display();

        if (sd_ok) {
            save_to_sd();
            draw_string(2, 2, "SAVED", GREEN, BLACK, 1);
        } else {
            draw_string(2, 2, "NO SD", GRAY, BLACK, 1);
        }
        draw_string(2, ST7789_HEIGHT-10, "A0:AGAIN  A3:MENU", WHITE, BLACK, 1);

        btn = wait_btn(BTN_A, BTN_B);
        if (btn == 'B') return;
        draw_camera_ready();
    }
}

/* -----------------------------------------------------------------------
 * Library mode
 * ----------------------------------------------------------------------- */
#define MAX_PHOTOS 64

static char photo_names[MAX_PHOTOS][13];
static int  photo_count = 0;
static int  photo_idx   = 0;

static int scan_photos(void) {
    photo_count = 0;
    DIR dir;
    FILINFO fno;
    if (f_opendir(&dir, "/") != FR_OK) return -1;
    while (photo_count < MAX_PHOTOS) {
        if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) break;
        int len = strlen(fno.fname);
        if (len > 4 &&
            (fno.fname[len-4] == '.' ) &&
            (fno.fname[len-3] == 'J' || fno.fname[len-3] == 'j') &&
            (fno.fname[len-2] == 'P' || fno.fname[len-2] == 'p') &&
            (fno.fname[len-1] == 'G' || fno.fname[len-1] == 'g')) {
            strncpy(photo_names[photo_count], fno.fname, 12);
            photo_names[photo_count][12] = '\0';
            photo_count++;
        }
    }
    f_closedir(&dir);
    return photo_count;
}

static void show_photo(int idx) {
    if (idx < 0 || idx >= photo_count) return;
    ST7789_Fill_Color(BLACK);

    if (load_from_sd(photo_names[idx]) != 0) {
        draw_string(80, 112, "LOAD ERR", RED, BLACK, 2);
        return;
    }
    decode_to_display();

    char info[32];
    snprintf(info, sizeof(info), "%s  %d/%d", photo_names[idx], idx+1, photo_count);
    ST7789_Fill(0, ST7789_HEIGHT-12, ST7789_WIDTH-1, ST7789_HEIGHT-1, BLACK);
    draw_string(4, ST7789_HEIGHT-10, info, WHITE, BLACK, 1);
    draw_string(4, 2, "A0:NEXT  A3:MENU", WHITE, BLACK, 1);
}

static void run_library_mode(void) {
    ST7789_Fill_Color(BLACK);
    draw_string(100, 108, "SCANNING", WHITE, BLACK, 2);

    scan_photos();

    if (photo_count == 0) {
        ST7789_Fill_Color(BLACK);
        draw_string(70, 100, "NO PHOTOS", WHITE, BLACK, 2);
        draw_string(70, 122, "A3: BACK", GRAY, BLACK, 1);
        wait_btn(NO_BTN, BTN_B);
        return;
    }

    photo_idx = 0;
    show_photo(photo_idx);

    while (1) {
        char btn = wait_btn(BTN_A, BTN_B);
        if (btn == 'B') return;
        photo_idx = (photo_idx + 1) % photo_count;
        show_photo(photo_idx);
    }
}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
int main(void) {
    __enable_irq();
    SystemClock_Config();
    host_serial_init();
    delay_ms(100);

    print("boot\r\n");

    gpio_config_mode(CAM_CS,     OUTPUT); gpio_write(CAM_CS,     true);
    gpio_config_mode(LCD_CS_PIN, OUTPUT); gpio_write(LCD_CS_PIN, true);
    gpio_config_mode(SD_CS,      OUTPUT); gpio_write(SD_CS,      true);

    spi_init();  

    if (camera_init() != 0) {
        print("ERR: camera_init\r\n");
        while (1);
    }
    print("camera ok\r\n");

    ST7789_GPIO_Init();
    ST7789_Init();
    ST7789_Fill_Color(BLACK);
    print("display ok\r\n");

    int sd_ok = (sd_mount() == 0);
    print(sd_ok ? "SD ok\r\n" : "SD not found\r\n");

    if (sd_ok) {
        scan_photos();
        file_counter = (uint32_t)photo_count;
    }

    gpio_config_mode(BTN_A, INPUT); gpio_config_pullup(BTN_A, PULL_UP);
    gpio_config_mode(BTN_B, INPUT); gpio_config_pullup(BTN_B, PULL_UP);

    print("ready\r\n");

    while (1) {
        draw_menu(sd_ok);

        AppMode mode;
        char btn = wait_btn(BTN_A, BTN_B);
        if (btn == 'A') {
            mode = MODE_CAMERA;
        } else {
            mode = sd_ok ? MODE_LIBRARY : MODE_MENU;
        }

        if (mode == MODE_CAMERA)  run_camera_mode(sd_ok);
        if (mode == MODE_LIBRARY) run_library_mode();
    }
}
