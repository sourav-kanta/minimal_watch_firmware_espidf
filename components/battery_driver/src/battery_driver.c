#include <battery_driver.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <gpio_manager.h>
#include <esp_assert.h>
#include <esp_log.h>
#include <kalman_filter.h>

static kalman_state_t batt_kalman_state;
static const char* TAG = "Battery driver";
static uint8_t reported_pct = 100;
static bool first_run = true;

const int lipo_curve_mv[] = {3270, 3610, 3690, 3710, 3730, 3770, 3790, 3820, 3870, 3950, 4200};
const int lipo_curve_pct[] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
const int curve_points = sizeof(lipo_curve_mv) / sizeof(lipo_curve_mv[0]);

#define BURST_READ_SAMPLE_COUNT     10
#define OLYMPIC_DROP_COUNT          3
#define KALMAN_NOISE_VARIANCE       50.0f
#define KALMAN_PROCESS_VARIANCE     0.1f
#define RECOVERY_THRESHOLD          3 

ESP_STATIC_ASSERT((BURST_READ_SAMPLE_COUNT - (2*OLYMPIC_DROP_COUNT)) > 0, 
                  "Invalid battery read burst configs");

static int compare_ints(const void *a, const void *b) {
    int int_a = *(const int *)a;
    int int_b = *(const int *)b;
    return (int_a > int_b) - (int_a < int_b);
}

uint8_t battery_driver_read_battery_percentage(void) {
    gpio_manager_enable_battery_read();
    TickType_t sleep_tick = pdMS_TO_TICKS(5);
    sleep_tick = sleep_tick == 0 ? 1 : sleep_tick;
    vTaskDelay(sleep_tick);

    int samples[BURST_READ_SAMPLE_COUNT];
    for(int i = 0; i < BURST_READ_SAMPLE_COUNT; i++) {
        samples[i] = gpio_manager_read_battery_mv();
        sleep_tick = pdMS_TO_TICKS(1);
        sleep_tick = sleep_tick == 0 ? 1 : sleep_tick;
        vTaskDelay(sleep_tick);
    }
    
    gpio_manager_disable_battery_read();
    
    qsort(samples, BURST_READ_SAMPLE_COUNT, sizeof(int), compare_ints);
    
    // Basic filtering with olympic drop
    int sum = 0;
    int valid_sample_count = BURST_READ_SAMPLE_COUNT - (OLYMPIC_DROP_COUNT * 2);
    for(int i = OLYMPIC_DROP_COUNT; i < BURST_READ_SAMPLE_COUNT - OLYMPIC_DROP_COUNT; i++) {
        sum += samples[i];
    }
    int average_mv = sum / valid_sample_count;
    ESP_LOGD(TAG, "Filtered ADC reading: %d", average_mv);

    // 1-D Kalman filter to smooth out the average
    int final_smoothed_mv = kalman_filter_update(&batt_kalman_state, average_mv);
    ESP_LOGD(TAG, "Kalman Smoothed Voltage (Sample %lu): %d mV", 
             batt_kalman_state.sample_count, final_smoothed_mv);

    uint8_t calculated_pct = 0;
    
    if (final_smoothed_mv <= lipo_curve_mv[0]) {
        calculated_pct = 0;
    } else if (final_smoothed_mv >= lipo_curve_mv[curve_points - 1]) {
        calculated_pct = 100;
    } else {
        for (int i = 0; i < curve_points - 1; i++) {
            if (final_smoothed_mv >= lipo_curve_mv[i] && final_smoothed_mv <= lipo_curve_mv[i + 1]) {
                int mv_diff = lipo_curve_mv[i + 1] - lipo_curve_mv[i];
                int pct_diff = lipo_curve_pct[i + 1] - lipo_curve_pct[i];
                int mv_offset = final_smoothed_mv - lipo_curve_mv[i];
                
                calculated_pct = (uint8_t)(lipo_curve_pct[i] + ((mv_offset * pct_diff) / mv_diff));
                break; 
            }
        }
    }

    // Hysteresis Filter 
    bool is_charging = gpio_manager_is_charging();

    if (first_run) {
        reported_pct = calculated_pct;
        first_run = false;
    } else if (is_charging) {
        reported_pct = calculated_pct;
    } else {
        if (calculated_pct < reported_pct) {
            reported_pct = calculated_pct;
        } else if (calculated_pct >= (reported_pct + RECOVERY_THRESHOLD)) {
            reported_pct = calculated_pct;
        }
    }

    return reported_pct;
}

void battery_driver_init(void) {
    kalman_filter_init(&batt_kalman_state, KALMAN_NOISE_VARIANCE, KALMAN_PROCESS_VARIANCE);
    first_run = true;
    reported_pct = 100;
}

void battery_driver_deinit(void) {
    batt_kalman_state.sample_count = 0;
} 
