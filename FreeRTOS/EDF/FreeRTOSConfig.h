#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/**
 * Minimal FreeRTOS configuration for your demo.
 * These settings are for a POSIX simulation (or simple embedded demo).
 * Adjust values as needed.
 */

/* Kernel Behavior */
#define configUSE_PREEMPTION                1
#define configUSE_IDLE_HOOK                 0
#define configUSE_TICK_HOOK                 0

/* CPU & Tick */
#define configCPU_CLOCK_HZ                  ( 100000000UL )  /* For simulation; change for your target */
#define configTICK_RATE_HZ                  ( 1000U )        /* 1ms tick */

/* Memory allocation */
#define configMINIMAL_STACK_SIZE            ( 128U )
#define configTOTAL_HEAP_SIZE               ( ( size_t ) ( 20 * 1024 ) )
#define configMAX_PRIORITIES                ( 7 )
#define configUSE_16_BIT_TICKS              0
#define configMAX_TASK_NAME_LEN             ( 16 )

/* API Function inclusion for FreeRTOS tasks:
   These macros ensure that tasks such as vTaskDelay and vTaskPrioritySet are compiled.
*/
#define INCLUDE_vTaskDelay                1
#define INCLUDE_vTaskPrioritySet          1
/* (Other API inclusion macros can be added here as needed.) */

/* Application-specific definitions */
#define TEMP_TASK_PERIOD_MS                 500U
#define PRESSURE_TASK_PERIOD_MS             1000U
#define HEIGHT_TASK_PERIOD_MS               2000U

#define TEMP_TASK_PRIORITY                  1
#define PRESSURE_TASK_PRIORITY              1
#define HEIGHT_TASK_PRIORITY                1

#endif /* FREERTOS_CONFIG_H */
