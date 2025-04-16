/**
 * File: main.c
 */
#include <stdio.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"

/* Forward declaration of the function that does the offline scheduling.
   This function might be in sim_offline_edfvd.c. */
extern void vRunOfflineEDFVD(void);

static void vSimulationTask(void *pvParameters)
{
    /* Prevent unused-parameter warning. */
    (void)pvParameters;

    /* We run the entire offline scheduling process once. */
    printf("Starting Offline EDF-VD Simulation inside FreeRTOS...\n");

    vRunOfflineEDFVD();

    printf("Offline simulation completed!\n");

    /* We can suspend this task or delete it once done. */
    vTaskDelete(NULL);
}


int main(void)
{
    /* 1. Create the Simulation Task. */
    xTaskCreate(vSimulationTask,
                "SimTask",
                configMINIMAL_STACK_SIZE * 4, /* Might need a bit more stack for file I/O. */
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);

    /* 2. Start the FreeRTOS scheduler. */
    vTaskStartScheduler();

    /* We should never reach here if the scheduler starts successfully. */
    for(;;);
    return 0;
}
