/**
 ******************************************************************************
 * @brief:  HAL - half-duplex RS485 port (one per board), used by the Modbus
 *          services. Optional: a port without RS485 does not implement it and
 *          the build does not include services/modbus.
 *
 *  8 data bits, no parity, 1 stop bit. RX is interrupt driven into a ring
 *  buffer. TX drives the direction pin: high before the first byte, low again
 *  once the last stop bit has left the wire.
 ******************************************************************************
**/

#ifndef __HAL_RS485_H__
#define __HAL_RS485_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

extern void hal_rs485_init(uint32_t baudrate);

/* One received byte, -1 if none. Never blocks. */
extern int hal_rs485_getc(void);

/* Send and return when the last stop bit is out and the bus is released.
 * Blocks for the transmit time (1 ms per byte at 9600 baud). */
extern void hal_rs485_write(const uint8_t* data, uint32_t len);

/* Bytes received since init, including those of frames that were dropped.
 * Diagnostics: bytes arriving but no frame answered = wrong baud rate or
 * A/B swapped; no bytes at all = no signal on the receiver. */
extern uint32_t hal_rs485_rx_bytes(void);

#ifdef __cplusplus
}
#endif

#endif /* __HAL_RS485_H__ */
