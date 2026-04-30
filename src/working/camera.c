#include "camera.h"
#include "spi.h"
#include "ee14lib.h"
#include <stddef.h>

/* PROGMEM no-op must come before the regs header */
#ifndef PROGMEM
#define PROGMEM
#endif
#include "ov2640_regs.h"

/* -----------------------------------------------------------------------
 * Internal SPI helpers (ArduChip register bus)
 * ----------------------------------------------------------------------- */
static uint8_t spi_read_reg(uint8_t addr) {
    uint8_t val;
    spi_cs_low(CAM_CS);
    spi_transfer(addr & 0x7F);
    val = spi_transfer(0x00);
    spi_cs_high(CAM_CS);
    return val;
}

static void spi_write_reg(uint8_t addr, uint8_t val) {
    spi_cs_low(CAM_CS);
    spi_transfer(addr | 0x80);
    spi_transfer(val);
    spi_cs_high(CAM_CS);
}

/* -----------------------------------------------------------------------
 * Internal I2C helpers (OV2640 sensor config via SCCB)
 * Uses I2C1 on PB6=SCL, PB7=SDA (5V tolerant pins)
 * ----------------------------------------------------------------------- */
static void i2c_init_peripheral(void) {
    RCC->APB1ENR1 |= RCC_APB1ENR1_I2C1EN;
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOAEN;  /* PA9/PA10 are on GPIOA */

    gpio_config_alternate_function(CAM_SCL, 4);  /* PA9  AF4 */
    gpio_config_alternate_function(CAM_SDA, 4);  /* PA10 AF4 */
    GPIOA->OTYPER |= (1 << 9) | (1 << 10);   /* open-drain */
    GPIOA->PUPDR  &= ~((3UL << (9*2)) | (3UL << (10*2)));
    GPIOA->PUPDR  |=   (1UL << (9*2)) | (1UL << (10*2));  /* pull-up */
    GPIOA->OSPEEDR &= ~((3UL << (9*2)) | (3UL << (10*2)));
    GPIOA->OSPEEDR |=   (1UL << (9*2)) | (1UL << (10*2)); /* medium speed */

    I2C1->CR1 &= ~I2C_CR1_PE;

    /* TIMINGR: 100 kHz at 80 MHz SYSCLK */
    I2C1->TIMINGR = (0U    << 28) |  /* PRESC  = 0 */
                    (1U    << 20) |  /* SCLDEL = 1 */
                    (0U    << 16) |  /* SDADEL = 0 */
                    (0xFU  <<  8) |  /* SCLH   = 15 */
                    (0x13U      );   /* SCLL   = 19 */

    I2C1->CR1 |= I2C_CR1_PE;
}

static int i2c_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t val) {
    uint32_t timeout;

    timeout = 100000;
    while ((I2C1->ISR & I2C_ISR_BUSY) && --timeout);
    if (!timeout) return -1;

    I2C1->CR2 = (dev_addr << 1)
              | (2U << I2C_CR2_NBYTES_Pos)
              | I2C_CR2_AUTOEND;
    I2C1->CR2 &= ~I2C_CR2_RD_WRN;
    I2C1->CR2 |=  I2C_CR2_START;

    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_TXIS) && --timeout);
    if (!timeout) return -1;
    I2C1->TXDR = reg;

    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_TXIS) && --timeout);
    if (!timeout) return -1;
    I2C1->TXDR = val;

    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_STOPF) && --timeout);
    I2C1->ICR |= I2C_ICR_STOPCF;

    return timeout ? 0 : -1;
}

static int write_sensor_regs(const struct sensor_reg *regs) {
    while (!(regs->reg == 0xFF && regs->val == 0xFF)) {
        if (i2c_write_reg(OV2640_I2C_ADDR, regs->reg, regs->val) != 0)
            return -1;
        regs++;
    }
    return 0;
}

static void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 8000; i++) __NOP();
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */
int camera_init(void) {
    /* CS as output, deasserted */
    gpio_config_mode(CAM_CS, OUTPUT);
    spi_cs_high(CAM_CS);
    spi_init();
    delay_ms(100);

    /* SPI self-test */
    spi_write_reg(ARDUCHIP_TEST1, 0x55);
    if (spi_read_reg(ARDUCHIP_TEST1) != 0x55) return -1;

    /* OV2640 sensor init over I2C */
    i2c_init_peripheral();

    i2c_write_reg(OV2640_I2C_ADDR, 0xFF, 0x01);  /* sensor bank */
    i2c_write_reg(OV2640_I2C_ADDR, 0x12, 0x80);  /* soft reset  */
    delay_ms(100);

    if (write_sensor_regs(OV2640_JPEG_INIT)    != 0) return -2;
    if (write_sensor_regs(OV2640_YUV422)       != 0) return -2;
    if (write_sensor_regs(OV2640_JPEG)         != 0) return -2;
    if (write_sensor_regs(OV2640_320x240_JPEG) != 0) return -2;

    return 0;
}

int camera_capture(void) {
    /* Reset write pointer, clear FIFO, reset read pointer */
    spi_write_reg(ARDUCHIP_FIFO, FIFO_WRPTR_RST_MASK);
    spi_write_reg(ARDUCHIP_FIFO, FIFO_CLEAR_MASK);
    spi_write_reg(ARDUCHIP_FIFO, FIFO_RDPTR_RST_MASK);
    delay_ms(10);

    /* Arm capture */
    spi_write_reg(ARDUCHIP_FIFO, FIFO_START_MASK);

    for (uint32_t t = 0; t < 3000; t++) {
        if (spi_read_reg(ARDUCHIP_TRIG) & CAP_DONE_MASK) {
            delay_ms(100);
            return 0;
        }
        delay_ms(1);
    }
    return -1;
}

void camera_fifo_reset_rdptr(void) {
    spi_write_reg(ARDUCHIP_FIFO, FIFO_RDPTR_RST_MASK);
}

uint32_t camera_fifo_length(void) {
    uint32_t len = 0;
    len  = (uint32_t)(spi_read_reg(0x44) & 0x7F) << 16;
    len |= (uint32_t) spi_read_reg(0x43)          <<  8;
    len |=             spi_read_reg(0x42);
    return len;
}

int camera_dump_uart(USART_TypeDef *uart) {
    uint32_t length = camera_fifo_length();
    if (length == 0 || length > 0x7FFFFF) return -1;

    /* Send raw FIFO bytes — no swap, no EOI scanning.
     * Python script handles byte-swap and EOI detection. */
    uint8_t header[8] = {
        0xFF, 0xAA,
        (length >> 16) & 0xFF,
        (length >>  8) & 0xFF,
        (length      ) & 0xFF,
        0xBB, 0xCC, 0xDD
    };
    serial_write(uart, (char *)header, sizeof(header));

    spi_cs_low(CAM_CS);
    spi_transfer(ARDUCHIP_BURST_FIFO);
    spi_transfer(0x00);  /* dummy byte */

    uint8_t chunk[64];
    uint32_t remaining = length;
    while (remaining > 0) {
        uint32_t n = (remaining > sizeof(chunk)) ? sizeof(chunk) : remaining;
        spi_transfer_buf(NULL, chunk, (uint16_t)n);
        serial_write(uart, (char *)chunk, n);
        remaining -= n;
    }

    spi_cs_high(CAM_CS);

    uint8_t footer[4] = { 0xFF, 0xBB, 0xFF, 0xBB };
    serial_write(uart, (char *)footer, sizeof(footer));

    return 0;
}
