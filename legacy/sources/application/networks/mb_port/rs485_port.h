#ifndef __RS485_PORT_H__
#define __RS485_PORT_H__

/* Lop port nanoMODBUS tren USART2 + chan DIR RS485 (PA2/PA3, xem io_cfg.h).
 * Nhan: ngat RXNE day byte vao ring buffer. Gui: polling TXE/TC, bat DIR
 * truoc khi gui, tra DIR ve nhan khi TC. Khong dung timer (TIM4 bo trong). */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void rs485_port_init(uint32_t baudrate);
extern int32_t rs485_port_read(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
extern int32_t rs485_port_write(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
extern void rs485_port_irq(void);	/* goi tu rs485_irq() trong system.c */

#ifdef __cplusplus
}
#endif

#endif /* __RS485_PORT_H__ */
