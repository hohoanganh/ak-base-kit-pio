/**
 ******************************************************************************
 * @brief:  Modbus RTU master over RS485 (nanoMODBUS client).
 *
 *  Every call sends one request and waits for the answer: it blocks for the
 *  frame time plus, at worst, the response timeout. Call it from a task with
 *  a short timeout and one request per message - a slave that does not answer
 *  must not hold the scheduler for seconds.
 *
 *  Return value: 0 = ok, > 0 = Modbus exception code sent by the slave,
 *  < 0 = no valid answer (MB_ERR_TIMEOUT, MB_ERR_CRC, ...).
 ******************************************************************************
**/

#ifndef __MB_MASTER_H__
#define __MB_MASTER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#define MB_OK					(0)
#define MB_ERR_ARG				(-1)
#define MB_ERR_BAD_RESPONSE		(-2)
#define MB_ERR_TIMEOUT			(-3)
#define MB_ERR_TRANSPORT		(-4)
#define MB_ERR_CRC				(-5)
#define MB_ERR_UNIT_ID			(-7)

/* response_timeout_ms: how long to wait for the first byte of an answer. */
extern int mb_master_init(uint32_t baudrate, uint16_t response_timeout_ms);

extern int mb_master_read_holding(uint8_t unit_id, uint16_t address, uint16_t quantity, uint16_t* out);
extern int mb_master_read_input(uint8_t unit_id, uint16_t address, uint16_t quantity, uint16_t* out);
extern int mb_master_write_single(uint8_t unit_id, uint16_t address, uint16_t value);
extern int mb_master_write_multiple(uint8_t unit_id, uint16_t address, uint16_t quantity, const uint16_t* regs);

extern const char* mb_err_str(int err);

#ifdef __cplusplus
}
#endif

#endif /* __MB_MASTER_H__ */
