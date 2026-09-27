#ifndef DIAG_H
#define DIAG_H

#include <stdint.h>

/* Last-resort diagnostics.
 *
 * UART_Mutex_Printf() is the right way to log from a task, but it cannot be
 * used from a configASSERT() (the scheduler may already hold an interrupt
 * mask) or from a fault handler (it takes a mutex and calls a non-reentrant
 * HAL transmit).  Everything declared here writes straight to the USART
 * registers instead, so it works from any context, including before the
 * scheduler exists.
 */

/* Latch the register addresses.  Call once, after MX_USART1_UART_Init(). */
void Diag_Init(void);

/* Point VTOR at the vector table at 0x08000000.  Call from SystemInit(). */
void Diag_RelocateVectors(void);

/* Print VTOR and the SVCall/PendSV entries it resolves to.  The FreeRTOS port
   asserts on exactly these two; this makes a bad VTOR visible before the
   assert fires. */
void Diag_DumpVectorInfo(void);

/* Minimal serial output.  '\n' is expanded to CRLF. */
void Diag_Puts(const char *s);
void Diag_PutHex(uint32_t value);
void Diag_PutDec(uint32_t value);

/* Report a failed assertion, then halt.  The file/line come from the
   __FILE__/__LINE__ at the configASSERT() call site.  Never returns. */
void Diag_AssertFailed(const char *file, int line) __attribute__((noreturn));

/* Decode and print a stacked exception frame, then halt.  Never returns. */
void Diag_HardFault(const uint32_t *frame) __attribute__((noreturn));

/* Strong override of the startup file's weak silent infinite loop. */
void HardFault_Handler(void);

#endif /* DIAG_H */
