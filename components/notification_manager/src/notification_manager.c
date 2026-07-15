#include <notification_manager.h>
#include <notification_consts.h>
#include <event_manager.h>
#include <esp_log.h>
#include <string.h>

static bool initialized = false;
notification_t notifications[MAX_NOTIFICATIONS];
unsigned int total_notifications = 0;
static const char* TAG = "Notification Manager";

static void assign_handlers(notification_t* notification) {
    switch(notification->app) {
        case NAVIGATION :
            notification->dismiss_handler = notification_manager_dismiss_notification;
            break;
        case CALL :
            notification->dismiss_handler = notification_manager_dismiss_notification;
            break;
        default :
            notification->dismiss_handler = notification_manager_dismiss_notification;
    }
}

void receive_notification(const event_t* event) {
    if((event->payload_len != sizeof(notification_t)) || event->data == NULL) {
        ESP_LOGE(TAG, "Invalid notification received, skipping");
        return;
    }
    if(total_notifications == MAX_NOTIFICATIONS) {
        ESP_LOGD(TAG, "Notifications full, discarding oldest"); 
        memmove(&notifications[0], &notifications[1], (MAX_NOTIFICATIONS-1)*sizeof(notification_t));
        total_notifications--;
    }
    memcpy(&notifications[total_notifications], event->data, sizeof(notification_t)); 
    strncpy(notifications[total_notifications].action_name, "Open App", 9);
    strncpy(notifications[total_notifications].dismiss_text, "Dismiss", 8); 
    assign_handlers(&notifications[total_notifications]);
    total_notifications++;    
    ESP_LOGI(TAG, "Notification received");
}                                                                                           
                                                                                            
void notification_manager_init(void) {                                                      
    if(initialized) return;                                                                 
    total_notifications = 0;                                                                
    event_subscribe(EVENT_NOTIFICATION_RECEIVED, receive_notification);                     
    initialized = true;                                                                     
}                                                                                           

void notification_manager_deinit(void) {
    event_unsubscribe(EVENT_NOTIFICATION_RECEIVED, receive_notification);
    total_notifications = 0;
    initialized = false;
}

unsigned int notification_manager_get_notification_count(void) {
    return total_notifications;
}

const notification_t* notification_manager_retreive_all_notification(unsigned int* count) {
    if(!initialized || count == NULL) return NULL;
    *count = total_notifications;
    return notifications;
}

const notification_t* notification_manager_retreive_notification(unsigned int index) {
    if(!initialized) return NULL;
    if(index >= total_notifications) return NULL;
    return &notifications[index];
}

void notification_manager_dismiss_notification(unsigned int index) {
    if(!initialized) return;
    if(index >= total_notifications) return;
    if(index != total_notifications-1)
        memmove(&notifications[index], &notifications[index+1], 
                (total_notifications - index - 1)*sizeof(notification_t));
    total_notifications--;
}


