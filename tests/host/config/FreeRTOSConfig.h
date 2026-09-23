#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/*-----------------------------------------------------------
 * Scheduler
 *----------------------------------------------------------*/

#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0

#define configCPU_CLOCK_HZ                      1000000UL
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    8
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS

/*-----------------------------------------------------------
 * Memory
 *----------------------------------------------------------*/

#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0
#define configTOTAL_HEAP_SIZE                  (1024 * 1024)

/*-----------------------------------------------------------
 * Synchronization
 *----------------------------------------------------------*/

#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_TASK_NOTIFICATIONS            1

/*-----------------------------------------------------------
 * Tasks
 *----------------------------------------------------------*/

#define configMINIMAL_STACK_SIZE                512
#define configMAX_TASK_NAME_LEN                 32

/*-----------------------------------------------------------
 * Software timers
 *----------------------------------------------------------*/

#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               2
#define configTIMER_QUEUE_LENGTH                16
#define configTIMER_TASK_STACK_DEPTH            1024

/*-----------------------------------------------------------
 * API inclusion
 *----------------------------------------------------------*/

#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskDelayUntil                  1
#define INCLUDE_vTaskDelete                      1
#define INCLUDE_vTaskSuspend                     1
#define INCLUDE_xTaskGetCurrentTaskHandle        1
#define INCLUDE_xTaskGetSchedulerState           1
#define INCLUDE_vTaskPrioritySet                 1
#define INCLUDE_uxTaskPriorityGet                1
#define INCLUDE_uxTaskGetStackHighWaterMark     1

/*-----------------------------------------------------------
 * Assertions
 *----------------------------------------------------------*/

#define configASSERT(x) \
    do { \
        if (!(x)) { \
            __builtin_trap(); \
        } \
    } while (0)

/*-----------------------------------------------------------
 * POSIX port
 *----------------------------------------------------------*/

#define configUSE_POSIX_ERRNO                    0

#endif
