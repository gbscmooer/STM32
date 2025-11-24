#include "mqtt_client.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include "wifi_function.h"
#include "usart.h"
#include <string.h>
#include <stdio.h>

// MQTT Broker Settings
#define MQTT_BROKER_IP "47.83.221.161"
#define MQTT_BROKER_PORT 1883
#define MQTT_CLIENT_ID "STM32_Weather_Client"
#define MQTT_TOPIC "weather/update"

extern StreamBufferHandle_t xSerialStreamBuffer;

// Task Handle
TaskHandle_t xMQTTTaskHandle = NULL;

volatile uint8_t g_sensor_temp = 0;
volatile uint8_t g_sensor_humi = 0;
volatile float g_sensor_cpu_temp = 0.0f;

// MQTT Task
void MQTTTask(void *pvParameters);

void MQTT_Client_Init(void)
{
    xTaskCreate(MQTTTask, "MQTTTask", 1024, NULL, 3, &xMQTTTaskHandle);
}

// Helper to read from StreamBuffer
int Transport_Recv(char *buffer, int len, int timeout_ms)
{
    if (xSerialStreamBuffer == NULL) return 0;
    return xStreamBufferReceive(xSerialStreamBuffer, buffer, len, pdMS_TO_TICKS(timeout_ms));
}

void ProcessMQTTPacket(uint8_t *data, int len) {
    if (len < 2) return;
    
    uint8_t cmd = data[0];
    
    // Parse Remaining Length
    int rem_len = 0;
    int multiplier = 1;
    int idx = 1;
    uint8_t encodedByte;
    do {
        if (idx >= len) return;
        encodedByte = data[idx++];
        rem_len += (encodedByte & 127) * multiplier;
        multiplier *= 128;
        if (multiplier > 128*128*128) return; // Error
    } while ((encodedByte & 128) != 0);
    
    if ((cmd & 0xF0) == 0x30) { // PUBLISH
        // Variable Header: Topic Name
        if (idx + 2 > len) return;
        int topic_len = (data[idx] << 8) | data[idx+1];
        idx += 2;
        
        if (idx + topic_len > len) return;
        
        char topic[64];
        int copy_len = (topic_len < 63) ? topic_len : 63;
        memcpy(topic, &data[idx], copy_len);
        topic[copy_len] = 0;
        idx += topic_len;
        
        // Payload
        int payload_len = len - idx;
        char payload[128];
        int p_copy_len = (payload_len < 127) ? payload_len : 127;
        if (p_copy_len > 0) {
            memcpy(payload, &data[idx], p_copy_len);
            payload[p_copy_len] = 0;
        } else {
            payload[0] = 0;
        }
        
        PC_Usart("\r\n--------------------------------\r\n");
        PC_Usart("MQTT Message Received\r\n");
        PC_Usart("Topic: %s\r\n", topic);
        PC_Usart("Payload: %s\r\n", payload);
        PC_Usart("--------------------------------\r\n");
    }
    else if ((cmd & 0xF0) == 0x20) {
        PC_Usart("\r\n[MQTT] CONNACK Received\r\n");
    }
    else if ((cmd & 0xF0) == 0x90) {
        PC_Usart("\r\n[MQTT] SUBACK Received\r\n");
    }
}

