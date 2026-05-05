#ifndef __ST7789_H
#define __ST7789_H

#include "stm32l432xx.h"
#include "ee14lib.h"
#include "spi.h"
#include <stdint.h>
#include <stddef.h>


// 320 width, 240 height (rotation is uint8_t 1-4)
#define USING_240X320
#define ST7789_ROTATION  1  

#ifdef USING_240X320
  #define ST7789_WIDTH   320
  #define ST7789_HEIGHT  240
  #define X_SHIFT        0
  #define Y_SHIFT        0
#endif

#define LCD_CS_PIN   D10  
#define LCD_DC_PIN   D3  
#define LCD_RST_PIN  D6 

#define ST7789_Select()    spi_cs_low(LCD_CS_PIN)
#define ST7789_UnSelect()  spi_cs_high(LCD_CS_PIN)
#define ST7789_DC_Clr()    gpio_write(LCD_DC_PIN, false)
#define ST7789_DC_Set()    gpio_write(LCD_DC_PIN, true)
#define ST7789_RST_Clr()   gpio_write(LCD_RST_PIN, false)
#define ST7789_RST_Set()   gpio_write(LCD_RST_PIN, true)

#define ABS(x) ((x) > 0 ? (x) : -(x))

/* RGB565 colour constants */
#define WHITE       0xFFFF
#define BLACK       0x0000
#define BLUE        0x001F
#define RED         0xF800
#define MAGENTA     0xF81F
#define GREEN       0x07E0
#define CYAN        0x7FFF
#define YELLOW      0xFFE0
#define GRAY        0x8430

/* command codes */
#define ST7789_NOP      0x00
#define ST7789_SWRESET  0x01
#define ST7789_SLPIN    0x10
#define ST7789_SLPOUT   0x11
#define ST7789_NORON    0x13
#define ST7789_INVOFF   0x20
#define ST7789_INVON    0x21
#define ST7789_DISPOFF  0x28
#define ST7789_DISPON   0x29
#define ST7789_CASET    0x2A
#define ST7789_RASET    0x2B
#define ST7789_RAMWR    0x2C
#define ST7789_RAMRD    0x2E
#define ST7789_COLMOD   0x3A
#define ST7789_MADCTL   0x36
#define ST7789_PTLAR    0x30

#define ST7789_MADCTL_MY   0x80
#define ST7789_MADCTL_MX   0x40
#define ST7789_MADCTL_MV   0x20
#define ST7789_MADCTL_ML   0x10
#define ST7789_MADCTL_RGB  0x00

#define ST7789_COLOR_MODE_16bit  0x55

typedef struct { uint8_t width; uint8_t height; const uint16_t *data; } FontDef;

void ST7789_Init(void);
void ST7789_SetRotation(uint8_t m);
void ST7789_Fill_Color(uint16_t color);
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ST7789_Fill(uint16_t xSta, uint16_t ySta, uint16_t xEnd, uint16_t yEnd, uint16_t color);
void ST7789_DrawPixel_4px(uint16_t x, uint16_t y, uint16_t color);
void ST7789_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void ST7789_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void ST7789_DrawCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);
void ST7789_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data);
void ST7789_InvertColors(uint8_t invert);
void ST7789_WriteChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor);
void ST7789_WriteString(uint16_t x, uint16_t y, const char *str, FontDef font, uint16_t color, uint16_t bgcolor);
void ST7789_DrawFilledRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7789_DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color);
void ST7789_DrawFilledTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t color);
void ST7789_DrawFilledCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void ST7789_TearEffect(uint8_t tear);
void ST7789_Test(void);

void ST7789_GPIO_Init(void);

#endif
