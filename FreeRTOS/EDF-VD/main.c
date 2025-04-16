/* Define _POSIX_C_SOURCE to enable POSIX functions like getcwd */
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // for getcwd()
#include <limits.h>     // for PATH_MAX
#include "FreeRTOS.h"
#include "task.h"
#include "sim_offline_edfvd.h"  // This header must declare vRunOfflineEDFVD()

static void vSimulationTask(void *pvParameters)
{
    /* Prevent unused-parameter warning */
    (void)pvParameters;
    
    printf("DEBUG: vSimulationTask starting...\n");

    /* Print current working directory (for file I/O verification) */
    {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
            printf("DEBUG: Current working directory: %s\n", cwd);
        else
            perror("DEBUG: getcwd() error");
    }

    /* Optional: Yield to ensure the task gets scheduled */
    vTaskDelay(1);

    printf("DEBUG: Calling vRunOfflineEDFVD()...\n");
    vRunOfflineEDFVD();
    printf("DEBUG: vRunOfflineEDFVD() completed.\n");
    
    /* Delete this task once finished */
    vTaskDelete(NULL);
}

int main(void)
{
    printf("DEBUG: Entering main()...\n");
    
    /* Create the simulation task.
       Using configMINIMAL_STACK_SIZE * 2 to reduce the allocated stack size.
       (Ensure configTOTAL_HEAP_SIZE in FreeRTOSConfig.h is large enough.)
    */
    BaseType_t xReturned = xTaskCreate(
        vSimulationTask,                   // Task function
        "SimTask",                         // Task name
        configMINIMAL_STACK_SIZE * 2,        // Stack depth multiplier
        NULL,                              // Task parameter (unused)
        tskIDLE_PRIORITY + 1,              // Task priority
        NULL                               // Task handle (not used)
    );
    
    if (xReturned != pdPASS)
    {
        printf("ERROR: xTaskCreate failed with code %ld\n", (long)xReturned);
        return EXIT_FAILURE;
    }
    
    printf("DEBUG: Simulation task created successfully.\n");
    printf("DEBUG: Starting the FreeRTOS scheduler...\n");
    
    vTaskStartScheduler();
    
    /* If the scheduler returns, it indicates a problem. */
    printf("ERROR: vTaskStartScheduler returned unexpectedly!\n");
    for(;;);
    return EXIT_SUCCESS;
}
