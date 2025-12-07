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
#include "camera_capture.h"
#include "http_upload.h"
#include "malloc.h"

// Task handles
TaskHandle_t xStartTaskHandle = NULL;
TaskHandle_t xSensorTaskHandle = NULL;
TaskHandle_t xCameraTaskHandle = NULL;

// Task prototypes
void StartTask(void *pvParameters);
void SensorTask(void *pvParameters);
void CameraTask(void *pvParameters);

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
    
    // 初始化OV7670摄像头
    PC_Usart("Initializing Camera...\r\n");
    if(Camera_Init() != 0)
    {
        PC_Usart("Warning: Camera initialization failed, continuing without camera\r\n");
        LCD_ShowString(10, 280, tftlcd_data.width, tftlcd_data.height, 16, "Camera: Failed");
    }
    else
    {
        PC_Usart("Camera initialized successfully\r\n");
        LCD_ShowString(10, 280, tftlcd_data.width, tftlcd_data.height, 16, "Camera: Ready ");
    }

    xTaskCreate(SensorTask, "SensorTask", 512, NULL, 3, &xSensorTaskHandle);
    
    // 创建相机任务（图像缓冲区使用动态分配，栈空间不需要太大）
    xTaskCreate(CameraTask, "CameraTask", 512, NULL, 2, &xCameraTaskHandle);

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

/*******************************************************************************
* 函 数 名: CameraTask
* 功能说明: 相机图像采集和上传任务
* 输    入: pvParameters - 任务参数（未使用）
* 输    出: 无
* 说    明: 该任务等待MQTT任务触发，然后采集图像并通过HTTP上传
*           使用动态内存分配避免栈溢出
*******************************************************************************/
void CameraTask(void *pvParameters)
{
    u8* image_buffer = NULL;
    
    PC_Usart("Camera Task Started\r\n");
    
    while(1)
    {
        // 等待MQTT任务触发相机上传请求
        if(g_camera_upload_request)
        {
            PC_Usart("\r\n========================================\r\n");
            PC_Usart("Camera Upload Request Received\r\n");
            PC_Usart("========================================\r\n");
            
            LCD_ShowString(10, 280, tftlcd_data.width, tftlcd_data.height, 16, "Camera: Capturing...");
            
            // 动态分配图像缓冲区（19200字节）
            image_buffer = (u8*)mymalloc(SRAMIN, CAMERA_GRAY_SIZE);
            
            if(image_buffer == NULL)
            {
                PC_Usart("Error: Failed to allocate image buffer (%d bytes)\r\n", CAMERA_GRAY_SIZE);
                LCD_ShowString(10, 280, tftlcd_data.width, tftlcd_data.height, 16, "Camera: Mem Error  ");
                g_camera_upload_request = 0;  // 清除请求标志
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            
            PC_Usart("Image buffer allocated: %d bytes\r\n", CAMERA_GRAY_SIZE);
            
            // 捕获灰度图像
            if(Camera_Capture_Gray(image_buffer, CAMERA_GRAY_SIZE) == 0)
            {
                PC_Usart("Image captured successfully\r\n");
                LCD_ShowString(10, 280, tftlcd_data.width, tftlcd_data.height, 16, "Camera: Uploading...  ");
                
                // 通过HTTP上传图像
                if(HTTP_Upload_Image(image_buffer, CAMERA_GRAY_SIZE) == 0)
                {
                    PC_Usart("Image uploaded successfully\r\n");
                    LCD_ShowString(10, 280, tftlcd_data.width, tftlcd_data.height, 16, "Camera: Upload OK     ");
                }
                else
                {
                    PC_Usart("Image upload failed\r\n");
                    LCD_ShowString(10, 280, tftlcd_data.width, tftlcd_data.height, 16, "Camera: Upload Failed ");
                }
            }
            else
            {
                PC_Usart("Image capture failed\r\n");
                LCD_ShowString(10, 280, tftlcd_data.width, tftlcd_data.height, 16, "Camera: Capture Failed");
            }
            
            // 释放图像缓冲区
            myfree(SRAMIN, image_buffer);
            image_buffer = NULL;
            PC_Usart("Image buffer freed\r\n");
            
            // 清除请求标志，通知MQTT任务完成
            g_camera_upload_request = 0;
            
            PC_Usart("========================================\r\n");
            PC_Usart("Camera Task Cycle Complete\r\n");
            PC_Usart("========================================\r\n\r\n");
        }
        
        // 短暂延时，避免CPU占用过高
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
