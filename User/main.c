#include "system.h"
#include "SysTick.h"
#include "led.h"
#include "usart.h"
#include "wifi_config.h"
#include "wifi_function.h"
#include <string.h>
#include <stdio.h>


void (*pNet_Test)(void);

int main()
{
	char cCh;	
	
	SysTick_Init(72);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);  //中断优先级分组 分2组
	USART1_Init(115200);
	LED_Init();
	WiFi_Config();
	
	// 初始化ESP8266模块
	delay_ms(1000);  // 等待模块稳定
	ESP8266_Choose(ENABLE);  // 使能ESP8266
	delay_ms(500);
	// ESP8266_Auto_BaudRate_Test();  // 波特率自动检测（函数未实现，已注释）
	ESP8266_AT_Test();  // AT测试
	
	PC_Usart("\r\n普中PZ-ESP8266 WiFi模块测试例程\r\n"); //打印测试例程提示信息
	PC_Usart("\r\nESP8266模块初始化完成！\r\n");
	PC_Usart ( "\r\n请输入模块的测试模式的编号，编号如下：\
              \r\n1.STA模式： TCP Client\
              \r\n2.AP模式： AP Server\
              \r\n3.STA+AP模式： (STA)TCP Client + (AP)TCP Server\
              \r\n4.简单WiFi连接测试\
              \r\n5.简单TCP连接测试（单连接）\
              \r\n输入格式为：编号+空格，点击发送\r\n" );
	
	scanf("%c", &cCh );
	PC_Usart("\r\n收到输入: %c\r\n", cCh);
	
	switch(cCh)
	{
		case '1':
			pNet_Test = ESP8266_STA_TCP_Client;
			PC_Usart("\r\n即将进行的测试是：1.STA模式： TCP Client\r\n");
			delay_ms(1000);  // 给用户时间看到消息
		  break;
		
		case '2':
			pNet_Test = ESP8266_AP_TCP_Server;
			PC_Usart("\r\n即将进行的测试是：2.AP模式： TCP Server\r\n");
			delay_ms(1000);
		  break;

		case '3':
			pNet_Test = ESP8266_StaTcpClient_ApTcpServer;
			PC_Usart("\r\n即将进行的测试是：3.STA+AP模式： (STA)TCP Client + (AP)TCP Server\r\n");
			delay_ms(1000);
		  break;
		
		case '4':
			pNet_Test = NULL;  // 不使用预定义函数，直接在这里处理
			PC_Usart("\r\n即将进行简单WiFi连接测试...\r\n");
			delay_ms(1000);
			
			// 简单WiFi连接测试
			ESP8266_Net_Mode_Choose(STA);
			ESP8266_Cmd("AT+CWLAP", "OK", 0, 5000);
			
			PC_Usart("\r\n建议选择以下英文WiFi网络进行测试：\r\n");
			PC_Usart("1. Xiaomi_CAEE\r\n");
			PC_Usart("2. TP-LINK_E45C\r\n");
			PC_Usart("\r\n请输入要连接的WiFi名称和密钥，输入格式为：名称,密钥 或 名称:密钥，点击发送\r\n");
			
			{
				char cStrInput[100] = {0};
				char *pStrDelimiter[2];
				char *pBuf, *pStr;
				u8 uc = 0;
				
				scanf("%s", cStrInput);
				PC_Usart("\r\n稍等片刻 ……\r\n");
				
				// 先尝试用逗号分割
				pBuf = cStrInput;
				uc = 0;
				while ((pStr = strtok(pBuf, ",")) != NULL) {
					pStrDelimiter[uc++] = pStr;
					pBuf = NULL;
				}
				
				// 如果逗号分割失败，尝试用冒号分割
				if(uc < 2) {
					pBuf = cStrInput;
					uc = 0;
					while ((pStr = strtok(pBuf, ":")) != NULL) {
						pStrDelimiter[uc++] = pStr;
						pBuf = NULL;
					}
				}
				
				if(uc >= 2) {
					ESP8266_JoinAP(pStrDelimiter[0], pStrDelimiter[1]);
				}
				else {
					PC_Usart("输入格式错误！\r\n");
				}
			}
		  break;
		
		case '5':
			pNet_Test = NULL;  // 简单TCP连接测试
			PC_Usart("\r\n即将进行简单TCP连接测试...\r\n");
			delay_ms(1000);
			
			// 首先检查WiFi连接状态
			ESP8266_Cmd("AT+CWJAP?", "OK", NULL, 2000);
			ESP8266_Cmd("AT+CIFSR", "OK", NULL, 2000);
			
			// 设置为单连接模式
			ESP8266_Enable_MultipleId(DISABLE);
			
			PC_Usart("\r\n请确保电脑上的TCP服务器已启动！\r\n");
			PC_Usart("请输入电脑的IP和端口号，输入格式为：IP:端口 或 IP,端口，点击发送\r\n");
			
			{
				char cStrInput[100] = {0};
				char *pStrDelimiter[2];
				char *pBuf, *pStr;
				char *response;
				u8 uc = 0;
				
				scanf("%s", cStrInput);
				PC_Usart("\r\n稍等片刻 ……\r\n");
				
				// 先尝试用冒号分割
				pBuf = cStrInput;
				uc = 0;
				while ((pStr = strtok(pBuf, ":")) != NULL) {
					pStrDelimiter[uc++] = pStr;
					pBuf = NULL;
				}
				
				// 如果冒号分割失败，尝试用逗号分割
				if(uc < 2) {
					pBuf = cStrInput;
					uc = 0;
					while ((pStr = strtok(pBuf, ",")) != NULL) {
						pStrDelimiter[uc++] = pStr;
						pBuf = NULL;
					}
				}
				
				if(uc >= 2) {
					// 尝试单个TCP连接
					if(ESP8266_Link_Server(enumTCP, pStrDelimiter[0], pStrDelimiter[1], Single_ID_0))
					{
						PC_Usart("\r\n? TCP连接建立成功！可以开始数据通信测试\r\n");
						
						// 简单的数据发送测试
						PC_Usart("发送测试消息...\r\n");
						ESP8266_SendString(DISABLE, "Hello from ESP8266!", 19, Single_ID_0);
						
						PC_Usart("等待服务器响应...\r\n");
						response = ESP8266_ReceiveString(DISABLE);
						if(response) {
							PC_Usart("收到服务器响应: %s\r\n", response);
						}
					}
					else {
						PC_Usart("\r\n? TCP连接失败！请检查:\r\n");
						PC_Usart("1. 服务器是否启动\r\n");
						PC_Usart("2. IP地址是否正确\r\n");
						PC_Usart("3. 端口号是否正确\r\n");
						PC_Usart("4. 防火墙设置\r\n");
					}
				}
				else {
					PC_Usart("输入格式错误！\r\n");
				}
			}
		  break;

		default:
			pNet_Test = NULL;
			PC_Usart("\r\n输入错误，不进行测试！收到字符码: 0x%02X\r\n", cCh);
		  break;		
			
	}
	if(pNet_Test)
	{
		PC_Usart("\r\n开始执行选定的测试模式...\r\n");
		
		// 再次确认ESP8266模块通信正常
		if(!ESP8266_Cmd("AT", "OK", NULL, 1000))
		{
			PC_Usart("\r\n警告：ESP8266模块通信异常，尝试重新初始化...\r\n");
			// ESP8266_Auto_BaudRate_Test();  // 函数未实现，已注释
			ESP8266_AT_Test();
		}
		else
		{
			PC_Usart("\r\nESP8266模块通信正常，开始测试...\r\n");
		}
		
		pNet_Test();
	}
	else
	{
		PC_Usart("\r\n未选择有效的测试模式\r\n");
	}
	
	while(1);
}
