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
	bool wifi_connected = false;
	bool ping_test_passed = false;
	
	// ========== 第1步：系统初始化 ==========
	PC_Usart("\r\n");
	PC_Usart("================================================\r\n");
	PC_Usart("    STM32 ESP8266 WiFi天气站系统启动中...    \r\n");
	PC_Usart("================================================\r\n");
	
	// 初始化系统时钟和基本外设
	SysTick_Init(72);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	PC_Usart("? 系统时钟初始化完成\r\n");
	
	// 初始化串口
	USART1_Init(115200);
	PC_Usart("? 串口初始化完成(波特率:115200)\r\n");
	
	// 初始化LED
	LED_Init();
	LED1 = 0;  // 点亮LED1表示系统正在运行
	PC_Usart("? LED初始化完成\r\n");
	
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
	
	// ========== 第2步：连接WiFi网络 ==========
	// 首先尝试连接默认WiFi
	wifi_connected = WiFi_Connect_Default();
	
	// 如果连接失败，进入菜单处理
	if(!wifi_connected)
	{
		wifi_connected = WiFi_Connect_Menu();
	}
	
	// 检查WiFi是否最终连接成功
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
	
	// 判断Ping测试结果
	if(ping1 && ping2)
	{
		ping_test_passed = true;
		PC_Usart("\r\n? 网络连通性测试通过！\r\n");
		PC_Usart("? 两个DNS服务器都可以正常访问\r\n");
		LED1 = 0;  // 点亮LED1表示成功
	}
	else if(ping1 || ping2)
	{
		ping_test_passed = true;
		PC_Usart("\r\n? 网络连通性测试部分通过\r\n");
		PC_Usart("? 至少一个DNS服务器可以访问\r\n");
		LED1 = 0;  // 点亮LED1表示基本成功
	}
	else
	{
		PC_Usart("\r\n? 网络连通性测试失败！\r\n");
		PC_Usart("? 无法访问外部网络\r\n");
		PC_Usart("可能的原因：\r\n");
		PC_Usart("1. 路由器未连接到互联网\r\n");
		PC_Usart("2. 网络防火墙阻止了ICMP包\r\n");
		PC_Usart("3. DNS服务器地址无法访问\r\n");
	}
	
	// ========== 系统就绪 ==========
	PC_Usart("\r\n");
	PC_Usart("================================================\r\n");
	PC_Usart("        系统初始化完成，进入主循环          \r\n");
	PC_Usart("================================================\r\n");
	PC_Usart("WiFi连接状态: %s\r\n", wifi_connected ? "已连接" : "未连接");
	PC_Usart("网络测试状态: %s\r\n", ping_test_passed ? "通过" : "失败");
	PC_Usart("\r\n等待后续功能开发...\r\n");
	
	// 主循环
	while(1)
	{
		// LED闪烁表示系统正常运行
		LED2 = !LED2;
		delay_ms(1000);
		
		// 这里可以添加后续的功能代码
		// 例如：获取天气信息、显示数据等
	}
}
