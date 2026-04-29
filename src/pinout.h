#ifndef PINOUT_H
#define PINOUT_H

#include "ee14lib.h"
#include "stm32l4xx.h"

// ---------------- SPI ----------------
#define CAMERA_CS   D7
#define CAMERA_SCK  D13
#define CAMERA_MISO D12 
#define CAMERA_MOSI D6

// ---------------- I2C ----------------
#define CAMERA_SCL  D1
#define CAMERA_SDA  D0

#endif
