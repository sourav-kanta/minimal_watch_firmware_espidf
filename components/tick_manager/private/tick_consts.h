#ifndef TICK_CONSTS_H
#define TICK_CONSTS_H

#include <tick_types.h>
#include <dependent_types.h>
#include <assert.h>

static const char* timer_names[] = {
    [TICK_WATCHFACE] = "Watchface Timer",
    [TICK_WORK] = "Work Timer",
};

static const unsigned int tick_intervals_ms[TICK_INVALID] = {
    [TICK_WATCHFACE] = 30,
    [TICK_WORK] = 1025,
};

static const dependent_tick_registry_t dep_tick_registry[DEPENDENT_TICK_INVALID] = {
    [DEPENDENT_TICK_SENSOR] = {
        .interval_work_ticks = 60,
        .event_mapping = EVENT_SENSOR_TICK,
    },
};

static_assert(sizeof(tick_intervals_ms) / sizeof(tick_intervals_ms[0]) == TICK_INVALID,
              "Tick intervals array size does not match TICK_INVALID count");
static_assert(sizeof(timer_names) / sizeof(timer_names[0]) == TICK_INVALID,
              "Timer name array size does not match TICK_INVALID count");
static_assert(sizeof(dep_tick_registry) / sizeof(dep_tick_registry[0]) == DEPENDENT_TICK_INVALID,
              "Dependent registry array size does not match DEPENDENT_TICK_INVALID count");
static_assert(sizeof(void*) >= sizeof(tick_type_t),
              "Pointer size is too small to safely pack tick_type_t");

#endif /* TICK_CONSTS_H */
