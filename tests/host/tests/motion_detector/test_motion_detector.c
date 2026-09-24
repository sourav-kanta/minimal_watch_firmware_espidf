#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <motion_detection_engine_stage1.h>
#include <stage1_types.h>

#ifndef INPUT_FILE_PATH
#error "INPUT_FILE_PATH must be defined by CMake"
#endif

#define SAMPLES_PER_CHUNK       21
#define BYTES_PER_SAMPLE        6

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} test_sample_t;

typedef struct {
    char name[64];
    size_t sample_count;
    long data_offset;
} scenario_t;

static FILE *test_file;

/* -------------------------------------------------------------------------- */
/* Test data helpers                                                          */
/* -------------------------------------------------------------------------- */

static void reset_engine(void)
{
    stage1_deinit();
    stage1_init();
}

static bool read_sample(test_sample_t *sample)
{
    int x;
    int y;
    int z;

    if (fscanf(test_file, "%d %d %d", &x, &y, &z) != 3) {
        return false;
    }

    assert(x >= INT16_MIN && x <= INT16_MAX);
    assert(y >= INT16_MIN && y <= INT16_MAX);
    assert(z >= INT16_MIN && z <= INT16_MAX);

    sample->x = (int16_t)x;
    sample->y = (int16_t)y;
    sample->z = (int16_t)z;

    return true;
}

/*
 * stage1_process_motion_data() expects:
 *
 *   byte 0: Y low
 *   byte 1: Y high
 *   byte 2: X low
 *   byte 3: X high
 *   byte 4: Z low
 *   byte 5: Z high
 *
 * The production code then applies:
 *
 *   raw_x = Y
 *   raw_y = -X
 *   raw_z = Z
 */
static void pack_sample(
    const test_sample_t *sample,
    uint8_t *data)
{
    int16_t production_x = sample->y;
    int16_t production_y = -sample->x;
    int16_t production_z = sample->z;

    data[0] = (uint8_t)(production_x & 0xFF);
    data[1] = (uint8_t)(((uint16_t)production_x >> 8) & 0xFF);

    data[2] = (uint8_t)(production_y & 0xFF);
    data[3] = (uint8_t)(((uint16_t)production_y >> 8) & 0xFF);

    data[4] = (uint8_t)(production_z & 0xFF);
    data[5] = (uint8_t)(((uint16_t)production_z >> 8) & 0xFF);
}

static imu_stage1_result_t run_scenario(
    const scenario_t *scenario,
    size_t chunk_samples)
{
    test_sample_t samples[chunk_samples];
    uint8_t data[chunk_samples * BYTES_PER_SAMPLE];

    assert(test_file);
    assert(scenario);

    fseek(test_file, scenario->data_offset, SEEK_SET);

    imu_stage1_result_t final_result = {0};

    size_t samples_remaining = scenario->sample_count;

    while (samples_remaining > 0) {
        size_t samples_to_process =
            samples_remaining < chunk_samples
                ? samples_remaining
                : chunk_samples;

        for (size_t i = 0; i < samples_to_process; i++) {
            assert(read_sample(&samples[i]));

            pack_sample(
                &samples[i],
                &data[i * BYTES_PER_SAMPLE]);
        }

        imu_stage1_result_t result = {0};

        stage1_process_motion_data(
            data,
            samples_to_process * BYTES_PER_SAMPLE,
            &result);

        /*
         * Steps are emitted per chunk, so accumulate them across
         * the entire scenario.
         */
        final_result.steps += result.steps;

        /*
         * no_motion represents the state produced by the most
         * recently processed window, so retain the latest value.
         */
        final_result.no_motion = result.no_motion;

        samples_remaining -= samples_to_process;
    }

    return final_result;
}

/* -------------------------------------------------------------------------- */
/* Scenario loading                                                           */
/* -------------------------------------------------------------------------- */

static bool find_scenario(
    const char *scenario_name,
    scenario_t *scenario)
{
    char line[256];

    rewind(test_file);

    while (fgets(line, sizeof(line), test_file)) {
        if (line[0] != '#') {
            continue;
        }

        char name[64];
        size_t sample_count;

        if (sscanf(
                line,
                "# %63s %zu",
                name,
                &sample_count) != 2) {
            continue;
        }

        if (strcmp(name, scenario_name) != 0) {
            continue;
        }

        scenario->sample_count = sample_count;

        strncpy(
            scenario->name,
            name,
            sizeof(scenario->name) - 1);

        scenario->name[sizeof(scenario->name) - 1] = '\0';

        /*
         * The scenario's data begins immediately after its header.
         */
        scenario->data_offset = ftell(test_file);

        return true;
    }

    return false;
}

/* -------------------------------------------------------------------------- */
/* Tests                                                                      */
/* -------------------------------------------------------------------------- */

static void test_simple_sine(void)
{
    printf("test_simple_sine...\n");

    reset_engine();

    scenario_t scenario = {0};

    assert(find_scenario("simple_sine", &scenario));

    imu_stage1_result_t result = run_scenario(
        &scenario,
        SAMPLES_PER_CHUNK);

    printf("  steps: %u, no_motion: %s\n",
           result.steps,
           result.no_motion ? "true" : "false");

    printf("  PASS\n");
}

