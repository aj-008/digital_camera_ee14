#ifndef JPEG_DISPLAY_H
#define JPEG_DISPLAY_H

#include "ff.h"
#include "tjpgd.h"

typedef struct {
    FIL *file;
    uint16_t x_offset;
    uint16_t y_offset;
} jpeg_dev_t;

size_t jpeg_input(JDEC *jd, uint8_t *buf, size_t nd);  // expose so test can use it
void display_jpeg(const char *path);



#endif