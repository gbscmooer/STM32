#ifndef __MQTT_CLIENT_H
#define __MQTT_CLIENT_H

#include <stdint.h>

void MQTT_Client_Init(void);

extern volatile uint8_t g_sensor_temp;
extern volatile uint8_t g_sensor_humi;
extern volatile float g_sensor_cpu_temp;
extern volatile uint8_t g_sensor_light;
extern volatile uint8_t g_beep_state;
extern volatile uint8_t g_mqtt_connected;
extern volatile uint8_t g_camera_upload_request;  // 触发相机上传的标志

#endif
