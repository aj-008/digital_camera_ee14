#ifndef CAMERA_H
#define CAMERA_H

#include "stm32l432xx.h"
#include <stdint.h>


#define CAM_CS  D9   

#define CAM_SCL  D1
#define CAM_SDA  D0

#define OV2640_I2C_ADDR  0x30

#define ARDUCHIP_TEST1       0x00
#define ARDUCHIP_FIFO        0x04
#define ARDUCHIP_TRIG        0x41
#define ARDUCHIP_BURST_FIFO  0x3C

#define FIFO_CLEAR_MASK      0x01
#define FIFO_START_MASK      0x02
#define FIFO_RDPTR_RST_MASK  0x10
#define FIFO_WRPTR_RST_MASK  0x20
#define CAP_DONE_MASK        0x08

#ifndef SENSOR_REG_DEFINED
#define SENSOR_REG_DEFINED
struct sensor_reg {
    uint16_t reg;
    uint16_t val;
};
#endif

#ifndef PROGMEM
#define PROGMEM
#endif

int      camera_init(void);
int      camera_capture(void);
uint32_t camera_fifo_length(void);
int      camera_dump_uart(USART_TypeDef *uart);
void     camera_fifo_reset_rdptr(void);


#endif 
