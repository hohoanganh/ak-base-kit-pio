/**
 ******************************************************************************
 * @brief:  Crash log kept in NVM: the last abnormal resets survive power loss
 *          and can be read from the shell (or any other channel) in the field.
 *
 *  What is recorded (only abnormal events, so the NVM is not worn by boots):
 *    HARDFAULT     pc, lr, CFSR + the task/signal that was being handled
 *    FATAL         tag + code of the FATAL() + task/signal
 *    WATCHDOG      the task/signal that was running when the watchdog bit
 *    TASK_STALLED  a task that did not answer the liveness ping (code = id)
 *
 *  Nothing is written inside a fault handler: the port keeps the facts in RAM
 *  that survives the reset and crash_log_capture() stores them on the next
 *  start. A ring of CRASH_LOG_SLOTS records, newest found by sequence number;
 *  a record cut by a power loss fails its CRC and is skipped.
 *
 *  NVM map: [0, 64) boot_ctrl (two slots), [64, 64 + 8 * 20) crash log.
 ******************************************************************************
**/

#ifndef __CRASH_LOG_H__
#define __CRASH_LOG_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#define CRASH_LOG_NVM_OFFSET	(64)
#define CRASH_LOG_SLOTS			(8)

#define CRASH_KIND_NONE			(0)
#define CRASH_KIND_HARDFAULT	(1)
#define CRASH_KIND_FATAL		(2)
#define CRASH_KIND_WATCHDOG		(3)
#define CRASH_KIND_TASK_STALLED	(4)

typedef struct {
	uint8_t  seq;		/* set by crash_log_add() */
	uint8_t  kind;		/* CRASH_KIND_* */
	uint8_t  code;		/* FATAL code / stalled task id */
	uint8_t  task;		/* task being handled (0xEF = idle, 0xEE = interrupt) */
	uint32_t pc;
	uint32_t lr;
	uint32_t info;		/* HARDFAULT: CFSR. FATAL: first 4 chars of the tag */
	uint8_t  sig;		/* signal being handled */
	uint8_t  reserved;
	uint16_t crc16;
} crash_rec_t;

/* Append a record (seq and crc are filled in). Returns 0 on success. */
extern int crash_log_add(crash_rec_t* rec);

/* Read the n-th newest record (0 = newest). Returns 1 if it exists. */
extern uint8_t crash_log_read(uint8_t n, crash_rec_t* out);

/* Number of valid records. */
extern uint8_t crash_log_count(void);

extern void crash_log_clear(void);

/* Call once at start: stores what the previous run left behind (HardFault,
 * FATAL, watchdog reset). Returns the kind stored, CRASH_KIND_NONE if the
 * previous run ended normally. */
extern uint8_t crash_log_capture(void);

extern const char* crash_kind_str(uint8_t kind);

#ifdef __cplusplus
}
#endif

#endif /* __CRASH_LOG_H__ */
