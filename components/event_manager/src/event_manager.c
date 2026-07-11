#include <event_manager.h>
#include <common_types.h>
#include <event_registry_types.h>
#include <string.h>
#include <esp_log.h>

static const char *TAG = "Event Manager";

static event_subscription_t subscriptions[EVENT_COUNT];
static bool initialized = false;

bool event_manager_init(void)
{
    memset(subscriptions, 0, sizeof(subscriptions));
    initialized = true;
    return true;
}

void event_manager_deinit(void)
{
    memset(subscriptions, 0, sizeof(subscriptions));
    initialized = false;
}

bool event_subscribe(event_id_t event_id, event_handler_t handler)
{
    if (!initialized ||
        handler == NULL ||
        event_id >= EVENT_COUNT)
    {
        return false;
    }

    event_subscription_t *sub = &subscriptions[event_id];
    // Check for duplicate subscription
    for (uint8_t i = 0; i < sub->count; i++)
    {
        if (sub->handlers[i] == handler)
        {
            return true;
        }
    }

    if (sub->count >= MAX_EVENT_SUBSCRIBERS)
    {
        ESP_LOGE(TAG, "Too many subscribers for event %d", event_id);
        return false;
    }

    sub->handlers[sub->count++] = handler;
    return true;
}

bool event_unsubscribe(event_id_t event_id, event_handler_t handler)
{
    if (!initialized || handler == NULL || event_id >= EVENT_COUNT)
    {
        return false;
    }

    event_subscription_t *sub = &subscriptions[event_id];
    for (uint8_t i = 0; i < sub->count; i++)
    {
        if (sub->handlers[i] == handler)
        {
            for (uint8_t j = i; j < sub->count - 1; j++)
            {
                sub->handlers[j] = sub->handlers[j + 1];
            }

            sub->count--;
            sub->handlers[sub->count] = NULL;
            return true;
        }
    }
    return false;
}

bool event_publish(const event_t *event)
{
    if (!initialized || event == NULL)
    {
        return false;
    }

    if (event->ev >= EVENT_COUNT)
    {
        ESP_LOGE(TAG, "Invalid event id %d", event->ev);
        return false;
    }

    event_subscription_t *sub = &subscriptions[event->ev];
    for (uint8_t i = 0; i < sub->count; i++)
    {
        sub->handlers[i](event);
    }
    return true;
}
