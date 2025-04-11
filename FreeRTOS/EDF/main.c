#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "custom_apis.h"
#include "FreeRTOSConfig.h"
#include "edf_scheduler.h"  /* For EDF scheduling (Task 3) */

/* Forward declarations of the 3 tasks */
static void vTemperatureTask(void *pvParameters);
static void vPressureTask(void *pvParameters);
static void vHeightTask(void *pvParameters);

/* We'll store the TaskHandles so we can register them with EDF. */
TaskHandle_t tempTaskHandle     = NULL;
TaskHandle_t pressureTaskHandle = NULL;
TaskHandle_t heightTaskHandle   = NULL;

/* For easy referencing in the xTasksEDF array: */
#define TEMP_TASK_INDEX     0
#define PRESSURE_TASK_INDEX 1
#define HEIGHT_TASK_INDEX   2

int main(void)
{
    printf("Starting FreeRTOS tasks with EDF scheduling...\n");

    /* Create Temperature Task */
    xTaskCreate(vTemperatureTask,
                "TempTask",
                configMINIMAL_STACK_SIZE,
                NULL,
                TEMP_TASK_PRIORITY,
                &tempTaskHandle);

    /* Create Pressure Task */
    xTaskCreate(vPressureTask,
                "PressureTask",
                configMINIMAL_STACK_SIZE,
                NULL,
                PRESSURE_TASK_PRIORITY,
                &pressureTaskHandle);

    /* Create Height Task */
    xTaskCreate(vHeightTask,
                "HeightTask",
                configMINIMAL_STACK_SIZE,
                NULL,
                HEIGHT_TASK_PRIORITY,
                &heightTaskHandle);

    /* Convert periods from ms to ticks. */
    TickType_t tempPeriodTicks     = pdMS_TO_TICKS(TEMP_TASK_PERIOD_MS);
    TickType_t pressurePeriodTicks = pdMS_TO_TICKS(PRESSURE_TASK_PERIOD_MS);
    TickType_t heightPeriodTicks   = pdMS_TO_TICKS(HEIGHT_TASK_PERIOD_MS);

    /* Register tasks for EDF scheduling. */
    vRegisterTaskEDF(tempTaskHandle,     tempPeriodTicks,     TEMP_TASK_INDEX);
    vRegisterTaskEDF(pressureTaskHandle, pressurePeriodTicks, PRESSURE_TASK_INDEX);
    vRegisterTaskEDF(heightTaskHandle,   heightPeriodTicks,   HEIGHT_TASK_INDEX);

    /* Start the EDF scheduler (creates a high-priority scheduling task). */
    vStartEDFScheduler();

    /* Finally, start the FreeRTOS scheduler. */
    vTaskStartScheduler();

    /* We should never reach here if the kernel is running. */
    for(;;);
    return 0;
}

/*-----------------------------------------------------------
 * Task Definitions
 *-----------------------------------------------------------*/

/**
 * @brief Periodically reads a random temperature value and prints to console.
 *        Also updates its EDF deadline at the end of each iteration.
 */
static void vTemperatureTask(void *pvParameters)
{
    (void) pvParameters;
    const TickType_t xDelay = pdMS_TO_TICKS(TEMP_TASK_PERIOD_MS);

    for(;;)
    {
        int32_t temp = getTemperature();
        printf("[TempTask]  Temp: %ld, TickTime: %lu\n",
               (long)temp, (unsigned long)xTaskGetTickCount());

        /* Mark that we've completed one job, so push our deadline. */
        vUpdateTaskDeadline(TEMP_TASK_INDEX);

        /* Sleep until next period. */
        vTaskDelay(xDelay);
    }
}

/**
 * @brief Periodically reads a random pressure value and prints to console.
 *        Also updates its EDF deadline at the end of each iteration.
 */
static void vPressureTask(void *pvParameters)
{
    (void) pvParameters;
    const TickType_t xDelay = pdMS_TO_TICKS(PRESSURE_TASK_PERIOD_MS);

    for(;;)
    {
        int32_t pressure = getPressure();
        printf("[PressureTask]  Pressure: %ld, TickTime: %lu\n",
               (long)pressure, (unsigned long)xTaskGetTickCount());

        vUpdateTaskDeadline(PRESSURE_TASK_INDEX);

        vTaskDelay(xDelay);
    }
}

/**
 * @brief Periodically reads a random height value and prints to console.
 *        Also updates its EDF deadline at the end of each iteration.
 */
static void vHeightTask(void *pvParameters)
{
    (void) pvParameters;
    const TickType_t xDelay = pdMS_TO_TICKS(HEIGHT_TASK_PERIOD_MS);

    for(;;)
    {
        int32_t height = getHeight();
        printf("[HeightTask]  Height: %ld, TickTime: %lu\n",
               (long)height, (unsigned long)xTaskGetTickCount());

        vUpdateTaskDeadline(HEIGHT_TASK_INDEX);

        vTaskDelay(xDelay);
    }
}
