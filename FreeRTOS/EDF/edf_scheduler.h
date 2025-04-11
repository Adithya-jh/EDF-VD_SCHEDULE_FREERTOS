#ifndef EDF_SCHEDULER_H
#define EDF_SCHEDULER_H

#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief Register a task with the EDF scheduler. 
 *        (Stores the period and calculates initial deadline.)
 * @param handle  Task handle returned by xTaskCreate().
 * @param period  Period in ticks.
 * @param index   Index to store in an internal array.
 */
void vRegisterTaskEDF(TaskHandle_t handle, TickType_t period, int index);

/**
 * @brief Update the task's next deadline by +period once it finishes a job.
 * @param index   The index of this task in the EDF array
 */
void vUpdateTaskDeadline(int index);

/**
 * @brief Creates the EDF scheduler task which runs at high priority.
 */
void vStartEDFScheduler(void);

#endif /* EDF_SCHEDULER_H */
