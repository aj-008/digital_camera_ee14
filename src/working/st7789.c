#include "st7789.h"
#include "spi.h"
#include "ee14lib.h"

/*
 * st7789.c — ST7789 driver ported to bare-metal SPI1
 *
 * All HAL_SPI_Transmit() calls replaced with spi_transfer_buf().
 * All HAL_GPIO_WritePin() calls replaced with gpio_write() / spi_cs_*.
 * HAL_Delay() replaced with local spin-wait.
 * ST7789_GPIO_Init() added — call once from main() before ST7789_Init().
 */

static void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 8000; i++) __NOP();
}

/* Initialise the three LCD control GPIO pins (CS, DC, RST) */
void ST7789_GPIO_Init(void) {
    gpio_config_mode(LCD_CS_PIN,  OUTPUT);
    gpio_config_mode(LCD_DC_PIN,  OUTPUT);
    gpio_config_mode(LCD_RST_PIN, OUTPUT);
    ST7789_UnSelect();
    ST7789_DC_Set();
    ST7789_RST_Set();
}

/* ── Internal helpers ────────────────────────────────────────────────── */

static void ST7789_WriteCommand(uint8_t cmd) {
    ST7789_Select();
    ST7789_DC_Clr();
    spi_transfer(cmd);
    ST7789_UnSelect();
}

static void ST7789_WriteData(const uint8_t *buff, size_t buff_size) {
    ST7789_Select();
    ST7789_DC_Set();
    while (buff_size > 0) {
        uint16_t chunk = (buff_size > 0xFFFF) ? 0xFFFF : (uint16_t)buff_size;
        spi_transfer_buf(buff, NULL, chunk);
        buff      += chunk;
        buff_size -= chunk;
    }
    ST7789_UnSelect();
}

static void ST7789_WriteSmallData(uint8_t data) {
    ST7789_Select();
    ST7789_DC_Set();
    spi_transfer(data);
    ST7789_UnSelect();
}

static void ST7789_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint16_t xs = x0 + X_SHIFT, xe = x1 + X_SHIFT;
    uint16_t ys = y0 + Y_SHIFT, ye = y1 + Y_SHIFT;

    ST7789_WriteCommand(ST7789_CASET);
    { uint8_t d[] = { xs >> 8, xs & 0xFF, xe >> 8, xe & 0xFF };
      ST7789_WriteData(d, 4); }

    ST7789_WriteCommand(ST7789_RASET);
    { uint8_t d[] = { ys >> 8, ys & 0xFF, ye >> 8, ye & 0xFF };
      ST7789_WriteData(d, 4); }

    ST7789_WriteCommand(ST7789_RAMWR);
}

/* ── Public API ─────────────────────────────────────────────────────── */

void ST7789_SetRotation(uint8_t m) {
    ST7789_WriteCommand(ST7789_MADCTL);
    switch (m) {
    case 0: ST7789_WriteSmallData(ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB); break;
    case 1: ST7789_WriteSmallData(ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB); break;
    case 2: ST7789_WriteSmallData(ST7789_MADCTL_RGB); break;
    case 3: ST7789_WriteSmallData(ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB); break;
    }
}

void ST7789_Init(void) {
    /* Hardware reset */
    ST7789_RST_Clr(); delay_ms(25);
    ST7789_RST_Set(); delay_ms(50);

    ST7789_WriteCommand(ST7789_SWRESET); delay_ms(150);
    ST7789_WriteCommand(ST7789_SLPOUT);  delay_ms(500);
    ST7789_WriteCommand(ST7789_COLMOD);
    ST7789_WriteSmallData(ST7789_COLOR_MODE_16bit);
    delay_ms(10);
    ST7789_WriteCommand(ST7789_NORON);   delay_ms(10);
    ST7789_SetRotation(ST7789_ROTATION);
    ST7789_WriteCommand(ST7789_INVON);   delay_ms(10);
    ST7789_WriteCommand(ST7789_DISPON);  delay_ms(500);
}

void ST7789_Fill_Color(uint16_t color) {
    ST7789_Fill(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1, color);
}

void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= ST7789_WIDTH || y >= ST7789_HEIGHT) return;
    ST7789_SetAddressWindow(x, y, x, y);
    uint8_t d[2] = { color >> 8, color & 0xFF };
    ST7789_WriteData(d, 2);
}

void ST7789_Fill(uint16_t xSta, uint16_t ySta, uint16_t xEnd, uint16_t yEnd, uint16_t color) {
    uint8_t hi = color >> 8, lo = color & 0xFF;
    ST7789_SetAddressWindow(xSta, ySta, xEnd, yEnd);
    ST7789_Select();
    ST7789_DC_Set();
    uint32_t pixels = (uint32_t)(xEnd - xSta + 1) * (yEnd - ySta + 1);
    for (uint32_t i = 0; i < pixels; i++) {
        spi_transfer(hi);
        spi_transfer(lo);
    }
    ST7789_UnSelect();
}

void ST7789_DrawPixel_4px(uint16_t x, uint16_t y, uint16_t color) {
    ST7789_Fill(x, y, x + 1, y + 1, color);
}

void ST7789_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data) {
    if ((x + w) > ST7789_WIDTH || (y + h) > ST7789_HEIGHT) return;
    ST7789_SetAddressWindow(x, y, x + w - 1, y + h - 1);
    ST7789_WriteData((const uint8_t *)data, (size_t)w * h * 2);
}

void ST7789_InvertColors(uint8_t invert) {
    ST7789_WriteCommand(invert ? ST7789_INVON : ST7789_INVOFF);
}

