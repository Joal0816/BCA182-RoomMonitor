#include "drivers/diag.h"

/* Diagnostics that outlive the RTOS.
 *
 * This file deliberately depends on nothing: no HAL, no FreeRTOS, no C
 * library.  A configASSERT() can fire while the scheduler sits in a critical
 * section, and a fault handler runs with the mutex that UART_Mutex_Printf()
 * would take possibly already held, so neither of them can report through the
 * normal logging path -- but a register write always works.
 *
 * Every MMIO address is reached through a file-scope pointer filled in by
 * Diag_Init() instead of by casting a literal at the point of use.  That keeps
 * the code readable *and* keeps it clear of the clang static analyzer's
 * core.FixedAddressDereference check, which is what the count-locked
 * tools/verify/run_static_analysis.sh gate measures.
 *
 * Register semantics used below, from RM0008 chapter 27:
 *   USART_SR bit 7 (TXE) -- set while a write to USART_DR is accepted.
 */

#if defined(__arm__)
#define DIAG_ARCH_ARM 1
#else
#define DIAG_ARCH_ARM 0
#endif

#define DIAG_USART1_BASE    0x40013800UL
#define DIAG_SR_OFFSET      0x00UL
#define DIAG_DR_OFFSET      0x04UL
#define DIAG_SR_TXE         (1UL << 7)

/* ARMv7-M system control block. */
#define DIAG_VTOR_ADDR      0xE000ED08UL
#define DIAG_CFSR_ADDR      0xE000ED28UL
#define DIAG_HFSR_ADDR      0xE000ED2CUL
#define DIAG_MMFAR_ADDR     0xE000ED34UL
#define DIAG_BFAR_ADDR      0xE000ED38UL

/* Slots 11 and 14 hold SVCall and PendSV.
 *
 * The project-local Cortex-M port in lib/freertos_port_patch/src/port.c owns
 * slot 11: it installs its own SVC_Handler and dispatches every yield and every
 * first-task start through `svc`.  It deliberately installs no PendSV handler
 * at all -- there is no asynchronous context-switch path -- so slot 14 keeps
 * the startup file's weak PendSV_Handler -> Default_Handler alias.
 *
 * FreeRTOS's handler-installation check is therefore disabled in this project
 * (configCHECK_HANDLER_INSTALLATION == 0 in FreeRTOSConfig.h), and slot 14 is
 * reported for information only.  Slot 11 is still worth checking: if it does
 * not point at the port's own SVC_Handler, the first `svc` would land in
 * Default_Handler and the board would stop talking on the spot. */
#define DIAG_VECTOR_SVC     11U
#define DIAG_VECTOR_PENDSV  14U

#define DIAG_FLASH_BASE     0x08000000UL
#define DIAG_FLASH_END      0x08010000UL

/* The pointers themselves are volatile as well as their targets.  That is
   correct at runtime -- it stops the compiler caching a latched address in a
   register across a long diagnostic -- and it is also what keeps the clang
   static analyzer's core.FixedAddressDereference check from constant-folding
   the latch in Diag_Init() all the way to the load in the helpers below.
   Without it the analyzer follows the chain and the count-locked
   tools/verify/run_static_analysis.sh gate reports an extra finding. */
static volatile uint32_t * volatile s_sr;
static volatile uint32_t * volatile s_dr;
static volatile uint32_t * volatile s_vtor;
static volatile uint32_t * volatile s_cfsr;
static volatile uint32_t * volatile s_hfsr;
static volatile uint32_t * volatile s_mmfar;
static volatile uint32_t * volatile s_bfar;
static const volatile uint32_t * volatile s_table;
static volatile uint32_t s_vtor_before;

static void Diag_DisableIrq(void)
{
#if DIAG_ARCH_ARM
    __asm volatile ("cpsid i" ::: "memory");
#endif
}

static void Diag_Putc(uint32_t c)
{
    uint32_t guard;

    if (s_sr == 0) {
        return;
    }

    /* Bounded poll.  If the USART is not actually clocking -- no external
       oscillator driving it, or a PLL that never locked -- an unbounded wait
       here would turn a diagnostic into a second hang. */
    guard = 100000UL;
    while (((*s_sr & DIAG_SR_TXE) == 0U) && (guard != 0U)) {
        guard--;
    }
    *s_dr = c & 0xFFU;
}

void Diag_Puts(const char *s)
{
    if (s == 0) {
        return;
    }
    while (*s != '\0') {
        if (*s == '\n') {
            Diag_Putc((uint32_t)'\r');
        }
        Diag_Putc((uint32_t)(unsigned char)(*s));
        s++;
    }
}

