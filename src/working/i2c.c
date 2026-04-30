#include "i2c.h"
#include "stm32l432xx.h"

/*
 * i2c.c — bare-metal I2C1 driver, STM32L432KC
 *
 * Pins: PB6=SCL (D5, AF4), PB7=SDA (D4, AF4) — 5V-tolerant.
 * These are the only I2C pins used by this project (OV2640 SCCB).
 * camera.c no longer has its own i2c_init_peripheral(); call i2c1_init()
 * from main before camera_init().
 *
 * TIMINGR: 100 kHz standard mode, PCLK1 = 80 MHz (per AN4235)
 */

#define I2C_TIMING  0x10420F13
#define I2C_TIMEOUT 100000UL

static bool wait_set(volatile uint32_t *reg, uint32_t flag) {
    uint32_t t = I2C_TIMEOUT;
    while (!(*reg & flag))
        if (--t == 0) return false;
    return true;
}

static bool wait_clear(volatile uint32_t *reg, uint32_t flag) {
    uint32_t t = I2C_TIMEOUT;
    while (*reg & flag)
        if (--t == 0) return false;
    return true;
}

static void recover(void) {
    I2C1->CR1 &= ~I2C_CR1_PE;
    for (volatile int i = 0; i < 500; i++);
    I2C1->CR1 |= I2C_CR1_PE;
    wait_clear(&I2C1->ISR, I2C_ISR_BUSY);
}

void i2c1_init(void) {
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOBEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_I2C1EN;

    /* I2C1 clock source = SYSCLK */
    RCC->CCIPR &= ~RCC_CCIPR_I2C1SEL;
    RCC->CCIPR |=  RCC_CCIPR_I2C1SEL_0;

    /* PB6=SCL, PB7=SDA: AF4, open-drain, pull-up */
    GPIOB->AFR[0] &= ~((0xFUL << 24) | (0xFUL << 28));
    GPIOB->AFR[0] |=  (4UL << 24) | (4UL << 28);
    GPIOB->MODER  &= ~((3UL << 12) | (3UL << 14));
    GPIOB->MODER  |=  (2UL << 12) | (2UL << 14);   /* alternate function */
    GPIOB->OTYPER |=  (1UL << 6)  | (1UL << 7);    /* open-drain */
    GPIOB->PUPDR  &= ~((3UL << 12) | (3UL << 14));
    GPIOB->PUPDR  |=  (1UL << 12) | (1UL << 14);   /* pull-up */
    GPIOB->OSPEEDR &= ~((3UL << 12) | (3UL << 14)); /* low speed is fine for 100 kHz */

    I2C1->CR1    &= ~I2C_CR1_PE;
    I2C1->CR1     = 0;
    I2C1->CR2     = 0;
    I2C1->TIMINGR = I2C_TIMING;
    I2C1->CR1    |= I2C_CR1_PE;
}

bool i2c1_scan(uint8_t addr) {
    I2C1->ICR = 0x3F38;
    I2C1->CR2 = ((uint32_t)addr << 1)
              | (0UL << I2C_CR2_NBYTES_Pos)
              | I2C_CR2_AUTOEND
              | I2C_CR2_START;

    uint32_t t = I2C_TIMEOUT;
    while (!(I2C1->ISR & I2C_ISR_STOPF) && !(I2C1->ISR & I2C_ISR_NACKF))
        if (--t == 0) { recover(); return false; }

    bool ack = !(I2C1->ISR & I2C_ISR_NACKF);
    I2C1->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF;
    wait_clear(&I2C1->ISR, I2C_ISR_BUSY);
    return ack;
}

bool i2c1_write(uint8_t addr, const uint8_t *buf, uint8_t len) {
    I2C1->ICR = 0x3F38;
    I2C1->CR2 = ((uint32_t)addr << 1)
              | ((uint32_t)len  << I2C_CR2_NBYTES_Pos)
              | I2C_CR2_AUTOEND
              | I2C_CR2_START;

    for (uint8_t i = 0; i < len; i++) {
        if (!wait_set(&I2C1->ISR, I2C_ISR_TXIS)) { recover(); return false; }
        I2C1->TXDR = buf[i];
    }
    if (!wait_set(&I2C1->ISR, I2C_ISR_STOPF)) { recover(); return false; }
    I2C1->ICR = I2C_ICR_STOPCF;
    wait_clear(&I2C1->ISR, I2C_ISR_BUSY);
    return true;
}

bool i2c1_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    return i2c1_write(addr, buf, 2);
}

bool i2c1_read_reg(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len) {
    /* Tx phase: write register address */
    I2C1->ICR = 0x3F38;
    I2C1->CR2 = ((uint32_t)addr << 1)
              | (1UL << I2C_CR2_NBYTES_Pos)
              | I2C_CR2_AUTOEND
              | I2C_CR2_START;

    if (!wait_set(&I2C1->ISR, I2C_ISR_TXIS))  { recover(); return false; }
    I2C1->TXDR = reg;
    if (!wait_set(&I2C1->ISR, I2C_ISR_STOPF)) { recover(); return false; }
    I2C1->ICR = I2C_ICR_STOPCF;
    wait_clear(&I2C1->ISR, I2C_ISR_BUSY);

    /* Rx phase */
    I2C1->ICR = 0x3F38;
    I2C1->CR2 = ((uint32_t)addr << 1)
              | ((uint32_t)len  << I2C_CR2_NBYTES_Pos)
              | I2C_CR2_AUTOEND
              | I2C_CR2_RD_WRN
              | I2C_CR2_START;

    for (uint8_t i = 0; i < len; i++) {
        if (!wait_set(&I2C1->ISR, I2C_ISR_RXNE)) { recover(); return false; }
        buf[i] = (uint8_t)I2C1->RXDR;
    }
    if (!wait_set(&I2C1->ISR, I2C_ISR_STOPF)) { recover(); return false; }
    I2C1->ICR = I2C_ICR_STOPCF;
    wait_clear(&I2C1->ISR, I2C_ISR_BUSY);
    return true;
}
