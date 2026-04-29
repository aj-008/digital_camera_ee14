#include "jpeg_display.h"

typedef struct {
    FIL *file;
    uint16_t x_offset;
    uint16_t y_offset;
} jpeg_dev_t;

static size_t jpeg_input(JDEC *jd, uint8_t *buf, size_t nd) {
    jpeg_dev_t *dev = (jpeg_dev_t*)jd->device;
    UINT rb = 0;
    if (buf) {
        f_read(dev->file, buf, nd, &rb);
        return rb;
    }
    f_lseek(dev->file, f_tell(dev->file) + nd);
    return nd;
}

static int jpeg_output(JDEC *jd, void *bitmap, JRECT *rect) {
    jpeg_dev_t *dev = (jpeg_dev_t*)jd->device;
    uint16_t *pixels = (uint16_t*)bitmap;

    

    for (uint16_t y = rect->top; y <= rect->bottom; y++) {
        for (uint16_t x = rect->left; x <= rect->right; x++) {
            uint16_t color = *pixels++;
            // Replace this with your actual TFT draw call:
            // TFT_DrawPixel(dev->x_offset + x, dev->y_offset + y, color);
        }
    }
    return 1;
}

void display_jpeg(const char *path) {
    static uint8_t work[4096];
    JDEC jdec;
    FIL file;
    jpeg_dev_t dev;

    if (f_open(&file, path, FA_READ) != FR_OK) return;

    dev.file     = &file;
    dev.x_offset = 0;
    dev.y_offset = 0;

    if (jd_prepare(&jdec, jpeg_input, work, sizeof(work), &dev) == JDR_OK) {
        jd_decomp(&jdec, jpeg_output, 0);
    }

    f_close(&file);
}