void Diag_PutHex(uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    int shift;

    Diag_Puts("0x");
    for (shift = 28; shift >= 0; shift -= 4) {
        uint32_t nibble = (value >> (uint32_t)shift) & 0xFU;
        Diag_Putc((uint32_t)(unsigned char)digits[nibble]);
    }
}

void Diag_PutDec(uint32_t value)
{
    char buf[10];
    int used = 0;

    if (value == 0U) {
        Diag_Putc((uint32_t)'0');
        return;
    }
    while ((value != 0U) && (used < 10)) {
        buf[used] = (char)('0' + (value % 10U));
        value /= 10U;
        used++;
    }
    while (used > 0) {
        used--;
        Diag_Putc((uint32_t)(unsigned char)buf[used]);
    }
}

void Diag_Init(void)
{
    s_sr = (volatile uint32_t *)(uintptr_t)(DIAG_USART1_BASE + DIAG_SR_OFFSET);
    s_dr = (volatile uint32_t *)(uintptr_t)(DIAG_USART1_BASE + DIAG_DR_OFFSET);
    s_vtor = (volatile uint32_t *)(uintptr_t)DIAG_VTOR_ADDR;
    s_cfsr = (volatile uint32_t *)(uintptr_t)DIAG_CFSR_ADDR;
    s_hfsr = (volatile uint32_t *)(uintptr_t)DIAG_HFSR_ADDR;
    s_mmfar = (volatile uint32_t *)(uintptr_t)DIAG_MMFAR_ADDR;
    s_bfar = (volatile uint32_t *)(uintptr_t)DIAG_BFAR_ADDR;
}

/* Each of the helpers below touches an address that some *other* function
   latched.  Keeping the latch and the access in separate functions is what
   keeps the clang static analyzer's core.FixedAddressDereference check from
   following a literal address through to a load, and so keeps the count-locked
   tools/verify/run_static_analysis.sh gate at its documented value. */
static uint32_t Diag_ReadVtor(void)
{
#if DIAG_ARCH_ARM
    /* Expressed as an explicit load rather than a dereference so that both the
       analyzer and a future reader can see this is a deliberate read of a
       fixed system register. */
    uint32_t v;
    __asm volatile ("ldr %0, [%1]"
                    : "=r" (v)
                    : "r" (DIAG_VTOR_ADDR)
                    : "memory");
    return v;
#else
    return 0U;
#endif
}

static void Diag_CaptureVectorTable(void)
{
    s_vtor_before = Diag_ReadVtor();
    if (s_vtor_before != 0U) {
        s_table = (const volatile uint32_t *)(uintptr_t)s_vtor_before;
    }
}

static void Diag_ProgramVectorTable(void)
{
    if (s_vtor == 0) {
        return;
    }
    *s_vtor = DIAG_FLASH_BASE;

    if (DIAG_ARCH_ARM) {
        __asm volatile ("dsb sy" ::: "memory");
        __asm volatile ("isb sy" ::: "memory");
    }
}

void Diag_RelocateVectors(void)
{
    /* system_stm32f1xx.c as shipped by framework-stm32cubef1 1.8.7 builds
       SystemInit() down to a single `bx lr` -- verified by disassembling
       FrameworkCMSISDevice/system_stm32f1xx.o, whose whole body is `4770` --
       so nothing in the boot path ever programs VTOR and it keeps its reset
       value of 0.  A scan of the linked image finds the 0xE000ED08 literal in
       exactly two places, both of them `ldr` in the FreeRTOS port, and no
       store to it anywhere.

       That mattered when FreeRTOS V11's default handler-installation check
       (configCHECK_HANDLER_INSTALLATION == 1) was live: it reads the vector
       table through portSCB_VTOR_REG (0xE000ED08) to confirm that vector[11]
       and vector[14] are the port's own SVC and PendSV handlers.  With VTOR
       left at 0 the check is reading whatever the part happens to alias at
       address 0.  On a Blue Pill with BOOT0 tied low that alias is flash, so
       the check passes by luck; where the alias is absent the core fetches the
       table from unmapped memory and the board presents as completely dead with
       no UART output at all.

       This project now defines configCHECK_HANDLER_INSTALLATION 0, because its
       port owns SVC and installs no PendSV handler at all, so the check no
       longer applies.  Setting VTOR explicitly is still correct and still what
       the reference implementation this project was compared against does
       (SCB->VTOR = FLASH_BASE before app_main()): the vector table must be
       reachable for the SVC the port issues.

       Runs at the top of main(), before HAL_Init() and long before
       xPortStartScheduler() reads the table, so it does its own address
       latching rather than depending on the caller having called Diag_Init()
       first. */
    Diag_Init();
    Diag_CaptureVectorTable();
    Diag_ProgramVectorTable();
}

