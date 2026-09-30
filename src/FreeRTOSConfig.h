#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* SystemCoreClock is maintained by the CMSIS device code (system_stm32f1xx.c)
   and updated by HAL_RCC_ClockConfig(), so the tick period follows whatever
   clock SystemClock_Config() actually achieved -- including the 8 MHz HSI
   fallback -- without this file having to be kept in sync by hand. */
extern uint32_t SystemCoreClock;

/*-----------------------------------------------------------
 * Application specific definitions.
 *
 * These definitions should be adjusted for your particular hardware and
 * application requirements.
 *
 * THESE PARAMETERS ARE DESCRIBED WITHIN THE 'CONFIGURATION' SECTION OF THE
 * FreeRTOS API DOCUMENTATION AVAILABLE ON THE FreeRTOS.org WEB SITE.
 *
 * See http://www.freertos.org/a00110.html.
 *----------------------------------------------------------*/

/* Cortex-M specific definitions. */
#ifdef __NVIC_PRIO_BITS
    #define configPRIO_BITS __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS 4
#endif

/* The lowest interrupt priority that can be used in a call to a "set priority"
   function. */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15

/* The highest interrupt priority that can be used by any interrupt service
   routine that makes calls to interrupt safe FreeRTOS API functions. */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

/* Maps the port interrupt priority values to the values used in the
   FreeRTOS API functions. */
#define configKERNEL_INTERRUPT_PRIORITY (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* Core configuration */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      ( SystemCoreClock )
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                128
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES  1

/* Memory allocation */
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   ((size_t)(12 * 1024))
#define configAPPLICATION_ALLOCATED_HEAP        0

/* Hook functions */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     1
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

/* Synchronization */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           0
#define configQUEUE_REGISTRY_SIZE               0
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  1
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 5
#define configSTACK_DEPTH_TYPE                  uint16_t
#define configMESSAGE_BUFFER_LENGTH_TYPE        size_t

/* Run time and task stats gathering. */
#define configUSE_TRACE_FACILITY                0
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/* Co-routine definitions. */
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         2

/* Software timer definitions. */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               3
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            256

/* Set the following definitions to 1 to include the API function, or zero
   to exclude the API function. */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskCleanUpResources           0
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTimerPendFunctionCall          0
#define INCLUDE_xQueueGetMutexHolder            0
/* AlarmTask reports its stack margin at runtime, so the read-only
   introspection API has to be linked in.  It affects no scheduling or
   memory behaviour. */
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_eTaskGetState                   0

/* The patched port (lib/freertos_port_patch/src/port.c) defines the CMSIS-named
   handlers SVC_Handler and SysTick_Handler directly, and never installs a PendSV
   handler, so the library's vPortSVCHandler/xPortPendSVHandler aliases are not
   needed here. */
#define configCHECK_HANDLER_INSTALLATION        0

/* Assert.
 *
 * The upstream default is `taskDISABLE_INTERRUPTS(); for(;;);` -- a silent
 * spin.  On a Cortex-M3 that makes a failed assertion indistinguishable from a
 * hang or a dead board, which is precisely the symptom this project chased for
 * a while.  Route it through Diag_AssertFailed() instead so the failing file
 * and line appear on the serial port before the halt.
 *
 * Diag_AssertFailed() writes the USART registers directly and takes no mutex,
 * which is what makes it usable from an assertion: the assertion may well have
 * fired while a task held the logging mutex, or with interrupts already
 * masked.  It is declared here rather than pulled in via #include because this
 * header is included very early -- before <stdint.h> in some translation
 * units. */
void Diag_AssertFailed(const char *file, int line) __attribute__((noreturn));
#define configASSERT(x) if((x) == 0) { Diag_AssertFailed(__FILE__, __LINE__); }

#endif /* FREERTOS_CONFIG_H */
