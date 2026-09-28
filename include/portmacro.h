/*
 * FreeRTOS Kernel V11.3.1
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/*
 * ---------------------------------------------------------------------------
 * PROJECT PORT (Wokwi): replaces the kernel's ARM_CM3 portmacro.h.
 * ---------------------------------------------------------------------------
 *
 * This file is picked up instead of
 * FreeRTOS-Kernel/portable/GCC/ARM_CM3/portmacro.h because the kernel's
 * portable.h includes "portmacro.h" and this project compiles with -Iinclude,
 * which precedes the port directory on the include path.
 *
 * Only the scheduling and critical-section macros differ from the stock
 * header; every type definition is byte-for-byte equivalent. The changes are:
 *
 *  - portYIELD() no longer pends PendSV with a store to ICSR (Wokwi returns to
 *    that store and loops). It calls vPortYield(), which switches context
 *    synchronously in the SVC handler, or defers the switch to the end of the
 *    current critical section.
 *
 *  - Critical sections no longer raise BASEPRI (not implemented in Wokwi).
 *    They gate the SysTick interrupt at its source (SysTick->CTRL.TICKINT) and
 *    recover the ticks that fell inside the window when they end.
 *
 * The two macros blog below (portNVIC_INT_CTRL_REG / portNVIC_PENDSVSET_BIT)
 * are retained only so the stock port.c can still be compiled; nothing that
 * ends up linked into the firmware pends PendSV.
 *
 * Why the header alone is not enough: the kernel sources (tasks.c, queue.c,
 * event_groups.c) compile against this file, but so must the port itself.
 * lib/freertos_port_patch/src/port.c is the other half and is linked in place
 * of the stock ARM_CM3 port.c.
 */

#ifndef PORTMACRO_H
#define PORTMACRO_H

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------
 * Port specific definitions.
 *
 * The settings in this file configure FreeRTOS correctly for the
 * given hardware and compiler.
 *
 * These settings should not be altered.
 *-----------------------------------------------------------
 */

/* Type definitions. */
#define portCHAR          char
#define portFLOAT         float
#define portDOUBLE        double
#define portLONG          long
#define portSHORT         short
#define portSTACK_TYPE    uint32_t
#define portBASE_TYPE     long

typedef portSTACK_TYPE   StackType_t;
typedef long             BaseType_t;
typedef unsigned long    UBaseType_t;

#if ( configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_16_BITS )
    typedef uint16_t     TickType_t;
    #define portMAX_DELAY              ( TickType_t ) 0xffff
#elif ( configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_32_BITS )
    typedef uint32_t     TickType_t;
    #define portMAX_DELAY              ( TickType_t ) 0xffffffffUL
#else
    #error configTICK_TYPE_WIDTH_IN_BITS set to a value not supported by this port.
#endif

/*-----------------------------------------------------------*/

/* Architecture specifics. */
#define portSTACK_GROWTH                ( -1 )
#define portTICK_PERIOD_MS              ( ( TickType_t ) 1000 / configTICK_RATE_HZ )
#define portBYTE_ALIGNMENT              8
#define portDONT_DISCARD                __attribute__( ( used ) )
/*-----------------------------------------------------------*/

/* Scheduler utilities.
 *
 * Wokwi's Cortex-M3 model returns to the instruction that stored the PendSV
 * set bit, so the stock PendSV-based yield spins forever. vPortYield() instead
 * switches context synchronously inside the SVC handler. If it is called
 * inside a critical section the switch is recorded and performed when that
 * section ends.
 */
extern void vPortYield( void );
extern void vPortYieldFromISR( void );
#define portYIELD()                                 vPortYield()
#define portEND_SWITCHING_ISR( xSwitchRequired )    do { if( ( xSwitchRequired ) != pdFALSE ) vPortYieldFromISR(); } while( 0 )
#define portYIELD_FROM_ISR( x )                     portEND_SWITCHING_ISR( x )

/* Only so the kernel's own (unlinked in this project) ARM_CM3 port.c still
 * compiles inside its archive. Nothing linked into the firmware pends PendSV. */
#define portNVIC_INT_CTRL_REG           ( * ( ( volatile uint32_t * ) 0xe000ed04 ) )
#define portNVIC_PENDSVSET_BIT          ( 1UL << 28UL )
/*-----------------------------------------------------------*/

/* Critical section management.
 *
 * BASEPRI is not implemented by Wokwi and no CPU mask keeps SysTick out, so
 * the tick is gated at its source and the wraps that fall inside the window
 * are replayed when the outermost section ends.
 */
extern void vPortEnterCritical( void );
extern void vPortExitCritical( void );
extern void vPortGateTick( void );
extern void vPortUngateTick( void );

/* SysTick is the only interrupt that calls into the kernel, so there is
 * nothing to mask "from ISR". */
#define portSET_INTERRUPT_MASK_FROM_ISR()           ( 0UL )
#define portCLEAR_INTERRUPT_MASK_FROM_ISR( x )      ( ( void ) ( x ) )

#define portDISABLE_INTERRUPTS()                    vPortGateTick()
#define portENABLE_INTERRUPTS()                     vPortUngateTick()

#define portENTER_CRITICAL()                        vPortEnterCritical()
#define portEXIT_CRITICAL()                         vPortExitCritical()
/*-----------------------------------------------------------*/

/* Task function macros as described on the FreeRTOS.org WEB site. */
#define portTASK_FUNCTION_PROTO( vFunction, pvParameters )    void vFunction( void * pvParameters )
#define portTASK_FUNCTION( vFunction, pvParameters )          void vFunction( void * pvParameters )
/*-----------------------------------------------------------*/

/* This port cannot suppress ticks: gating SysTick would stop the tick that
 * would wake the CPU again, and Wokwi models no sleep mode. */
#if ( configUSE_TICKLESS_IDLE == 1 )
    #error The Wokwi project port does not support tickless idle.
#endif

/* The port has its own ready-list bookkeeping, so the priority-bitmap shortcut
 * would need a portYIELD() that runs from inside the scheduler. */
#if ( configUSE_PORT_OPTIMISED_TASK_SELECTION == 1 )
    #error The Wokwi project port expects configUSE_PORT_OPTIMISED_TASK_SELECTION 0.
#endif
/*-----------------------------------------------------------*/

#define portNOP()    __asm volatile ( "nop" )
#define portINLINE              __inline

#ifndef portFORCE_INLINE
    #define portFORCE_INLINE    inline __attribute__( ( always_inline ) )
#endif
/*-----------------------------------------------------------*/

portFORCE_INLINE static BaseType_t xPortIsInsideInterrupt( void )
{
    uint32_t ulCurrentInterrupt;
    BaseType_t xReturn;

    /* Obtain the number of the currently executing interrupt. */
    __asm volatile ( "mrs %0, ipsr" : "=r" ( ulCurrentInterrupt )::"memory" );

    if( ulCurrentInterrupt == 0 )
    {
        xReturn = pdFALSE;
    }
    else
    {
        xReturn = pdTRUE;
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

#define portMEMORY_BARRIER()    __asm volatile ( "" ::: "memory" )

#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */
