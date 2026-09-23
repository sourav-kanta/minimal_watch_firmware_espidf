#include <stdio.h>
#include <stdlib.h>

#include <FreeRTOS.h>
#include <task.h>

/*
 * Test: FreeRTOS basic task execution.
 *
 * Verifies that the host FreeRTOS port can create a task, start the
 * scheduler, and execute the created task successfully.
 */
static void test_task(void *arg) {
    (void)arg;
    printf("FreeRTOS task is running!\n");
    exit(0);
}

int main(void) {
    printf("Starting FreeRTOS host test\n");

    BaseType_t result = xTaskCreate(
        test_task,
        "test",
        1024,
        NULL,
        1,
        NULL
    );

    if (result != pdPASS) {
        printf("xTaskCreate failed\n");
        return 1;
    }

    vTaskStartScheduler();
    return 0;
}
