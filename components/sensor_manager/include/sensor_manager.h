#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

void sensor_manager_init(void);
void sensor_manager_deinit(void);
void sensor_manager_arm_wakeup_interrupt(void);
void sensor_manager_disarm_wakeup_interrupt(void);

#endif /* SENSOR_MANAGER_H */
