<img width="600" height="338" alt="output" src="https://github.com/user-attachments/assets/ba8c9e58-5b83-465b-9aa0-0f723b5d3fcf" />



# Digital Camera — EE14 Final Project
Jack Geraghty, AJ Romeo, Andrew Schretzmayer, Taiyr Ashkenov


A digital camera built on the STM32L432KC microcontroller. The system captures images using an OV2640 image sensor, previews them on an ST7789 display, and writes them to an SD card using the FatFS filesystem.

## Features

- **Image capture** via OV2640 camera sensor
- **SD card storage** using FatFS for image saving
- **Shared SPI bus** between camera and display

## Hardware

| Component | Part |
|---|---|
| Microcontroller | STM32L432KC |
| Image Sensor | OV2640 (Arducam 2MP SPI module) |
| Display | ST7789 TFT LCD, 240×320, SPI |
| Storage | SD card via SPI + FatFS |

## Pinout
 | Signal | Port/Pin | STM32 Pin |
  |--------|----------|-------------|
  | SPI1 SCK | PA5 | A4 |
  | SPI1 MISO | PA6 | A5 |
  | SPI1 MOSI | PA7 | A6 |
  | SPI3 SCK | PB3 | D13 |
  | SPI3 MISO | PB4 | D12 |
  | SPI3 MOSI | PB5 | D11 |
  | CAM_CS | PA8 | D9 |
  | LCD_DC | PB0 | D3 |
  | I2C1 SCL | PB6 | D5 |
  | I2C1 SDA | PB7 | D4 |
  | BTN_A | PA0 | A0 |
  | BTN_B | PA3 | A3 |

