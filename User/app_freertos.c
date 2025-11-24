#include "app_freertos.h"
#include "FreeRTOS.h"
#include "task.h"
#include "system.h"
#include "led.h"
#include "usart.h"
#include "wifi_config.h"
#include "wifi_function.h"
#include "mqtt_client.h"
#include "stm32f10x.h"
#include "dht11.h"
#include "adc_temp.h"
#include "tftlcd.h"
#include "lsens.h"
#include "beep.h"

// Task handles
TaskHandle_t xStartTaskHandle = NULL;
TaskHandle_t xSensorTaskHandle = NULL;

// Task prototypes
void StartTask(void *pvParameters);
void SensorTask(void *pvParameters);

void App_Start(void)
{
    // Create Start Task
    xTaskCreate(StartTask, "StartTask", 512, NULL, 2, &xStartTaskHandle);
    
    // Start Scheduler
    vTaskStartScheduler();
}

void StartTask(void *pvParameters)
{
    // Initialize Hardware
    // SysTick, USART, LED are initialized in main.c
    
    PC_Usart("FreeRTOS Started\r\n");
    
    // Initialize WiFi Hardware
    WiFi_Config();
    PC_Usart("WiFi UART Configured\r\n");
    
    // Initialize ESP8266
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP8266_Choose(ENABLE);
    PC_Usart("ESP8266 Enabled\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP8266_AT_Test();
    PC_Usart("ESP8266 AT Test OK\r\n");
    
    ESP8266_Net_Mode_Choose(STA);
    PC_Usart("ESP8266 STA Mode Set\r\n");
    
    // Initialize Sensors & LCD
    TFTLCD_Init();
    ADC_Temp_Init();
    Lsens_Init();
    BEEP_Init();
    BEEP = 0; // Ensure Beep is OFF
    g_beep_state = 0;
    
    FRONT_COLOR = BLACK;
    LCD_ShowString(10, 50, tftlcd_data.width, tftlcd_data.height, 16, "DHT11");
    LCD_ShowString(10, 100, tftlcd_data.width, tftlcd_data.height, 16, "Temp:                 C");
    LCD_ShowString(10, 130, tftlcd_data.width, tftlcd_data.height, 16, "Humi:                 %RH");
    LCD_ShowString(10, 160, tftlcd_data.width, tftlcd_data.height, 16, "CPU Temp:             C");
    LCD_ShowString(10, 190, tftlcd_data.width, tftlcd_data.height, 16, "Light:                ");
    LCD_ShowString(10, 220, tftlcd_data.width, tftlcd_data.height, 16, "Beep:                 ");
    LCD_ShowString(10, 250, tftlcd_data.width, tftlcd_data.height, 16, "MQTT: Wait...         ");
    FRONT_COLOR = RED;
    
    while(DHT11_Init())
    {
        LCD_ShowString(130, 50, tftlcd_data.width, tftlcd_data.height, 16, "Error   ");
        PC_Usart("DHT11 Check Error!\r\n");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    LCD_ShowString(130, 50, tftlcd_data.width, tftlcd_data.height, 16, "Success");
    PC_Usart("DHT11 Check OK!\r\n");

    xTaskCreate(SensorTask, "SensorTask", 512, NULL, 3, &xSensorTaskHandle);

    // Create MQTT Task
    MQTT_Client_Init();
    
    // Delete Start Task
    vTaskDelete(NULL);
}

// Hook functions removed as they are disabled in FreeRTOSConfig.h

// Assert implementation for core_mqtt
void __aeabi_assert(const char *expr, const char *file, int line)
{
    PC_Usart("ASSERT FAILED: %s at %s:%d\r\n", expr, file, line);
    while(1);
}

void SensorTask(void *pvParameters)
{
    u8 temp, humi;
    u8 temp_buf[3], humi_buf[3];
    int cpu_temp;
    u8 cpu_temp_buf[6];
    
    while(1)
    {
        taskENTER_CRITICAL();
        u8 res = DHT11_Read_Data(&temp, &humi);
        taskEXIT_CRITICAL();
        
        if (res == 0) {
            // Update Globals for MQTT
            g_sensor_temp = temp;
            g_sensor_humi = humi;
            
            // LCD Update
            temp_buf[0] = temp / 10 + 0x30;	
            temp_buf[1] = temp % 10 + 0x30;
            temp_buf[2] = '\0';
            LCD_ShowString(130, 100, tftlcd_data.width, tftlcd_data.height, 16, temp_buf);
                
            humi_buf[0] = humi / 10 + 0x30;	
            humi_buf[1] = humi % 10 + 0x30;
            humi_buf[2] = '\0';
            LCD_ShowString(130, 130, tftlcd_data.width, tftlcd_data.height, 16, humi_buf);
        } else {
            PC_Usart("DHT11 Read Failed: %d\r\n", res);
        }

        // Light Sensor
        g_sensor_light = Lsens_Get_Val();
        u8 light_buf[4];
        if (g_sensor_light > 99) {
            light_buf[0] = g_sensor_light / 100 + 0x30;
            light_buf[1] = (g_sensor_light % 100) / 10 + 0x30;
            light_buf[2] = g_sensor_light % 10 + 0x30;
            light_buf[3] = '\0';
        } else if (g_sensor_light > 9) {
            light_buf[0] = g_sensor_light / 10 + 0x30;
            light_buf[1] = g_sensor_light % 10 + 0x30;
            light_buf[2] = ' ';
            light_buf[3] = '\0';
        } else {
            light_buf[0] = g_sensor_light + 0x30;
            light_buf[1] = ' ';
            light_buf[2] = ' ';
            light_buf[3] = '\0';
        }
        LCD_ShowString(130, 190, tftlcd_data.width, tftlcd_data.height, 16, light_buf);

        // Beep Status
        if (g_beep_state) {
            LCD_ShowString(130, 220, tftlcd_data.width, tftlcd_data.height, 16, "ON ");
        } else {
            LCD_ShowString(130, 220, tftlcd_data.width, tftlcd_data.height, 16, "OFF");
        }

        // MQTT Status
        if (g_mqtt_connected) {
            LCD_ShowString(60, 250, tftlcd_data.width, tftlcd_data.height, 16, "Connected   ");
        } else {
            LCD_ShowString(60, 250, tftlcd_data.width, tftlcd_data.height, 16, "Disconnected");
        }

        // CPU Temp
        cpu_temp = Get_Temperture();
        g_sensor_cpu_temp = cpu_temp / 100.0f;
        
        cpu_temp_buf[0] = cpu_temp / 1000 + 0x30;
        cpu_temp_buf[1] = (cpu_temp % 1000) / 100 + 0x30;
        cpu_temp_buf[2] = '.';
        cpu_temp_buf[3] = (cpu_temp % 100) / 10 + 0x30;
        cpu_temp_buf[4] = cpu_temp % 10 + 0x30;
        cpu_temp_buf[5] = '\0';
        LCD_ShowString(130, 160, tftlcd_data.width, tftlcd_data.height, 16, cpu_temp_buf);
        
        vTaskDelay(pdMS_TO_TICKS(2000)); // Read every 2 seconds
    }
}
