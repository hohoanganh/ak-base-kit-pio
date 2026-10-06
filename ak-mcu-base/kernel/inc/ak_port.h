/**
 ******************************************************************************
 * @brief:  AK kernel port interface.
 *
 * The kernel never touches chip registers. Each port (port/<name>/) provides:
 *   - nestable critical section
 *   - millisecond counter (task tracing)
 *   - fatal error handler (FATAL)
 *   - console output (xprintf)
 ******************************************************************************
**/

#ifndef __AK_PORT_H__
#define __AK_PORT_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#include "xprintf.h"

#define __AK_PACKETED	__attribute__((__packed__))
#define __AK_WEAK		__attribute__((__weak__))

/* Nestable critical section. The outermost EXIT restores the interrupt state
 * saved by the outermost ENTRY (safe to call from ISRs). */
extern void ak_port_enter_critical(void);
extern void ak_port_exit_critical(void);

/* Milliseconds since boot (wraps after ~49 days). */
extern uint32_t ak_port_millis(void);

/* Unrecoverable error. MCU: log + reset. Host: longjmp to test harness.
 * Never returns. */
extern void ak_port_fatal(const char* s, uint8_t c) __attribute__((noreturn));

/* Called by the scheduler loop when no message is pending (WFI / sleep).
 * Runs inside a critical section: the port must sleep in a way that a pending
 * interrupt still wakes it (Cortex-M WFI does, also with PRIMASK set). */
extern void ak_port_idle(void);

/* The scheduler is about to run a handler (or went idle: AK_TASK_IDLE_ID).
 * The port remembers it across a reset: a watchdog reset can then be traced
 * to the handler that was stuck. Called inside a critical section. */
extern void ak_port_note_dispatch(uint8_t task_id, uint8_t sig);

#define ENTRY_CRITICAL()		ak_port_enter_critical()
#define EXIT_CRITICAL()			ak_port_exit_critical()
#define FATAL(s, c)				ak_port_fatal((s), (uint8_t)(c))
#define AK_PRINTF				xprintf

/* Index of highest set bit (1..32), 0 if val == 0. __builtin_clz(0) is UB on
 * x86 hosts; ARM CLZ returns 32, which is why the original never hit it. */
#define LOG2LKUP(val)			((uint8_t)((val) ? (32U - (uint8_t)__builtin_clz((uint32_t)(val))) : 0U))

#ifdef __cplusplus
}
#endif

#endif /* __AK_PORT_H__ */
