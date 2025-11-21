#include "system.h"
#include "SysTick.h"
#include "led.h"
#include "usart.h"
#include "wifi_config.h"
#include "wifi_function.h"
#include <string.h>
#include <stdio.h>

/**
  * @brief  main 主函数
  * @param  无
  * @retval 无
  */

int main(void)
{
	int i = 0;
	bool wifi_connected = false;
	// 初始化系统时钟和基本外设,LED2绿色正常,LED1红色错误
	SysTick_Init(72);
	USART1_Init(115200);
	LED_Init();
	

	while(i < 10){
		LED1=!LED1;
		LED2=!LED2;
		delay_ms(200);
		i++;
	}
	PC_Usart("USART Initialized OK\r\n");
	PC_Usart("LED Initialized OK\r\n");
	
	
	// 初始化WiFi配置
	WiFi_Config();
	PC_Usart("? WiFi串口配置完成\r\n");
	
	// 初始化ESP8266模块
	PC_Usart("\r\n=== 初始化ESP8266模块 ===\r\n");
	delay_ms(1000);
	ESP8266_Choose(ENABLE);
	PC_Usart("? ESP8266模块使能\r\n");
	delay_ms(500);
	
	ESP8266_AT_Test();
	PC_Usart("? ESP8266模块AT测试通过\r\n");
	
	// 设置为STA模式
	ESP8266_Net_Mode_Choose(STA);
	PC_Usart("? ESP8266设置为STA模式\r\n");
	
	PC_Usart("\r\n=== ESP8266模块初始化完成 ===\r\n");
	LED1 = 1;  // 熄灭LED1
	LED2 = 0;  // 点亮LED2表示WiFi模块就绪

	// 扫描周围WiFi网络
	PC_Usart("\r\n=== 扫描周围WiFi网络 ===\r\n");
	ESP8266_Scan_AP();
	PC_Usart("? 扫描周围WiFi完成\r\n");
	
	// 等待ESP8266完全就绪
	delay_ms(2000);
	
	// 首先尝试连接默认WiFi
	wifi_connected = WiFi_Connect_Default();

	// 如果连接失败，进入菜单处理
	if(!wifi_connected)
	{
		wifi_connected = WiFi_Connect_Menu();
	}

		if(!wifi_connected)
	{
		PC_Usart("\r\n? 未能连接到WiFi网络，系统停止\r\n");
		LED2 = 1;  // 熄灭LED2
		LED1 = 0;  // 点亮LED1表示错误
		while(1)
		{
			LED1 = !LED1;  // LED1闪烁表示错误状态
			delay_ms(500);
		}
	}

	PC_Usart("\r\n? WiFi连接成功，准备进行网络测试\r\n");
	LED2 = 1;  // 熄灭LED2
	
	// ========== 第3步：执行Ping测试 ==========
	PC_Usart("\r\n=== 开始网络连通性测试 ===\r\n");
	
	// Ping 1.1.1.1 (Cloudflare DNS)
	bool ping1 = WiFi_Ping_Test("1.1.1.1");
	delay_ms(1000);
	
	// Ping 8.8.8.8 (Google DNS)
	bool ping2 = WiFi_Ping_Test("8.8.8.8");
	delay_ms(1000);

	if(ping1 || ping2){
		LED2 = 0;  // 点亮LED2表示Ping测试成功
		PC_Usart("\r\n? Ping测试成功，网络连接正常\r\n");
	}else{
		LED1 = 0;  // 点亮LED1表示Ping测试失败
		PC_Usart("\r\n? Ping测试失败，网络连接异常\r\n");
	}

	// ========== 主循环：定时获取天气数据 ==========
	PC_Usart("\r\n=== 进入天气监测主循环 ===\r\n");
	PC_Usart("每60秒更新一次天气数据\r\n\r\n");
	
	WeatherData weather;
	u32 last_update_time = 0;
	u32 current_time = 0;
	u32 update_interval = 60000;  // 默认60秒
	
	// 首次获取天气数据
	if(Weather_Get_Data(&weather))
	{
		Weather_Display(&weather);
		LED2 = 0;  // 点亮LED2表示成功
		update_interval = 60000; // 成功后60秒更新
	}
	else
	{
		PC_Usart("? 首次获取天气数据失败\r\n");
		LED1 = 0;  // 点亮LED1表示失败
		update_interval = 5000; // 失败后5秒重试
	}
	
	last_update_time = 0;  // 重置计时器

	while(1)
	{
		// 获取当前时间（毫秒）
		current_time += 1000;  // 每次循环1秒
		
		// 检查是否到达更新间隔
		if(current_time - last_update_time >= update_interval)
		{
			PC_Usart("\r\n>>> 定时更新天气数据 (间隔: %d秒) <<<\r\n", update_interval/1000);
			
			// 获取并显示天气数据
			if(Weather_Get_Data(&weather))
			{
				Weather_Display(&weather);
				LED2 = 0;  // 点亮LED2表示成功
				LED1 = 1;  // 熄灭LED1
				update_interval = 60000; // 成功后60秒更新
			}
			else
			{
				PC_Usart("? 获取天气数据失败，5秒后重试\r\n");
				LED1 = 0;  // 点亮LED1表示失败
				update_interval = 5000; // 失败后5秒重试
			}
			
			last_update_time = current_time;
		}
		
		// LED闪烁表示系统正常运行
		LED2 = !LED2;
		delay_ms(1000);
		
	}
}

