#include "jpeg_display.h"
#include "st7789.h"
#include <stdio.h>



size_t jpeg_input(JDEC *jd, uint8_t *buf, size_t nd) {
    jpeg_dev_t *dev = (jpeg_dev_t*)jd->device;
    UINT rb = 0;
    if (buf) {
        f_read(dev->file, buf, nd, &rb);
        return rb;
    }
    f_lseek(dev->file, f_tell(dev->file) + nd);
    return nd;
}

int jpeg_output(JDEC *jd, void *bitmap, JRECT *rect) {
    jpeg_dev_t *dev = (jpeg_dev_t*)jd->device;
    uint16_t *pixels = (uint16_t*)bitmap;

    for (uint16_t y = rect->top; y <= rect->bottom; y++) {
        for (uint16_t x = rect->left; x <= rect->right; x++) {
            ST7789_DrawPixel(dev->x_offset + x, dev->y_offset + y, *pixels++);
        }
    }
    return 1;
}

void display_jpeg(const char *path) {
    static uint8_t work[4096];
    JDEC jdec;
    FIL file;
    jpeg_dev_t dev;
    char status[32];
    int y_pos = 5;

    ST7789_Fill_Color(BLACK);

    // --- Open ---
    FRESULT fres = f_open(&file, path, FA_READ);
    snprintf(status, sizeof(status), "Open: %d", fres);
    ST7789_WriteString(5, y_pos, status, Font_7x10, fres == FR_OK ? GREEN : RED, BLACK);
    y_pos += 15;
    if (fres != FR_OK) return;

    snprintf(status, sizeof(status), "Size: %lu", (uint32_t)f_size(&file));
    ST7789_WriteString(5, y_pos, status, Font_7x10, WHITE, BLACK);
    y_pos += 15;

    dev.file     = &file;
    dev.x_offset = 0;
    dev.y_offset = 0;

    // --- Prepare ---
    JRESULT jres = jd_prepare(&jdec, jpeg_input, work, sizeof(work), &dev);
    snprintf(status, sizeof(status), "Prepare: %d", jres);
    ST7789_WriteString(5, y_pos, status, Font_7x10, jres == JDR_OK ? GREEN : RED, BLACK);
    y_pos += 15;
    if (jres != JDR_OK) {
        f_close(&file);
        return;
    }

    // --- Show dimensions ---
    snprintf(status, sizeof(status), "Dim: %dx%d", jdec.width, jdec.height);
    ST7789_WriteString(5, y_pos, status, Font_7x10, YELLOW, BLACK);
    y_pos += 15;

    HAL_Delay(2000); // read the status before image draws over it

    // --- Decompress ---
    ST7789_Fill_Color(BLACK);
    jres = jd_decomp(&jdec, jpeg_output, 0);
    if (jres != JDR_OK) {
        snprintf(status, sizeof(status), "Decomp: %d", jres);
        ST7789_WriteString(5, 5, status, Font_7x10, RED, BLACK);
    }

    f_close(&file);
}