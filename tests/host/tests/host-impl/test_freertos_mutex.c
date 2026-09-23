#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

static SemaphoreHandle_t mutex;

/*
 * Test: FreeRTOS mutex operation.
 *
 * Verifies that the host FreeRTOS port can create a mutex, acquire it
 * from a task, release it, and successfully continue task execution.
 */
static void test_task(void *arg) {
    (void)arg;
    assert(xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE);
    printf("Task acquired mutex\n");
    xSemaphoreGive(mutex);
    printf("Task released mutex\n");
    exit(0);
}

int main(void) {
    printf("Starting FreeRTOS mutex test\n");
    mutex = xSemaphoreCreateMutex();
    assert(mutex != NULL);
    assert(xTaskCreate(
        test_task,
        "mutex_test",
        1024,
        NULL,
        1,
        NULL
    ) == pdPASS);

    vTaskStartScheduler();
    return 0;
}
