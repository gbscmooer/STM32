#ifndef __MQTT_CLIENT_H
#define __MQTT_CLIENT_H

#include <stdint.h>

void MQTT_Client_Init(void);

extern volatile uint8_t g_sensor_temp;
extern volatile uint8_t g_sensor_humi;
extern volatile float g_sensor_cpu_temp;

#endif
