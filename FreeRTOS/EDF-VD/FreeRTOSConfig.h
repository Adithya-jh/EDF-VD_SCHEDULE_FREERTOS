#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/*-----------------------------------------------------------
 * FreeRTOS Kernel Configuration
 *-----------------------------------------------------------*/

/* Basic definitions */
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configUSE_16_BIT_TICKS                  0

/* This is more relevant for real MCUs than for POSIX,
   but the kernel needs it. For POSIX, it's often unused. */
#define configCPU_CLOCK_HZ                      ( ( unsigned long ) 100000000UL )

/* Set the tick rate to 1 kHz (1ms per tick). */
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )

/* Minimal stack size in words (not bytes).
   For POSIX demos, set it to a higher value if you see warnings about stack. */
#define configMINIMAL_STACK_SIZE ( ( unsigned short ) 8192 )

/* Total heap size used by the kernel (for dynamic allocation).
   If you use heap_4.c or heap_5.c, adjust accordingly. */
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 20 * 1024 ) )

/* Maximum number of task priorities. 
   You can raise this if you need more priority levels. */
#define configMAX_PRIORITIES                    ( 7 )

/* Task name length. 16 is typical for debugging. */
#define configMAX_TASK_NAME_LEN                 ( 16 )

/* Use the idle hook? (0 = disabled, 1 = enabled) */
#define configUSE_IDLE_HOOK                     0

/* Use the tick hook? (0 = disabled, 1 = enabled) */
#define configUSE_TICK_HOOK                     0

/* Hook function for stack overflow checking. 
   You can enable this for debugging on embedded. */
#define configCHECK_FOR_STACK_OVERFLOW          0
#define configUSE_MALLOC_FAILED_HOOK            0

/*-----------------------------------------------------------
 * Run time and task stats gathering related definitions.
 * (Optional, but helpful for debugging and analysis.)
 *-----------------------------------------------------------*/
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY               0

/*-----------------------------------------------------------
 * Co-routine definitions (if you use them).
 *-----------------------------------------------------------*/
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         ( 2 )

/*-----------------------------------------------------------
 * Software timer definitions. (If you use timers.c)
 *-----------------------------------------------------------*/
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( 3 )
#define configTIMER_QUEUE_LENGTH                5
#define configTIMER_TASK_STACK_DEPTH            ( configMINIMAL_STACK_SIZE * 2 )

/*-----------------------------------------------------------
 * Optional functions inclusion
 * 
 * These macros allow or disallow certain APIs (like vTaskDelay, 
 * vTaskPrioritySet, etc.) to be compiled into the kernel.
 * Setting them to 1 includes the API, 0 excludes it.
 *-----------------------------------------------------------*/
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xResumeFromISR                  1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetIdleTaskHandle          0
#define INCLUDE_xTaskAbortDelay                 0
#define INCLUDE_xTaskGetSchedulerState          1

/*-----------------------------------------------------------
 * Assertion definition
 * 
 * configASSERT() is a standard FreeRTOS macro for runtime checking.
 * You can define it to something like an abort or custom handler.
 *-----------------------------------------------------------*/
#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

#endif /* FREERTOS_CONFIG_H */
