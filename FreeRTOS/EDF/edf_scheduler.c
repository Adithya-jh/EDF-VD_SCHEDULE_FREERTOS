#include <stdio.h>
#include "edf_scheduler.h"

/* Data structure to hold each task's info. */
typedef struct
{
    TaskHandle_t xHandle;
    TickType_t   xPeriod;
    TickType_t   xNextDeadline;
} EDFTask_t;

/* Adjust if you have more tasks; for this assignment, we have 3. */
#define NUM_EDF_TASKS (3)

static EDFTask_t xTasksEDF[NUM_EDF_TASKS];

void vRegisterTaskEDF(TaskHandle_t handle, TickType_t period, int index)
{
    if(index < NUM_EDF_TASKS)
    {
        xTasksEDF[index].xHandle       = handle;
        xTasksEDF[index].xPeriod       = period;
        xTasksEDF[index].xNextDeadline = xTaskGetTickCount() + period;
    }
}

void vUpdateTaskDeadline(int index)
{
    if(index < NUM_EDF_TASKS)
    {
        /* Once a task finishes its job, push its next deadline by +period. */
        xTasksEDF[index].xNextDeadline += xTasksEDF[index].xPeriod;
    }
}

/* The actual EDF scheduler task. */
static void vScheduleEDF(void *pvParameters)
{
    (void)pvParameters;

    /* We'll reevaluate priorities periodically. */
    const TickType_t xSchedulerDelay = pdMS_TO_TICKS(1);

    for(;;)
    {
        TickType_t earliestDeadline = (TickType_t)-1; /* Largest possible. */
        int earliestIndex = -1;

        /* 1. Find which task has the earliest deadline. */
        for(int i = 0; i < NUM_EDF_TASKS; i++)
        {
            if(xTasksEDF[i].xNextDeadline < earliestDeadline)
            {
                earliestDeadline = xTasksEDF[i].xNextDeadline;
                earliestIndex = i;
            }
        }

        /* 2. Set that task's priority to the highest, others to lower. */
        if(earliestIndex != -1)
        {
            for(int i = 0; i < NUM_EDF_TASKS; i++)
            {
                if(i == earliestIndex)
                {
                    /* Highest priority => (configMAX_PRIORITIES - 1). */
                    vTaskPrioritySet(xTasksEDF[i].xHandle, configMAX_PRIORITIES - 1);
                }
                else
                {
                    /* Low priority => pick 1 or something minimal. */
                    vTaskPrioritySet(xTasksEDF[i].xHandle, 1);
                }
            }
        }

        /* 3. Sleep a bit, then do it again. */
        vTaskDelay(xSchedulerDelay);
    }
}

void vStartEDFScheduler(void)
{
    /* Create the EDF scheduler task itself with high priority. */
    xTaskCreate(vScheduleEDF, 
                "EDF_Sched", 
                configMINIMAL_STACK_SIZE, 
                NULL, 
                (configMAX_PRIORITIES - 1), 
                NULL);
}