static void test_sine_noise(void)
{
    printf("test_sine_noise...\n");

    reset_engine();

    scenario_t scenario = {0};

    assert(find_scenario("sine_noise", &scenario));

    imu_stage1_result_t result = run_scenario(
        &scenario,
        SAMPLES_PER_CHUNK);

    printf("  steps: %u, no_motion: %s\n",
           result.steps,
           result.no_motion ? "true" : "false");

    printf("  PASS\n");
}

static void test_varying_frequency(void)
{
    printf("test_varying_frequency...\n");

    reset_engine();

    scenario_t scenario = {0};

    assert(find_scenario("varying_frequency", &scenario));

    imu_stage1_result_t result = run_scenario(
        &scenario,
        SAMPLES_PER_CHUNK);

    printf("  steps: %u, no_motion: %s\n",
           result.steps,
           result.no_motion ? "true" : "false");

    printf("  PASS\n");
}

static void test_varying_frequency_noise(void)
{
    printf("test_varying_frequency_noise...\n");

    reset_engine();

    scenario_t scenario = {0};

    assert(find_scenario("varying_frequency_noise", &scenario));

    imu_stage1_result_t result = run_scenario(
        &scenario,
        SAMPLES_PER_CHUNK);

    printf("  steps: %u, no_motion: %s\n",
           result.steps,
           result.no_motion ? "true" : "false");

    printf("  PASS\n");
}

static void test_varying_frequency_amplitude(void)
{
    printf("test_varying_frequency_amplitude...\n");

    reset_engine();

    scenario_t scenario = {0};

    assert(find_scenario("varying_frequency_amplitude", &scenario));

    imu_stage1_result_t result = run_scenario(
        &scenario,
        SAMPLES_PER_CHUNK);

    printf("  steps: %u, no_motion: %s\n",
           result.steps,
           result.no_motion ? "true" : "false");

    printf("  PASS\n");
}

static void test_varying_frequency_amplitude_noise(void)
{
    printf("test_varying_frequency_amplitude_noise...\n");

    reset_engine();

    scenario_t scenario = {0};

    assert(find_scenario(
        "varying_frequency_amplitude_noise",
        &scenario));

    imu_stage1_result_t result = run_scenario(
        &scenario,
        SAMPLES_PER_CHUNK);

    printf("  steps: %u, no_motion: %s\n",
           result.steps,
           result.no_motion ? "true" : "false");

    printf("  PASS\n");
}

static void test_stillness_steps_stillness(void)
{
    printf("test_stillness_steps_stillness...\n");

    reset_engine();

    scenario_t scenario = {0};

    assert(find_scenario(
        "stillness_steps_stillness",
        &scenario));

    imu_stage1_result_t result = run_scenario(
        &scenario,
        SAMPLES_PER_CHUNK);

    printf("  steps: %u, no_motion: %s\n",
           result.steps,
           result.no_motion ? "true" : "false");

    printf("  PASS\n");
}

static void test_steps_gap_steps(void)
{
    printf("test_steps_gap_steps...\n");

    reset_engine();

    scenario_t scenario = {0};

    assert(find_scenario(
        "steps_gap_steps",
        &scenario));

    imu_stage1_result_t result = run_scenario(
        &scenario,
        SAMPLES_PER_CHUNK);

    printf("  steps: %u, no_motion: %s\n",
           result.steps,
           result.no_motion ? "true" : "false");

    printf("  PASS\n");
}

static void test_single_missed_peak(void)
{
    printf("test_single_missed_peak...\n");

    reset_engine();

    scenario_t scenario = {0};

    assert(find_scenario(
        "single_missed_peak",
        &scenario));

    imu_stage1_result_t result = run_scenario(
        &scenario,
        SAMPLES_PER_CHUNK);

    printf("  steps: %u, no_motion: %s\n",
           result.steps,
           result.no_motion ? "true" : "false");

    printf("  PASS\n");
}

static void test_discontinuous_steps(void)
{
    printf("test_discontinuous_steps...\n");

    reset_engine();

    scenario_t scenario = {0};

    assert(find_scenario(
        "discontinuous_steps",
        &scenario));

    imu_stage1_result_t result = run_scenario(
        &scenario,
        SAMPLES_PER_CHUNK);

    printf("  steps: %u, no_motion: %s\n",
           result.steps,
           result.no_motion ? "true" : "false");

    printf("  PASS\n");
}

/* -------------------------------------------------------------------------- */
/* Test task                                                                  */
/* -------------------------------------------------------------------------- */

static void host_test_task(void *arg)
{
    (void)arg;

    test_file = fopen(INPUT_FILE_PATH, "r");
    assert(test_file);

    printf("[TEST] stage1 motion detection tests\n");

    test_simple_sine();
    test_sine_noise();
    test_varying_frequency();
    test_varying_frequency_noise();
    test_varying_frequency_amplitude();
    test_varying_frequency_amplitude_noise();
    test_stillness_steps_stillness();
    test_steps_gap_steps();
    test_single_missed_peak();
    test_discontinuous_steps();

    stage1_deinit();

    fclose(test_file);
    test_file = NULL;

    printf("[TEST] all stage1 motion detection tests passed\n");

    vTaskEndScheduler();
    vTaskDelete(NULL);
}

/* -------------------------------------------------------------------------- */
/* Main                                                                       */
/* -------------------------------------------------------------------------- */

int main(void)
{
    xTaskCreate(
        host_test_task,
        "stage1_test",
        4096,
        NULL,
        3,
        NULL
    );

    vTaskStartScheduler();

    return 0;
}