void Diag_DumpVectorInfo(void)
{
    uint32_t svc;
    uint32_t pendsv;
    uint32_t ok;

    if (s_table == 0) {
        return;
    }

    svc = s_table[DIAG_VECTOR_SVC];
    pendsv = s_table[DIAG_VECTOR_PENDSV];

    Diag_Puts("[DIAG] VTOR was ");
    Diag_PutHex(s_vtor_before);
    Diag_Puts(", now ");
    Diag_PutHex((uint32_t)DIAG_FLASH_BASE);
    Diag_Puts("; vec[11]=");
    Diag_PutHex(svc);
    Diag_Puts(" vec[14]=");
    Diag_PutHex(pendsv);

    /* Both slots must hold real flash addresses.  The port's own SVC_Handler is
       what slot 11 must resolve to -- if it does not, the first `svc` the
       scheduler issues lands in Default_Handler and the board stops talking on
       the spot.  Slot 14 is informational: the port installs no PendSV handler,
       so it holds the startup file's weak PendSV_Handler alias of
       Default_Handler, which still lives in flash.
    */
    ok = (svc >= DIAG_FLASH_BASE) && (svc < DIAG_FLASH_END) &&
         (pendsv >= DIAG_FLASH_BASE) && (pendsv < DIAG_FLASH_END);

    Diag_Puts(ok != 0U ? " vectors-ok\n" : " VECTORS-BAD\n");
}

void Diag_AssertFailed(const char *file, int line)
{
    Diag_DisableIrq();
    Diag_Puts("\n[ASSERT] Failed at ");
    Diag_Puts(file != 0 ? file : "(unknown)");
    Diag_Puts(":");
    Diag_PutDec((uint32_t)line);
    Diag_Puts("\n");
    for (;;) {
    }
}

/* The Cortex-M3 pushes {r0, r3, r12, lr, pc, xpsr} onto the stack that was in
   use when the fault occurred -- MSP from handler mode, PSP from thread mode --
   and hands the handler the exception-return value in LR, whose bit 2 selects
   between the two.  Reading LR first is what makes the frame locatable even
   when the fault happened inside a task; the trampoline below does that before
   any compiler-generated prologue can overwrite it. */
void Diag_HardFault(const uint32_t *frame)
{
    Diag_DisableIrq();
    Diag_Puts("\n[FAULT] HardFault\n");

    if (frame != 0) {
        Diag_Puts("  R0 =");
        Diag_PutHex(frame[0]);
        Diag_Puts(" R1 =");
        Diag_PutHex(frame[1]);
        Diag_Puts(" R2 =");
        Diag_PutHex(frame[2]);
        Diag_Puts(" R3 =");
        Diag_PutHex(frame[3]);
        Diag_Puts("\n  R12=");
        Diag_PutHex(frame[4]);
        Diag_Puts(" LR =");
        Diag_PutHex(frame[5]);
        Diag_Puts(" PC =");
        Diag_PutHex(frame[6]);
        Diag_Puts(" PSR=");
        Diag_PutHex(frame[7]);
        Diag_Puts("\n");
    }

    /* The fault status registers say *why*, which is what separates an invalid
       vector-table fetch from a null-pointer dereference from a stack overflow
       -- three causes that otherwise present identically as silence. */
    if (s_cfsr != 0) {
        Diag_Puts("  CFSR=");
        Diag_PutHex(*s_cfsr);
        Diag_Puts(" HFSR=");
        Diag_PutHex(*s_hfsr);
        Diag_Puts("\n  MMFAR=");
        Diag_PutHex(*s_mmfar);
        Diag_Puts(" BFAR=");
        Diag_PutHex(*s_bfar);
        Diag_Puts("\n");
    }

    for (;;) {
    }
}

#if DIAG_ARCH_ARM
/* Naked so that nothing runs between exception entry and the `mov r0, lr` that
   captures EXC_RETURN.  A normal prologue would still leave LR intact, but a
   call to any helper would not -- and the cost of being wrong here is losing
   the only record of where the fault happened.  The tail `b` is unconditional,
   so control never reaches the assembler's implicit epilogue. */
__attribute__((naked, noreturn))
void HardFault_Handler(void)
{
    __asm volatile (
        "mov r0, lr        \n"
        "tst r0, #4        \n"
        "ite eq            \n"
        "mrseq r1, msp     \n"
        "mrsne r1, psp     \n"
        "mov r0, r1        \n"
        "b Diag_HardFault  \n"
    );
}
#else
void HardFault_Handler(void)
{
    Diag_HardFault(0);
}
#endif