void MQTTTask(void *pvParameters)
{
    bool wifi_connected = false;
    bool tcp_connected = false;
    char mqtt_packet[128];
    
    while(1)
    {
        // Step 1: Connect WiFi (only once)
        if (!wifi_connected)
        {
            PC_Usart("Connecting to WiFi...\r\n");
            if (WiFi_Connect_Default())
            {
                PC_Usart("WiFi Connected.\r\n");
                wifi_connected = true;
            }
            else
            {
                PC_Usart("WiFi Connect Failed. Retrying in 5s...\r\n");
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
        }
        
        // Step 2: Connect TCP (retry if failed, without reconnecting WiFi)
        if (!tcp_connected)
        {
            PC_Usart("Connecting to TCP %s:%d...\r\n", MQTT_BROKER_IP, MQTT_BROKER_PORT);
            
            if (ESP8266_TCP_Connect(MQTT_BROKER_IP, MQTT_BROKER_PORT))
            {
                PC_Usart("TCP Connected. Sending MQTT Connect...\r\n");
                tcp_connected = true;
                
                // MQTT CONNECT Packet
                int client_id_len = strlen(MQTT_CLIENT_ID);
                int remaining_len = 10 + (2 + client_id_len);
                
                int idx = 0;
                mqtt_packet[idx++] = 0x10;
                mqtt_packet[idx++] = remaining_len;
                
                // Protocol Name "MQTT"
                mqtt_packet[idx++] = 0x00; mqtt_packet[idx++] = 0x04;
                mqtt_packet[idx++] = 'M'; mqtt_packet[idx++] = 'Q'; mqtt_packet[idx++] = 'T'; mqtt_packet[idx++] = 'T';
                
                mqtt_packet[idx++] = 0x04; // Level
                mqtt_packet[idx++] = 0x02; // Flags
                mqtt_packet[idx++] = 0x00; mqtt_packet[idx++] = 0x3C; // Keep Alive 60s
                
                // Client ID
                mqtt_packet[idx++] = (client_id_len >> 8) & 0xFF;
                mqtt_packet[idx++] = client_id_len & 0xFF;
                memcpy(&mqtt_packet[idx], MQTT_CLIENT_ID, client_id_len);
                idx += client_id_len;
                
                ESP8266_TCP_Send(mqtt_packet, idx);
                
                PC_Usart("MQTT Connect Sent. Waiting for CONNACK...\r\n");
                vTaskDelay(pdMS_TO_TICKS(3000));
                
                // Clear buffer
                char temp_buf[100];
                while(Transport_Recv(temp_buf, sizeof(temp_buf), 100) > 0);
                
                // Subscribe
                PC_Usart("Subscribing to %s...\r\n", MQTT_TOPIC);
                
                int topic_len = strlen(MQTT_TOPIC);
                remaining_len = 2 + (2 + topic_len) + 1;
                
                idx = 0;
                mqtt_packet[idx++] = 0x82;
                mqtt_packet[idx++] = remaining_len;
                mqtt_packet[idx++] = 0x00; mqtt_packet[idx++] = 0x01; // Packet ID
                mqtt_packet[idx++] = (topic_len >> 8) & 0xFF;
                mqtt_packet[idx++] = topic_len & 0xFF;
                memcpy(&mqtt_packet[idx], MQTT_TOPIC, topic_len);
                idx += topic_len;
                mqtt_packet[idx++] = 0x00; // QoS 0
                
                if(ESP8266_TCP_Send(mqtt_packet, idx))
                {
                    PC_Usart("MQTT Subscribe Sent. Waiting for messages...\r\n");
                }
                else
                {
                    PC_Usart("Subscribe Send Failed. Reconnecting TCP...\r\n");
                    tcp_connected = false;
                    ESP8266_TCP_Disconnect();
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    continue;
                }
            }
            else
            {
                PC_Usart("TCP Connect Failed. Retrying in 3s...\r\n");
                ESP8266_Cmd("AT+CIPCLOSE", "OK", "CLOSED", 1000);
                vTaskDelay(pdMS_TO_TICKS(3000));
                continue;
            }
        }
        
        // Step 3: Receive MQTT messages
        if (tcp_connected)
        {
            // Publish Sensor Data every 5 seconds
            static TickType_t last_publish_time = 0;
            if (xTaskGetTickCount() - last_publish_time >= pdMS_TO_TICKS(5000))
            {
                last_publish_time = xTaskGetTickCount();
                
                char payload[64];
                sprintf(payload, "{\"temp\":%d,\"humi\":%d,\"cpu\":%.2f}", g_sensor_temp, g_sensor_humi, g_sensor_cpu_temp);
                
                int payload_len = strlen(payload);
                int topic_len = strlen(MQTT_TOPIC);
                int remaining_len = 2 + topic_len + payload_len; 
                
                char pub_packet[128];
                int idx = 0;
                pub_packet[idx++] = 0x30; // PUBLISH, QoS 0
                
                // Remaining Length (assuming < 128 for this short payload)
                if (remaining_len > 127) {
                    pub_packet[idx++] = (remaining_len & 0x7F) | 0x80;
                    pub_packet[idx++] = (remaining_len >> 7);
                } else {
                    pub_packet[idx++] = remaining_len;
                }
                
                // Topic Name
                pub_packet[idx++] = (topic_len >> 8) & 0xFF;
                pub_packet[idx++] = topic_len & 0xFF;
                memcpy(&pub_packet[idx], MQTT_TOPIC, topic_len);
                idx += topic_len;
                
                // Payload
                memcpy(&pub_packet[idx], payload, payload_len);
                idx += payload_len;
                
                ESP8266_TCP_Send(pub_packet, idx);
                PC_Usart("Published: %s\r\n", payload);
            }

            char ch;
            if (Transport_Recv(&ch, 1, 100) > 0)
            {
                static int state = 0;
                static int len_cnt = 0;
                static int data_len = 0;
                static char len_str[10];
                static uint8_t rx_buffer[256];
                static int rx_idx = 0;
                
                switch(state)
                {
                    case 0: if (ch == '+') state = 1; break;
                    case 1: if (ch == 'I') state = 2; else state = 0; break;
                    case 2: if (ch == 'P') state = 3; else state = 0; break;
                    case 3: if (ch == 'D') state = 4; else state = 0; break;
                    case 4: if (ch == ',') { state = 5; len_cnt = 0; } else state = 0; break;
                    case 5: 
                        if (ch >= '0' && ch <= '9') {
                            len_str[len_cnt++] = ch;
                            if (len_cnt >= 9) state = 0;
                        } else if (ch == ':') {
                            len_str[len_cnt] = 0;
                            sscanf(len_str, "%d", &data_len);
                            state = 6;
                            rx_idx = 0;
                        } else {
                            state = 0;
                        }
                        break;
                    case 6:
                        if (data_len > 0) {
                            if (rx_idx < 256) {
                                rx_buffer[rx_idx++] = (uint8_t)ch;
                            }
                            data_len--;
                            if (data_len == 0) {
                                ProcessMQTTPacket(rx_buffer, rx_idx);
                                state = 0;
                            }
                        }
                        break;
                }
            }
        }
    }
}