void ST7789_TearEffect(uint8_t tear) {
    ST7789_WriteCommand(tear ? 0x35 : 0x34);
}

/* ── Drawing primitives ─────────────────────────────────────────────── */

void ST7789_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    int16_t dx = ABS((int16_t)x2 - x1), sx = x1 < x2 ? 1 : -1;
    int16_t dy = ABS((int16_t)y2 - y1), sy = y1 < y2 ? 1 : -1;
    int16_t err = (dx > dy ? dx : -dy) / 2, e2;
    for (;;) {
        ST7789_DrawPixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        e2 = err;
        if (e2 > -dx) { err -= dy; x1 += sx; }
        if (e2 <  dy) { err += dx; y1 += sy; }
    }
}

void ST7789_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color) {
    ST7789_DrawLine(x1, y1, x2, y1, color);
    ST7789_DrawLine(x1, y2, x2, y2, color);
    ST7789_DrawLine(x1, y1, x1, y2, color);
    ST7789_DrawLine(x2, y1, x2, y2, color);
}

void ST7789_DrawFilledRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    ST7789_Fill(x, y, x + w - 1, y + h - 1, color);
}

void ST7789_DrawCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color) {
    int16_t f = 1 - r, ddF_x = 1, ddF_y = -2 * r, x = 0, y = r;
    ST7789_DrawPixel(x0, y0 + r, color); ST7789_DrawPixel(x0, y0 - r, color);
    ST7789_DrawPixel(x0 + r, y0, color); ST7789_DrawPixel(x0 - r, y0, color);
    while (x < y) {
        if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
        x++; ddF_x += 2; f += ddF_x;
        ST7789_DrawPixel(x0 + x, y0 + y, color); ST7789_DrawPixel(x0 - x, y0 + y, color);
        ST7789_DrawPixel(x0 + x, y0 - y, color); ST7789_DrawPixel(x0 - x, y0 - y, color);
        ST7789_DrawPixel(x0 + y, y0 + x, color); ST7789_DrawPixel(x0 - y, y0 + x, color);
        ST7789_DrawPixel(x0 + y, y0 - x, color); ST7789_DrawPixel(x0 - y, y0 - x, color);
    }
}

void ST7789_DrawFilledCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    ST7789_DrawLine(x0, y0 - r, x0, y0 + r, color);
    int16_t f = 1 - r, ddF_x = 1, ddF_y = -2 * r, x = 0, y = r;
    while (x < y) {
        if (f >= 0) { y--; ddF_y += 2; f += ddF_y; }
        x++; ddF_x += 2; f += ddF_x;
        ST7789_DrawLine(x0 + x, y0 - y, x0 + x, y0 + y, color);
        ST7789_DrawLine(x0 - x, y0 - y, x0 - x, y0 + y, color);
        ST7789_DrawLine(x0 + y, y0 - x, x0 + y, y0 + x, color);
        ST7789_DrawLine(x0 - y, y0 - x, x0 - y, y0 + x, color);
    }
}

void ST7789_DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                          uint16_t x3, uint16_t y3, uint16_t color) {
    ST7789_DrawLine(x1, y1, x2, y2, color);
    ST7789_DrawLine(x2, y2, x3, y3, color);
    ST7789_DrawLine(x3, y3, x1, y1, color);
}

void ST7789_DrawFilledTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                                uint16_t x3, uint16_t y3, uint16_t color) {
    /* Sort vertices by y */
    #define SWAP16(a,b) { uint16_t t = a; a = b; b = t; }
    if (y1 > y2) { SWAP16(y1,y2); SWAP16(x1,x2); }
    if (y1 > y3) { SWAP16(y1,y3); SWAP16(x1,x3); }
    if (y2 > y3) { SWAP16(y2,y3); SWAP16(x2,x3); }
    #undef SWAP16

    int16_t dx12 = x2-x1, dy12 = y2-y1;
    int16_t dx13 = x3-x1, dy13 = y3-y1;
    int16_t dx23 = x3-x2, dy23 = y3-y2;
    for (int16_t y = y1; y <= y3; y++) {
        int16_t xa, xb;
        if (y < y2) {
            xa = dy12 ? x1 + (int16_t)((int32_t)dx12*(y-y1)/dy12) : x1;
            xb = dy13 ? x1 + (int16_t)((int32_t)dx13*(y-y1)/dy13) : x1;
        } else {
            xa = dy23 ? x2 + (int16_t)((int32_t)dx23*(y-y2)/dy23) : x2;
            xb = dy13 ? x1 + (int16_t)((int32_t)dx13*(y-y1)/dy13) : x1;
        }
        if (xa > xb) { int16_t t = xa; xa = xb; xb = t; }
        ST7789_DrawLine(xa, y, xb, y, color);
    }
}

void ST7789_WriteChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor) {
    /* Requires a fonts.h implementation — stub provided */
    (void)x; (void)y; (void)ch; (void)font; (void)color; (void)bgcolor;
}

void ST7789_WriteString(uint16_t x, uint16_t y, const char *str, FontDef font, uint16_t color, uint16_t bgcolor) {
    while (*str) {
        ST7789_WriteChar(x, y, *str, font, color, bgcolor);
        x += font.width;
        str++;
    }
}

void ST7789_Test(void) {
    ST7789_Fill_Color(RED);   delay_ms(500);
    ST7789_Fill_Color(GREEN); delay_ms(500);
    ST7789_Fill_Color(BLUE);  delay_ms(500);
    ST7789_Fill_Color(BLACK); delay_ms(500);
}
