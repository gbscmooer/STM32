#include "wifi_function.h"
#include "wifi_config.h"
#include "SysTick.h"
#include "usart.h"
#include "../cJSON/cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

// 全局变量控制日志输出
static bool g_wifi_log_enable = true;

// 设置日志输出状态
void WiFi_Set_Log(bool enable)
{
	g_wifi_log_enable = enable;
}

// 内部日志宏，受g_wifi_log_enable控制
#define WIFI_LOG(...) if(g_wifi_log_enable) PC_Usart(__VA_ARGS__)

/*
 * 函数名：ESP8266_Choose
 * 描述  ：使能/禁用WF-ESP8266模块
 * 输入  ：enumChoose = ENABLE，使能模块
 *         enumChoose = DISABLE，禁用模块
 * 返回  : 无
 * 调用  ：被外部调用
 */
void ESP8266_Choose ( FunctionalState enumChoose )
{
	if ( enumChoose == ENABLE )
		ESP8266_CH_HIGH_LEVEL();
	
	else
		ESP8266_CH_LOW_LEVEL();
	
}


/*
 * 函数名：ESP8266_Rst
 * 描述  ：重启WF-ESP8266模块
 * 输入  ：无
 * 返回  : 无
 * 调用  ：被ESP8266_AT_Test调用
 */
void ESP8266_Rst ( void )
{
	#if 0
	 ESP8266_Cmd ( "AT+RST", "OK", "ready", 2500 );   	
	
	#else
	 ESP8266_RST_LOW_LEVEL();
	 delay_ms ( 500 ); 
	 ESP8266_RST_HIGH_LEVEL();
	 
	#endif

}


/*
 * 函数名：ESP8266_Auto_BaudRate_Test
 * 描述  ：ESP8266模块波特率自动检测
 * 输入  ：无
 * 返回  : 无
 * 调用  ：被外部调用
 */
void ESP8266_Auto_BaudRate_Test ( void )
{
	u32 baud_rates[] = {115200, 9600, 57600, 38400, 19200, 4800};
	u8 i;
	USART_InitTypeDef USART_InitStructure;
	
	PC_Usart("\r\n开始ESP8266波特率自动检测...\r\n");
	
	for(i = 0; i < 6; i++)
	{
		PC_Usart("尝试波特率: %d\r\n", baud_rates[i]);
		
		// 重新配置USART3波特率
		USART_Cmd(USART3, DISABLE);
		USART_InitStructure.USART_BaudRate = baud_rates[i];
		USART_InitStructure.USART_WordLength = USART_WordLength_8b;
		USART_InitStructure.USART_StopBits = USART_StopBits_1;
		USART_InitStructure.USART_Parity = USART_Parity_No;
		USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
		USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
		USART_Init(USART3, &USART_InitStructure);
		USART_Cmd(USART3, ENABLE);
		
		delay_ms(100);
		
		// 测试AT指令
		if( ESP8266_Cmd ( "AT", "OK", NULL, 1000 ) )
		{
			PC_Usart("找到正确波特率: %d\r\n", baud_rates[i]);
			return;
		}
		
		delay_ms(500);
	}
	
	PC_Usart("未找到匹配的波特率，使用默认115200\r\n");
	// 设置回默认波特率115200
	USART_Cmd(USART3, DISABLE);
	USART_InitStructure.USART_BaudRate = 115200;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART3, &USART_InitStructure);
	USART_Cmd(USART3, ENABLE);
}


/*
 * 函数名：ESP8266_AT_Test
 * 描述  ：对WF-ESP8266模块进行AT测试启动
 * 输入  ：无
 * 返回  : 无
 * 调用  ：被外部调用
 */
void ESP8266_AT_Test ( void )
{
	u8 retry_count = 0;
	
	ESP8266_RST_HIGH_LEVEL();
	
	delay_ms ( 1000 ); 
	
	PC_Usart("\r\n开始ESP8266 AT测试...\r\n");
	
	while ( ! ESP8266_Cmd ( "AT", "OK", NULL, 1000 ) ) 
	{
		retry_count++;
		PC_Usart("AT测试失败，重试第%d次...\r\n", retry_count);
		ESP8266_Rst ();
		delay_ms(2000);  // 增加重启后的延时
		
		if(retry_count >= 5)  // 限制重试次数
		{
			PC_Usart("ESP8266模块无响应，请检查硬件连接和波特率设置！\r\n");
			break;
		}
	}
	
	if(retry_count < 5)
		PC_Usart("ESP8266 AT测试成功！\r\n");

}


/*
 * 函数名：ESP8266_Cmd
 * 描述  ：对WF-ESP8266模块发送AT指令
 * 输入  ：cmd，待发送的指令
 *         reply1，reply2，期待的响应，为NULL表不需响应，两者为或逻辑关系
 *         waittime，等待响应的时间
 * 返回  : 1，指令发送成功
 *         0，指令发送失败
 * 调用  ：被外部调用
 */
bool ESP8266_Cmd ( char * cmd, char * reply1, char * reply2, u32 waittime )
{    
	u32 start_time = 0;
	bool found = false;
	u32 elapsed_time = 0;
	u16 last_length = 0;
	u8 stable_count = 0;
	
	// 命令前短暂延迟，避免指令过密
	delay_ms(300);
	
	// 清除接收缓冲区
	strEsp8266_Fram_Record .InfBit .FramLength = 0;               //从新开始接收新的数据包
	strEsp8266_Fram_Record .InfBit .FramFinishFlag = 0;           //清除接收完成标志
	memset(strEsp8266_Fram_Record .Data_RX_BUF, 0, RX_BUF_MAX_LEN); //清空接收缓冲区

	WIFI_LOG ("\r\n[发送AT指令]: %s\r\n", cmd);  // 显示发送的指令
	ESP8266_Usart ( "%s\r\n", cmd );

	if ( ( reply1 == 0 ) && ( reply2 == 0 ) )                      //不需要接收数据
		return true;
	
	// 改进：循环检查是否收到期望的响应
	start_time = 0;
	while(start_time < waittime)
	{
		delay_ms(50);  // 每50ms检查一次
		start_time += 50;
		
		// 临时添加结束符以便strstr查找
		if(strEsp8266_Fram_Record.InfBit.FramLength > 0)
		{
			strEsp8266_Fram_Record.Data_RX_BUF[strEsp8266_Fram_Record.InfBit.FramLength] = '\0';
			
			// 检查是否收到期望的响应
			if((reply1 && strstr(strEsp8266_Fram_Record.Data_RX_BUF, reply1)) || 
			   (reply2 && strstr(strEsp8266_Fram_Record.Data_RX_BUF, reply2)))
			{
				found = true;
				break;
			}
			
			// 检查是否busy
			if(strstr(strEsp8266_Fram_Record.Data_RX_BUF, "busy"))
			{
				WIFI_LOG ("[警告]: ESP8266模块忙碌，等待2秒后重试...\r\n");
				delay_ms(2000);
				// 清除缓冲区并重新发送
				strEsp8266_Fram_Record .InfBit .FramLength = 0;
				strEsp8266_Fram_Record .InfBit .FramFinishFlag = 0;
				memset(strEsp8266_Fram_Record .Data_RX_BUF, 0, RX_BUF_MAX_LEN);
				
				ESP8266_Usart ( "%s\r\n", cmd );
				start_time = 0; // 重置计时器
				continue;
			}
		}
	}
	
	strEsp8266_Fram_Record .Data_RX_BUF [ strEsp8266_Fram_Record .InfBit .FramLength ]  = '\0';

	// 特殊处理CWLAP命令的输出，提取英文WiFi名称
	if(strstr(cmd, "CWLAP")) 
	{
		char *ptr;
		char *ssid_start;
		char *ssid_end;
		char *c;
		bool is_ascii;
		u8 wifi_count = 0;
		
		// 先显示原始数据长度用于调试
		WIFI_LOG ("[接收数据长度]: %d字节\r\n", strEsp8266_Fram_Record .InfBit .FramLength);
		
		WIFI_LOG ("[扫描到的WiFi网络]:\r\n");
		// 查找并显示英文WiFi名称
		ptr = strEsp8266_Fram_Record .Data_RX_BUF;
		while((ptr = strstr(ptr, "+CWLAP:")) != NULL)
		{
			ssid_start = strchr(ptr, '"');
			if(ssid_start)
			{
				ssid_start++;
				ssid_end = strchr(ssid_start, '"');
				if(ssid_end)
				{
					// 检查是否为英文/数字WiFi名称
					is_ascii = true;
					for(c = ssid_start; c < ssid_end; c++)
					{
						if(*c < 32 || *c > 126)  // 非ASCII字符
						{
							is_ascii = false;
							break;
						}
					}
					
					if(is_ascii && (ssid_end - ssid_start) > 0)
					{
						*ssid_end = '\0';
						WIFI_LOG ("  %d. %s\r\n", ++wifi_count, ssid_start);
						*ssid_end = '"';
					}
				}
			}
			ptr++;
		}
		if(wifi_count == 0)
		{
			WIFI_LOG ("  未发现英文名称的WiFi网络\r\n");
			WIFI_LOG ("  (可能都是中文名称或扫描超时)\r\n");
		}
	}
	else
	{
		WIFI_LOG ("[ESP8266回复]: %s\r\n", strEsp8266_Fram_Record .Data_RX_BUF);
	}
  
	return found;
	
}


/*
 * 函数名：ESP8266_Net_Mode_Choose
 * 描述  ：选择WF-ESP8266模块的工作模式
 * 输入  ：enumMode，工作模式
 * 返回  : 1，选择成功
 *         0，选择失败
 * 调用  ：被外部调用
 */
bool ESP8266_Net_Mode_Choose ( ENUM_Net_ModeTypeDef enumMode )
{
	switch ( enumMode )
	{
		case STA:
			return ESP8266_Cmd ( "AT+CWMODE=1", "OK", "no change", 2500 ); 
		
	  case AP:
		  return ESP8266_Cmd ( "AT+CWMODE=2", "OK", "no change", 2500 ); 
		
		case STA_AP:
		  return ESP8266_Cmd ( "AT+CWMODE=3", "OK", "no change", 2500 ); 
		
	  default:
		  return false;
  }
	
}

/*
 * 函数名：ESP8266_Scan_AP
 * 描述  ：扫描周围的WiFi网络
 * 输入  ：无
 * 返回  : 无
 * 调用  ：被外部调用
 */
void ESP8266_Scan_AP ( void )
{
	PC_Usart("开始扫描WiFi网络（预计需要10-15秒）...\r\n");
	delay_ms(3000);  // 扫描前等待3秒，让ESP8266完全就绪
	ESP8266_Cmd ( "AT+CWLAP", "OK", NULL, 15000 );  // 等待15秒扫描完成
}

/*
 * 函数名：ESP8266_JoinAP
 * 描述  ：WF-ESP8266模块连接外部WiFi
 * 输入  ：pSSID，WiFi名称字符串
 *       ：pPassWord，WiFi密码字符串
 * 返回  : 1，连接成功
 *         0，连接失败
 * 调用  ：被外部调用
 */
bool ESP8266_JoinAP ( char * pSSID, char * pPassWord )
{
	char cCmd [120];
	bool result;

	sprintf ( cCmd, "AT+CWJAP=\"%s\",\"%s\"", pSSID, pPassWord );
	
	PC_Usart("\r\n正在连接WiFi: %s...\r\n", pSSID);
	
	// 连接前额外延迟，确保模块空闲
	delay_ms(500);
	
	// 增加等待时间到10秒，因为WiFi连接可能需要更长时间
	result = ESP8266_Cmd ( cCmd, "OK", "WIFI GOT IP", 10000 );
	
	if(result) 
	{
		PC_Usart("? WiFi连接成功！\r\n");
		// 查看获得的IP地址
		delay_ms(1000);
		ESP8266_Cmd ( "AT+CIFSR", "OK", NULL, 2000 );
	}
	else
	{
		PC_Usart("? WiFi连接失败！\r\n");
		PC_Usart("可能的原因：\r\n");
		PC_Usart("1. WiFi密码错误\r\n");
		PC_Usart("2. WiFi名称包含特殊字符\r\n");
		PC_Usart("3. 信号强度不够\r\n");
		PC_Usart("4. 频段不支持(ESP8266只支持2.4GHz)\r\n");
	}
	
	return result;
	
}


/*
 * 函数名：ESP8266_BuildAP
 * 描述  ：WF-ESP8266模块创建WiFi热点
 * 输入  ：pSSID，WiFi名称字符串
 *       ：pPassWord，WiFi密码字符串
 *       ：enunPsdMode，WiFi加密方式代号字符串
 * 返回  : 1，创建成功
 *         0，创建失败
 * 调用  ：被外部调用
 */
bool ESP8266_BuildAP ( char * pSSID, char * pPassWord, char * enunPsdMode )
{
	char cCmd [120];

	sprintf ( cCmd, "AT+CWSAP=\"%s\",\"%s\",1,%s", pSSID, pPassWord, enunPsdMode );
	
	return ESP8266_Cmd ( cCmd, "OK", 0, 1000 );
	
}


/*
 * 函数名：ESP8266_Enable_MultipleId
 * 描述  ：WF-ESP8266模块启动多连接
 * 输入  ：enumEnUnvarnishTx，配置是否多连接
 * 返回  : 1，配置成功
 *         0，配置失败
 * 调用  ：被外部调用
 */
bool ESP8266_Enable_MultipleId ( FunctionalState enumEnUnvarnishTx )
{
	char cStr [20];
	
	sprintf ( cStr, "AT+CIPMUX=%d", ( enumEnUnvarnishTx ? 1 : 0 ) );
	
	return ESP8266_Cmd ( cStr, "OK", 0, 500 );
	
}


/*
 * 函数名：ESP8266_Link_Server
 * 描述  ：WF-ESP8266模块连接外部服务器
 * 输入  ：enumE，网络协议
 *       ：ip，服务器IP字符串
 *       ：ComNum，服务器端口字符串
 *       ：id，模块连接服务器的ID
 * 返回  : 1，连接成功
 *         0，连接失败
 * 调用  ：被外部调用
 */
bool ESP8266_Link_Server ( ENUM_NetPro_TypeDef enumE, char * ip, char * ComNum, ENUM_ID_NO_TypeDef id)
{
	char cStr [100] = { 0 }, cCmd [120];
	bool result;

  switch (  enumE )
  {
		case enumTCP:
		  sprintf ( cStr, "\"%s\",\"%s\",%s", "TCP", ip, ComNum );
		  break;
		
		case enumUDP:
		  sprintf ( cStr, "\"%s\",\"%s\",%s", "UDP", ip, ComNum );
		  break;
		
		default:
			break;
  }

  if ( id < 5 )
    sprintf ( cCmd, "AT+CIPSTART=%d,%s", id, cStr);
  else
	  sprintf ( cCmd, "AT+CIPSTART=%s", cStr );

	PC_Usart("\r\n正在连接服务器 %s:%s...\r\n", ip, ComNum);

	// 增加等待时间到10秒，并等待正确的响应
	result = ESP8266_Cmd ( cCmd, "CONNECT", "OK", 10000 );
	
	if(result) 
	{
		PC_Usart("? TCP连接成功！\r\n");
	}
	else
	{
		PC_Usart("? TCP连接失败！\r\n");
		PC_Usart("可能的原因：\r\n");
		PC_Usart("1. 服务器IP地址不正确\r\n");
		PC_Usart("2. 服务器端口未开放\r\n");
		PC_Usart("3. 防火墙阻止连接\r\n");
		PC_Usart("4. 网络不通\r\n");
		PC_Usart("5. 服务器未启动\r\n");
	}
	
	return result;
	
}


/*
 * 函数名：ESP8266_StartOrShutServer
 * 描述  ：WF-ESP8266模块开启或关闭服务器模式
 * 输入  ：enumMode，开启/关闭
 *       ：pPortNum，服务器端口号字符串
 *       ：pTimeOver，服务器超时时间字符串，单位：秒
 * 返回  : 1，操作成功
 *         0，操作失败
 * 调用  ：被外部调用
 */
bool ESP8266_StartOrShutServer ( FunctionalState enumMode, char * pPortNum, char * pTimeOver )
{
	char cCmd1 [120], cCmd2 [120];

	if ( enumMode )
	{
		sprintf ( cCmd1, "AT+CIPSERVER=%d,%s", 1, pPortNum );
		
		sprintf ( cCmd2, "AT+CIPSTO=%s", pTimeOver );

		return ( ESP8266_Cmd ( cCmd1, "OK", 0, 500 ) &&
						 ESP8266_Cmd ( cCmd2, "OK", 0, 500 ) );
	}
	
	else
	{
		sprintf ( cCmd1, "AT+CIPSERVER=%d,%s", 0, pPortNum );

		return ESP8266_Cmd ( cCmd1, "OK", 0, 500 );
	}
	
}


/*
 * 函数名：ESP8266_UnvarnishSend
 * 描述  ：配置WF-ESP8266模块进入透传发送
 * 输入  ：无
 * 返回  : 1，配置成功
 *         0，配置失败
 * 调用  ：被外部调用
 */
bool ESP8266_UnvarnishSend ( void )
{
	return (
	  ESP8266_Cmd ( "AT+CIPMODE=1", "OK", 0, 500 ) &&
	  ESP8266_Cmd ( "AT+CIPSEND", "\r\n", ">", 500 ) );
	
}


/*
 * 函数名：ESP8266_SendString
 * 描述  ：WF-ESP8266模块发送字符串
 * 输入  ：enumEnUnvarnishTx，声明是否已使能了透传模式
 *       ：pStr，要发送的字符串
 *       ：ulStrLength，要发送的字符串的字节数
 *       ：ucId，哪个ID发送的字符串
 * 返回  : 1，发送成功
 *         0，发送失败
 * 调用  ：被外部调用
 */
bool ESP8266_SendString ( FunctionalState enumEnUnvarnishTx, char * pStr, u32 ulStrLength, ENUM_ID_NO_TypeDef ucId )
{
	char cStr [20];
	bool bRet = false;
		
	if ( enumEnUnvarnishTx )
		ESP8266_Usart ( "%s", pStr );

	
	else
	{
		if ( ucId < 5 )
			sprintf ( cStr, "AT+CIPSEND=%d,%d", ucId, ulStrLength + 2 );

		else
			sprintf ( cStr, "AT+CIPSEND=%d", ulStrLength + 2 );
		
		ESP8266_Cmd ( cStr, "> ", 0, 1000 );

		bRet = ESP8266_Cmd ( pStr, "SEND OK", 0, 1000 );
  }
	
	return bRet;

}


/*
 * 函数名：ESP8266_ReceiveString
 * 描述  ：WF-ESP8266模块接收字符串
 * 输入  ：enumEnUnvarnishTx，声明是否已使能了透传模式
 * 返回  : 接收到的字符串首地址
 * 调用  ：被外部调用
 */
char * ESP8266_ReceiveString ( FunctionalState enumEnUnvarnishTx )
{
	char * pRecStr = 0;
	
	strEsp8266_Fram_Record .InfBit .FramLength = 0;
	strEsp8266_Fram_Record .InfBit .FramFinishFlag = 0;
	while ( ! strEsp8266_Fram_Record .InfBit .FramFinishFlag );
	strEsp8266_Fram_Record .Data_RX_BUF [ strEsp8266_Fram_Record .InfBit .FramLength ] = '\0';
	
	if ( enumEnUnvarnishTx )
	{
		if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, ">" ) )
			pRecStr = strEsp8266_Fram_Record .Data_RX_BUF;

	}
	
	else 
	{
		if ( strstr ( strEsp8266_Fram_Record .Data_RX_BUF, "+IPD" ) )
			pRecStr = strEsp8266_Fram_Record .Data_RX_BUF;

	}

	return pRecStr;
	
}


/*
 * 函数名：ESP8266_STA_TCP_Client
 * 描述  ：PZ-ESP8266模块进行STA TCP Clien测试
 * 输入  ：无
 * 返回  : 无
 * 调用  ：被外部调用
 */
void ESP8266_STA_TCP_Client ( void )
{
	char cStrInput [100] = { 0 }, * pStrDelimiter [2], * pBuf, * pStr;
	u8 uc = 0;
  u32 ul = 0;

	ESP8266_Choose ( ENABLE );	

  ESP8266_AT_Test ();
	
	ESP8266_Net_Mode_Choose ( STA );
  
	ESP8266_Cmd ( "AT+CWLAP", "OK", 0, 5000 );
		
  do
	{
		PC_Usart ( "\r\n请输入要连接的WiFi名称和密钥，输入格式为：名称字符+英文逗号+密钥字符+空格，点击发送\r\n" );

		scanf ( "%s", cStrInput );

		PC_Usart ( "\r\n稍等片刻 ……\r\n" );

		pBuf = cStrInput;
		uc = 0;
		while ( ( pStr = strtok ( pBuf, "," ) ) != NULL )
		{
			pStrDelimiter [ uc ++ ] = pStr;
			pBuf = NULL;
		} 
		
  } while ( ! ESP8266_JoinAP ( pStrDelimiter [0], pStrDelimiter [1] ) );
	
	ESP8266_Enable_MultipleId ( ENABLE );
	
	do 
	{
		PC_Usart ( "\r\n请在电脑上将网络调试助手以TCP Server连接网络，并输入电脑的IP和端口号，输入格式为：电脑IP+英文逗号+端口号+空格，点击发送\r\n" );

		scanf ( "%s", cStrInput );

		PC_Usart ( "\r\n稍等片刻 ……\r\n" );

		pBuf = cStrInput;
		uc = 0;
		while ( ( pStr = strtok ( pBuf, "," ) ) != NULL )
		{
			pStrDelimiter [ uc ++ ] = pStr;
			pBuf = NULL;
		} 
		
  } while ( ! ( ESP8266_Link_Server ( enumTCP, pStrDelimiter [0], pStrDelimiter [1], Multiple_ID_0 ) &&
	              ESP8266_Link_Server ( enumTCP, pStrDelimiter [0], pStrDelimiter [1], Multiple_ID_1 ) &&
	              ESP8266_Link_Server ( enumTCP, pStrDelimiter [0], pStrDelimiter [1], Multiple_ID_2 ) &&
	              ESP8266_Link_Server ( enumTCP, pStrDelimiter [0], pStrDelimiter [1], Multiple_ID_3 ) &&
	              ESP8266_Link_Server ( enumTCP, pStrDelimiter [0], pStrDelimiter [1], Multiple_ID_4 ) ) );

  for ( uc = 0; uc < 5; uc ++ )
	{
		PC_Usart ( "\r\n请输入端口ID%d要发送的字符串，输入格式为：字符串（不含空格）+空格，点击发送\r\n", uc );

		scanf ( "%s", cStrInput );

		ul = strlen ( cStrInput );
		
		ESP8266_SendString ( DISABLE, cStrInput, ul, ( ENUM_ID_NO_TypeDef ) uc );
		
	}
	
	
	PC_Usart ( "\r\n请在网络调试助手发送字符串\r\n" );
	while (1)
	{
	  pStr = ESP8266_ReceiveString ( DISABLE );
		PC_Usart ( "%s", pStr );
	}

}


/*
 * 函数名：ESP8266_AP_TCP_Server
 * 描述  ：PZ-ESP8266模块进行AP TCP Server测试
 * 输入  ：无
 * 返回  : 无
 * 调用  ：被外部调用
 */
void ESP8266_AP_TCP_Server ( void )
{
	char cStrInput [100] = { 0 }, * pStrDelimiter [3], * pBuf, * pStr;
	u8 uc = 0;
  u32 ul = 0;

  ESP8266_Choose ( ENABLE );

	ESP8266_AT_Test ();
	
	ESP8266_Net_Mode_Choose ( AP );


	PC_Usart ( "\r\n请输入要创建的WiFi的名称、加密方式和密钥，加密方式的编号为：\
              \r\n0 = OPEN\
              \r\n1 = WEP\
              \r\n2 = WPA_PSK\
	            \r\n3 = WPA2_PSK\
              \r\n4 = WPA_WPA2_PSK\
							\r\n输入格式为：名称字符+英文逗号+加密方式编号+英文逗号+密钥字符+空格，点击发送\r\n" );

	scanf ( "%s", cStrInput );

	PC_Usart ( "\r\n稍等片刻 ……\r\n" );

	pBuf = cStrInput;
	uc = 0;
	while ( ( pStr = strtok ( pBuf, "," ) ) != NULL )
	{
		pStrDelimiter [ uc ++ ] = pStr;
		pBuf = NULL;
	} 
	
	ESP8266_BuildAP ( pStrDelimiter [0], pStrDelimiter [2], pStrDelimiter [1] );
	ESP8266_Cmd ( "AT+RST", "OK", "ready", 2500 ); //*
		

	ESP8266_Enable_MultipleId ( ENABLE );
		
	
	PC_Usart ( "\r\n请输入服务器要开启的端口号和超时时间（0~28800，单位：秒），输入格式为：端口号字符+英文逗号+超时时间字符+空格，点击发送\r\n" );

	scanf ( "%s", cStrInput );

	PC_Usart ( "\r\n稍等片刻 ……\r\n" );

	pBuf = cStrInput;
	uc = 0;
	while ( ( pStr = strtok ( pBuf, "," ) ) != NULL )
	{
		pStrDelimiter [ uc ++ ] = pStr;
		pBuf = NULL;
	} 

	ESP8266_StartOrShutServer ( ENABLE, pStrDelimiter [0], pStrDelimiter [1] );
	
	
	do
	{
		PC_Usart ( "\r\n正查询本模块IP……\r\n" );
	  ESP8266_Cmd ( "AT+CIFSR", "OK", "Link", 500 );
		
		PC_Usart ( "\r\n请用手机连接刚才创建的WiFi，这里只连接一个手机，作为ID0，然后用手机网络调试助手以TCP Client连接刚才开启的服务器（AP IP）……\r\n" );
		delay_ms ( 20000 ) ;
	}	while ( ! ESP8266_Cmd ( "AT+CIPSTATUS", "+CIPSTATUS:0", 0, 500 ) );
	

	PC_Usart ( "\r\n请输入要向端口手机（ID0）发送的字符串，输入格式为：字符串（不含空格）+空格，点击发送\r\n" );

	scanf ( "%s", cStrInput );

	ul = strlen ( cStrInput );
	
	ESP8266_SendString ( DISABLE, cStrInput, ul, Multiple_ID_0 );

	
	PC_Usart ( "\r\n请在手机网络调试助手发送字符串\r\n" );
	while (1)
	{
	  pStr = ESP8266_ReceiveString ( DISABLE );
		PC_Usart ( "%s", pStr );
	}

}


/*
 * 函数名：ESP8266_StaTcpClient_ApTcpServer
 * 描述  ：PZ-ESP8266模块进行STA(TCP Client)+AP(TCP Server)测试
 * 输入  ：无
 * 返回  : 无
 * 调用  ：被外部调用
 */
void ESP8266_StaTcpClient_ApTcpServer ( void )
{
	char cStrInput [100] = { 0 }, * pStrDelimiter [3], * pBuf, * pStr;
	u8 uc = 0;
  u32 ul = 0;

  ESP8266_Choose ( ENABLE );

	ESP8266_AT_Test ();
	
	ESP8266_Net_Mode_Choose ( STA_AP );


	PC_Usart ( "\r\n请输入要创建的WiFi的名称、加密方式和密钥，加密方式的编号为：\
						\r\n0 = OPEN\
						\r\n1  =WEP\
						\r\n2 = WPA_PSK\
						\r\n3 = WPA2_PSK\
						\r\n4 = WPA_WPA2_PSK\
						\r\n输入格式为：名称字符+英文逗号+加密方式编号+英文逗号+密钥字符+空格，点击发送\r\n" );

	scanf ( "%s", cStrInput );

	PC_Usart ( "\r\n稍等片刻 ……\r\n" );

	pBuf = cStrInput;
	uc = 0;
	while ( ( pStr = strtok ( pBuf, "," ) ) != NULL )
	{
		pStrDelimiter [ uc ++ ] = pStr;
		pBuf = NULL;
	} 
	
	ESP8266_BuildAP ( pStrDelimiter [0], pStrDelimiter [2], pStrDelimiter [1] );
	ESP8266_Cmd ( "AT+RST", "OK", "ready", 2500 ); //*
	

	ESP8266_Cmd ( "AT+CWLAP", "OK", 0, 5000 );
		
  do
	{
		PC_Usart ( "\r\n请输入要连接的WiFi名称和密钥，输入格式为：名称字符+英文逗号+密钥字符+空格，点击发送\r\n" );

		scanf ( "%s", cStrInput );

		PC_Usart ( "\r\n稍等片刻 ……\r\n" );

		pBuf = cStrInput;
		uc = 0;
		while ( ( pStr = strtok ( pBuf, "," ) ) != NULL )
		{
			pStrDelimiter [ uc ++ ] = pStr;
			pBuf = NULL;
		} 
		
  } while ( ! ESP8266_JoinAP ( pStrDelimiter [0], pStrDelimiter [1] ) );

	
	ESP8266_Enable_MultipleId ( ENABLE );
		
	
	PC_Usart ( "\r\n请输入服务器要开启的端口号和超时时间（0~28800，单位：秒），输入格式为：端口号字符+英文逗号+超时时间字符+空格，点击发送\r\n" );

	scanf ( "%s", cStrInput );

	PC_Usart ( "\r\n稍等片刻 ……\r\n" );

	pBuf = cStrInput;
	uc = 0;
	while ( ( pStr = strtok ( pBuf, "," ) ) != NULL )
	{
		pStrDelimiter [ uc ++ ] = pStr;
		pBuf = NULL;
	} 

	ESP8266_StartOrShutServer ( ENABLE, pStrDelimiter [0], pStrDelimiter [1] );
	
	
	do 
	{
		PC_Usart ( "\r\n请在电脑上将网络调试助手以TCP Server连接网络，并输入电脑的IP和端口号，输入格式为：电脑IP+英文逗号+端口号+空格，点击发送\r\n" );

		scanf ( "%s", cStrInput );

		PC_Usart ( "\r\n稍等片刻 ……\r\n" );

		pBuf = cStrInput;
		uc = 0;
		while ( ( pStr = strtok ( pBuf, "," ) ) != NULL )
		{
			pStrDelimiter [ uc ++ ] = pStr;
			pBuf = NULL;
		} 
		
  } while ( ! ( ESP8266_Link_Server ( enumTCP, pStrDelimiter [0], pStrDelimiter [1], Multiple_ID_0 ) &&
	              ESP8266_Link_Server ( enumTCP, pStrDelimiter [0], pStrDelimiter [1], Multiple_ID_1 ) &&
	              ESP8266_Link_Server ( enumTCP, pStrDelimiter [0], pStrDelimiter [1], Multiple_ID_2 ) ) );
		
	
	do
	{
		PC_Usart ( "\r\n正查询本模块IP，前一个为AP IP，后一个为STA IP……\r\n" );
	  ESP8266_Cmd ( "AT+CIFSR", "OK", "Link", 500 );
		
		PC_Usart ( "\r\n请用手机连接刚才创建的WiFi，这里只连接一个手机，作为ID3，然后用手机网络调试助手以TCP Client连接刚才开启的服务器（AP IP）……\r\n" );
		delay_ms ( 20000 ) ;
	}	while ( ! ESP8266_Cmd ( "AT+CIPSTATUS", "+CIPSTATUS:3", 0, 500 ) );
	

	for ( uc = 0; uc < 3; uc ++ )
	{
		PC_Usart ( "\r\n请输入端口ID%d要发送的字符串，输入格式为：字符串（不含空格）+空格，点击发送\r\n", uc );

		scanf ( "%s", cStrInput );

		ul = strlen ( cStrInput );
		
		ESP8266_SendString ( DISABLE, cStrInput, ul, ( ENUM_ID_NO_TypeDef ) uc );
		
	}
	
	
	PC_Usart ( "\r\n请输入要向端口手机（ID3）发送的字符串，输入格式为：字符串（不含空格）+空格，点击发送\r\n" );

	scanf ( "%s", cStrInput );

	ul = strlen ( cStrInput );
	
	ESP8266_SendString ( DISABLE, cStrInput, ul, Multiple_ID_3 );

	
	PC_Usart ( "\r\n请在电脑网络调试助手或手机网络调试助手发送字符串……\r\n" );
	while (1)
	{
	  pStr = ESP8266_ReceiveString ( DISABLE );
		PC_Usart ( "%s", pStr );
	}
	
}

/**
  * @brief  WiFi_Connect_Default 连接默认WiFi网络
  * @param  无
  * @retval true：连接成功，false：连接失败
  */
bool WiFi_Connect_Default(void)
{
	PC_Usart("\r\n=== 尝试连接默认WiFi网络 ===\r\n");
	
	if(ESP8266_JoinAP("OPPOx", "huangzixuan"))
	{
		PC_Usart(" 默认WiFi连接成功！\r\n");
		return true;
	}
	else
	{
		PC_Usart(" 默认WiFi连接失败！\r\n");
		return false;
	}
}

/**
  * @brief  WiFi_Connect_Menu WiFi连接失败后的菜单处理
  * @param  无
  * @retval true：最终连接成功，false：用户放弃连接
  */
bool WiFi_Connect_Menu(void)
{
	char choice;
	
	while(1)
	{
		PC_Usart("\r\n=== WiFi连接失败处理菜单 ===\r\n");
		PC_Usart("1. 重新连接默认WiFi\r\n");
		PC_Usart("2. 扫描并连接其他WiFi\r\n");
		PC_Usart("请输入选项(1或2): ");
		
		choice = USART1_GetChar();
		PC_Usart("%c\r\n", choice);
		
		if(choice == '1')
		{
			if(WiFi_Connect_Default())
			{
				return true;
			}
		}
		else if(choice == '2')
		{
			PC_Usart("\r\n=== 扫描可用WiFi网络 ===\r\n");
			ESP8266_Cmd("AT+CWLAP", "OK", NULL, 15000);  // 增加到15秒
			
			PC_Usart("\r\n请输入要连接的WiFi信息\r\n");
			PC_Usart("格式: WiFi名称,密码\r\n");
			PC_Usart("例如: TP-LINK_5G,12345678\r\n");
			PC_Usart("请输入(以回车结束): ");
			
			char input[100] = {0};
			char *pStrDelimiter[2];
			char *pBuf, *pStr;
			u8 uc = 0;
			u8 idx = 0;
			char ch;
			
			// 手动读取字符串直到回车
			while(idx < 99)
			{
				while(USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET);
				ch = USART_ReceiveData(USART1);
				
				if(ch == '\r' || ch == '\n')
				{
					if(idx > 0) break;  // 至少有一个字符才结束
					else continue;  // 忽略开头的空白
				}
				
				input[idx++] = ch;
				// 回显
				USART_SendData(USART1, ch);
				while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
			}
			input[idx] = '\0';
			PC_Usart("\r\n接收到: %s\r\n", input);
			
			pBuf = input;
			uc = 0;
			while ((pStr = strtok(pBuf, ",")) != NULL) {
				pStrDelimiter[uc++] = pStr;
				pBuf = NULL;
			}
			
			if(uc < 2) {
				pBuf = input;
				uc = 0;
				while ((pStr = strtok(pBuf, ":")) != NULL) {
					pStrDelimiter[uc++] = pStr;
					pBuf = NULL;
				}
			}
			
			if(uc >= 2 && pStrDelimiter[0] != NULL && pStrDelimiter[1] != NULL)
			{
				if(ESP8266_JoinAP(pStrDelimiter[0], pStrDelimiter[1]))
				{
					return true;
				}
			}
			else
			{
				PC_Usart("? 输入格式错误！\r\n");
			}
		}
		else
		{
			PC_Usart("? 无效选项，请重新选择\r\n");
		}
	}
}

/**
  * @brief  WiFi_Ping_Test 执行Ping测试
  * @param  target：目标地址字符串
  * @retval true：Ping成功，false：Ping失败
  */
bool WiFi_Ping_Test(char *target)
{
	char cmd[100];
	
	PC_Usart("\r\n正在Ping %s...\r\n", target);
	sprintf(cmd, "AT+PING=\"%s\"", target);
	
	if(ESP8266_Cmd(cmd, "OK", "+", 5000))
	{
		PC_Usart("? Ping %s 成功！\r\n", target);
		return true;
	}
	else
	{
		PC_Usart("? Ping %s 失败！\r\n", target);
		return false;
	}
}

/**
  * @brief  ESP8266_TCP_Connect 连接TCP服务器
  * @param  ip：服务器IP地址或域名
  * @param  port：服务器端口号
  * @retval true：连接成功，false：连接失败
  */
bool ESP8266_TCP_Connect(char *ip, u16 port)
{
	char cmd[120];
	
	WIFI_LOG("\r\n正在连接TCP服务器 %s:%d...\r\n", ip, port);
	
	// 1. 尝试关闭可能存在的旧连接（容错处理，ERROR是正常的）
	WIFI_LOG("[步骤1] 清理旧连接...\r\n");
	ESP8266_Cmd("AT+CIPCLOSE", "OK", "ERROR", 1000);
	delay_ms(500);  // 等待ESP8266处理完毕
	
	// 2. 设置单连接模式
	WIFI_LOG("[步骤2] 设置单连接模式...\r\n");
	if(!ESP8266_Cmd("AT+CIPMUX=0", "OK", NULL, 2000))
	{
		WIFI_LOG("? 设置单连接模式失败\r\n");
		return false;
	}
	WIFI_LOG("? 单连接模式设置成功\r\n");
	delay_ms(500);  // 等待设置生效
	
	// 3. 连接TCP服务器
	WIFI_LOG("[步骤3] 发送TCP连接命令...\r\n");
	sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d", ip, port);
	
	// 等待CONNECT响应（不使用OK，因为有些固件只返回CONNECT）
	if(ESP8266_Cmd(cmd, "CONNECT", "OK", 10000))
	{
		WIFI_LOG("? TCP连接成功！\r\n");
		delay_ms(1000);  // 连接成功后等待，让ESP8266状态稳定，避免下一个命令太快
		return true;
	}
	else
	{
		WIFI_LOG("? TCP连接失败！\r\n");
		return false;
	}
}

/**
  * @brief  ESP8266_TCP_Send 发送TCP数据
  * @param  data：要发送的数据
  * @param  len：数据长度
  * @retval true：发送成功，false：发送失败
  */
bool ESP8266_TCP_Send(char *data, u16 len)
{
	char cmd[32];
	u8 retry = 0;
	u16 last_length = 0;
	u8 stable_count = 0;
	
	PC_Usart("\r\n准备发送TCP数据 (%d字节)...\r\n", len);
	
	// 1. 发送CIPSEND命令，等待>提示符
	sprintf(cmd, "AT+CIPSEND=%d", len);
	
	if(!ESP8266_Cmd(cmd, ">", NULL, 3000))
	{
		PC_Usart("? 未收到>提示符，发送准备失败\r\n");
		return false;
	}
	
	PC_Usart("? 收到>提示符\r\n");
	delay_ms(100);  // 等待ESP8266完全准备好接收数据
	
	// 2. 清空缓冲区准备接收SEND OK
	strEsp8266_Fram_Record.InfBit.FramLength = 0;
	strEsp8266_Fram_Record.InfBit.FramFinishFlag = 0;
	memset(strEsp8266_Fram_Record.Data_RX_BUF, 0, RX_BUF_MAX_LEN);
	
	// 3. 发送实际数据
	PC_Usart("正在发送数据...\r\n");
	ESP8266_Usart("%s", data);
	
	// 4. 等待发送完成确认（改进：循环检查而非固定延时）
	while(retry < 40)  // 最多等待2秒
	{
		delay_ms(50);
		
		// 实时检查是否收到SEND OK
		if(strEsp8266_Fram_Record.InfBit.FramLength > 0)
		{
			strEsp8266_Fram_Record.Data_RX_BUF[strEsp8266_Fram_Record.InfBit.FramLength] = '\0';
			if(strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "SEND OK"))
			{
				break; // 收到确认，提前结束等待
			}
		}
		
		// 检查数据长度稳定性
		if(strEsp8266_Fram_Record.InfBit.FramLength > 0)
		{
			if(strEsp8266_Fram_Record.InfBit.FramLength == last_length)
			{
				stable_count++;
				if(stable_count >= 4)  // 连续4次(200ms)长度不变
				{
					break;  // 接收完成
				}
			}
			else
			{
				stable_count = 0;
				last_length = strEsp8266_Fram_Record.InfBit.FramLength;
			}
		}
		retry++;
	}
	
	// 5. 检查是否收到SEND OK
	strEsp8266_Fram_Record.Data_RX_BUF[strEsp8266_Fram_Record.InfBit.FramLength] = '\0';
	if(strstr(strEsp8266_Fram_Record.Data_RX_BUF, "SEND OK"))
	{
		WIFI_LOG("? 数据发送成功 (SEND OK)\r\n");
		delay_ms(200);  // SEND OK后稍等，让ESP8266准备接收服务器响应
		return true;
	}
	else
	{
		WIFI_LOG("? 未收到SEND OK (收到: %s)\r\n", strEsp8266_Fram_Record.Data_RX_BUF);
		delay_ms(200);
		return true;  // 即使没收到也继续，因为数据可能已发送
	}
}

/**
  * @brief  ESP8266_TCP_Disconnect 断开TCP连接
  * @param  无
  * @retval true：断开成功，false：断开失败
  */
bool ESP8266_TCP_Disconnect(void)
{
	WIFI_LOG("\r\n正在关闭TCP连接...\r\n");
	
	if(ESP8266_Cmd("AT+CIPCLOSE", "OK", "CLOSED", 2000))
	{
		WIFI_LOG("? TCP连接已关闭\r\n");
		return true;
	}
	else
	{
		WIFI_LOG("? 关闭TCP连接失败\r\n");
		return false;
	}
}

/**
  * @brief  ESP8266_HTTP_Request 发送HTTP请求并接收响应
  * @param  host：服务器域名或IP
  * @param  port：服务器端口，通常80
  * @param  path：请求路径，例如"/api/weather"
  * @param  response：存储响应的缓冲区
  * @param  max_len：缓冲区最大长度
  * @retval true：请求成功，false：请求失败
  */
bool ESP8266_HTTP_Request(char *host, u16 port, char *path, char *response, u16 max_len)
{
	char http_request[512];
	u16 len;
	
	WIFI_LOG("\r\n=== 发送HTTP请求 ===\r\n");
	WIFI_LOG("目标: http://%s:%d%s\r\n", host, port, path);
	
	// 1. 连接TCP服务器
	if(!ESP8266_TCP_Connect(host, port))
	{
		return false;
	}
	
	// 2. 构造HTTP GET请求
	sprintf(http_request, 
		"GET %s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"User-Agent: ESP8266\r\n"
		"Connection: close\r\n"
		"\r\n",
		path, host);
	
	len = strlen(http_request);
	WIFI_LOG("发送HTTP请求 (%d字节):\r\n%s\r\n", len, http_request);
	
	// 3. 清空接收缓冲区准备接收HTTP响应
	strEsp8266_Fram_Record.InfBit.FramLength = 0;
	strEsp8266_Fram_Record.InfBit.FramFinishFlag = 0;
	memset(strEsp8266_Fram_Record.Data_RX_BUF, 0, RX_BUF_MAX_LEN);
	delay_ms(100);
	
	// 4. 发送HTTP请求
	if(!ESP8266_TCP_Send(http_request, len))
	{
		WIFI_LOG("? 发送HTTP请求失败\r\n");
		ESP8266_TCP_Disconnect();
		return false;
	}
	
	WIFI_LOG("? HTTP请求已发送\r\n");
	
	// 5. 等待并接收响应（改进：动态等待直到数据稳定或连接关闭）
	WIFI_LOG("等待服务器响应...\r\n");
	u8 retry = 0;
	bool got_data = false;
	u16 last_len = 0;
	u8 stable_cnt = 0;
	
	while(retry < 50)  // 最多等待10秒
	{
		delay_ms(200);
		
		// 确保字符串结束符
		if(strEsp8266_Fram_Record.InfBit.FramLength > 0)
		{
			strEsp8266_Fram_Record.Data_RX_BUF[strEsp8266_Fram_Record.InfBit.FramLength] = '\0';
		}
		
		// 如果还没收到数据
		if(!got_data)
		{
			if(strEsp8266_Fram_Record.InfBit.FramLength > 10)
			{
				// 检查是否包含+IPD或HTTP响应头
				if(strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "+IPD") ||
				   strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "HTTP/"))
				{
					WIFI_LOG("? 开始接收数据 (%d字节)...\r\n", strEsp8266_Fram_Record.InfBit.FramLength);
					got_data = true;
					last_len = strEsp8266_Fram_Record.InfBit.FramLength;
				}
			}
		}
		else
		{
			// 已收到数据，检查是否稳定
			if(strEsp8266_Fram_Record.InfBit.FramLength == last_len)
			{
				stable_cnt++;
				// 连续5次(1秒)长度不变，认为接收完成
				if(stable_cnt >= 5)
				{
					WIFI_LOG("? 数据接收稳定 (%d字节)\r\n", last_len);
					break;
				}
			}
			else
			{
				last_len = strEsp8266_Fram_Record.InfBit.FramLength;
				stable_cnt = 0;
				// WIFI_LOG("  接收中... (%d字节)\r\n", last_len);
			}
			
			// 检查是否包含 CLOSED (服务器主动关闭连接)
			if(strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "CLOSED"))
			{
				 WIFI_LOG("? 检测到连接关闭，接收结束\r\n");
				 break;
			}
		}
		
		retry++;
		if(retry % 5 == 0 && !got_data)
		{
			WIFI_LOG("等待中... %d秒\r\n", retry / 5);
		}
	}
	
	if(!got_data && retry >= 50)
	{
		WIFI_LOG("? 等待超时！\r\n");
		ESP8266_TCP_Disconnect();
		return false;
	}
	
	// 6. 处理响应数据（去除+IPD前缀和末尾的CLOSED）
	if(got_data && strEsp8266_Fram_Record.InfBit.FramLength > 0)
	{
		char *http_start = NULL;
		char *closed_ptr = NULL;
		
		// 确保结束符
		strEsp8266_Fram_Record.Data_RX_BUF[strEsp8266_Fram_Record.InfBit.FramLength] = '\0';
		
		// 去除末尾的 CLOSED
		closed_ptr = strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "CLOSED");
		if(closed_ptr)
		{
			*closed_ptr = '\0';
			// 更新长度（虽然下面计算http_len主要靠指针减法，但保持一致性）
			strEsp8266_Fram_Record.InfBit.FramLength = closed_ptr - (char*)strEsp8266_Fram_Record.Data_RX_BUF;
		}
		
		// 查找HTTP响应的开始位置（跳过+IPD前缀）
		http_start = strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "HTTP/");
		if(http_start == NULL)
		{
			// 如果没找到HTTP/，查找{（JSON直接开始的情况）
			http_start = strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "{");
		}
		
		if(http_start == NULL)
		{
			// 没找到HTTP响应头，直接复制所有数据
			WIFI_LOG("? 未找到HTTP响应头，使用原始数据\r\n");
			http_start = (char*)strEsp8266_Fram_Record.Data_RX_BUF;
		}
		else
		{
			WIFI_LOG("? 找到HTTP响应，偏移量: %d字节\r\n", http_start - (char*)strEsp8266_Fram_Record.Data_RX_BUF);
		}
		
		// 计算实际HTTP数据长度
		u16 http_len = strlen(http_start); // 使用strlen因为我们已经处理了CLOSED并添加了\0
		u16 copy_len = http_len;
		if(copy_len > max_len - 1)
		{
			copy_len = max_len - 1;
			WIFI_LOG("? 响应数据过大，截断至 %d 字节\r\n", copy_len);
		}
		
		memcpy(response, http_start, copy_len);
		response[copy_len] = '\0';
		
		WIFI_LOG("? 接收到HTTP响应 (%d字节)\r\n", copy_len);
		// 只显示前200字节，避免刷屏
		if(copy_len > 200)
		{
			char preview[201];
			memcpy(preview, response, 200);
			preview[200] = '\0';
			WIFI_LOG("响应预览(前200字节):\r\n%s...\r\n", preview);
		}
		else
		{
			WIFI_LOG("完整响应:\r\n%s\r\n", response);
		}
		
		// 检查是否已经关闭连接（通过检查是否包含CLOSED）
		// 注意：这里我们检查的是原始接收缓冲区，因为response已经被截断了
		// 但我们在上面已经处理了CLOSED并截断了长度，所以如果closed_ptr不为空，说明已关闭
		if(closed_ptr != NULL)
		{
			WIFI_LOG("? 连接已由服务器关闭，无需发送CIPCLOSE\r\n");
		}
		else
		{
			// 再次检查缓冲区中是否包含CLOSED，以防万一
			if(strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "CLOSED"))
			{
				WIFI_LOG("? 检测到CLOSED，无需发送CIPCLOSE\r\n");
			}
			else
			{
				ESP8266_TCP_Disconnect();
			}
		}
		
		return true;
	}
	else
	{
		WIFI_LOG("? 未接收到响应\r\n");
		ESP8266_TCP_Disconnect();
		return false;
	}
}

// 定义全局缓冲区，避免栈溢出
static char g_weather_http_response[2048];

/**
  * @brief  Weather_Get_Data 获取天气数据
  * @param  weather：存储天气数据的结构体指针
  * @retval true：获取成功，false：获取失败
  */
bool Weather_Get_Data(WeatherData *weather)
{
	// 使用全局缓冲区代替栈变量
	// char http_response[2048]; 
	char *json_start;
	cJSON *root = NULL;
	cJSON *data = NULL;
	cJSON *item = NULL;
	bool result = false;
	
	// 初始化天气数据
	weather->valid = false;
	memset(weather->location, 0, sizeof(weather->location));
	memset(weather->temperature, 0, sizeof(weather->temperature));
	memset(weather->humidity, 0, sizeof(weather->humidity));
	memset(weather->weather, 0, sizeof(weather->weather));
	memset(weather->description, 0, sizeof(weather->description));
	memset(weather->last_update, 0, sizeof(weather->last_update));
	
	WIFI_LOG("\r\n=== 获取天气数据 ===\r\n");
	
	// 直接使用IP地址，避免域名解析延迟
	// 服务器IP: 47.83.221.161
	if(!ESP8266_HTTP_Request("47.83.221.161", 80, "/api/weather/", g_weather_http_response, sizeof(g_weather_http_response)))
	{
		WIFI_LOG("? HTTP请求失败\r\n");
		return false;
	}
	
	// 查找JSON数据起始位置（跳过HTTP头）
	json_start = strstr(g_weather_http_response, "\r\n\r\n");
	if(json_start != NULL)
	{
		json_start += 4;  // 跳过 \r\n\r\n
		// 再次确认是否以 { 开头
		char *real_json_start = strchr(json_start, '{');
		if(real_json_start != NULL)
		{
			json_start = real_json_start;
		}
	}
	else
	{
		json_start = strstr(g_weather_http_response, "{");
	}
	
	if(json_start == NULL)
	{
		WIFI_LOG("? 未找到JSON数据\r\n");
		return false;
	}
	
	// 查找JSON结束位置（最后一个 }）
	char *json_end = strrchr(json_start, '}');
	if(json_end != NULL)
	{
		*(json_end + 1) = '\0'; // 截断后续垃圾数据
	}
	else
	{
		WIFI_LOG("? 未找到JSON结束符\r\n");
		// 尝试继续解析，也许只是缺少结束符
	}
	
	WIFI_LOG("解析JSON数据...\r\n");
	WIFI_LOG("待解析的JSON内容:\r\n%s\r\n", json_start); // 打印完整JSON内容以便调试
	
	// 解析JSON
	root = cJSON_Parse(json_start);
	if(root == NULL)
	{
		WIFI_LOG("? JSON解析失败\r\n");
		const char *error_ptr = cJSON_GetErrorPtr();
		if(error_ptr != NULL)
		{
			WIFI_LOG("错误位置: %s\r\n", error_ptr);
		}
		return false;
	}
	
	// 检查status字段
	item = cJSON_GetObjectItem(root, "status");
	if(item != NULL && cJSON_IsString(item))
	{
		if(strcmp(item->valuestring, "success") != 0)
		{
			// 服务器返回错误状态
			WIFI_LOG("? API返回错误状态: %s\r\n", item->valuestring);
			
			// 尝试获取错误消息
			cJSON *msg_item = cJSON_GetObjectItem(root, "msg");
			if(msg_item != NULL && cJSON_IsString(msg_item))
			{
				// 尝试检测特定的UTF-8错误信息 "上游服务器不可用"
				// 使用十六进制匹配以避免编码问题: \xE4\xB8\x8A\xE6\xB8\xB8 (上游)
				if(strstr(msg_item->valuestring, "\xE4\xB8\x8A\xE6\xB8\xB8") != NULL || 
				   strstr(msg_item->valuestring, "unavailable") != NULL)
				{
					WIFI_LOG("提示: 上游服务器不可用，请稍后重试\r\n");
				}
				else
				{
					// 其他错误信息，直接打印（可能包含UTF-8字符导致乱码）
					WIFI_LOG("错误信息(UTF-8): %s\r\n", msg_item->valuestring);
				}
			}
			
			cJSON_Delete(root);
			return false;
		}
	}
	else
	{
		WIFI_LOG("? 未找到status字段或格式错误\r\n");
		cJSON_Delete(root);
		return false;
	}
	
	// 获取data对象
	data = cJSON_GetObjectItem(root, "data");
	if(data == NULL)
	{
		WIFI_LOG("? 未找到data字段\r\n");
		cJSON_Delete(root);
		return false;
	}
	
	// 提取各个字段
	item = cJSON_GetObjectItem(data, "location");
	if(item != NULL && cJSON_IsString(item))
	{
		strncpy(weather->location, item->valuestring, sizeof(weather->location) - 1);
	}
	
	item = cJSON_GetObjectItem(data, "temperature");
	if(item != NULL && cJSON_IsString(item))
	{
		strncpy(weather->temperature, item->valuestring, sizeof(weather->temperature) - 1);
	}
	
	item = cJSON_GetObjectItem(data, "humidity");
	if(item != NULL && cJSON_IsString(item))
	{
		strncpy(weather->humidity, item->valuestring, sizeof(weather->humidity) - 1);
	}
	
	item = cJSON_GetObjectItem(data, "weather");
	if(item != NULL && cJSON_IsString(item))
	{
		strncpy(weather->weather, item->valuestring, sizeof(weather->weather) - 1);
	}
	
	item = cJSON_GetObjectItem(data, "description");
	if(item != NULL && cJSON_IsString(item))
	{
		strncpy(weather->description, item->valuestring, sizeof(weather->description) - 1);
	}
	
	item = cJSON_GetObjectItem(data, "last_update");
	if(item != NULL && cJSON_IsString(item))
	{
		strncpy(weather->last_update, item->valuestring, sizeof(weather->last_update) - 1);
	}
	
	weather->valid = true;
	result = true;
	
	WIFI_LOG("? 天气数据解析成功\r\n");
	
	// 释放JSON对象
	cJSON_Delete(root);
	
	return result;
}

/**
  * @brief  Weather_Display 显示天气数据
  * @param  weather：天气数据结构体指针
  * @retval 无
  */
void Weather_Display(WeatherData *weather)
{
	if(!weather->valid)
	{
		PC_Usart("\r\n[天气数据无效]\r\n");
		return;
	}
	
	PC_Usart("\r\n╔════════════════════════════════╗\r\n");
	PC_Usart("║      天气信息显示              ║\r\n");
	PC_Usart("╠════════════════════════════════╣\r\n");
	PC_Usart("║ 位置: %-24s ║\r\n", weather->location);
	PC_Usart("║ 温度: %-24s ║\r\n", weather->temperature);
	PC_Usart("║ 湿度: %-24s ║\r\n", weather->humidity);
	PC_Usart("║ 天气: %-24s ║\r\n", weather->weather);
	PC_Usart("║ 描述: %-24s ║\r\n", weather->description);
	PC_Usart("╠════════════════════════════════╣\r\n");
	PC_Usart("║ 更新: %-24s ║\r\n", weather->last_update);
	PC_Usart("╚════════════════════════════════╝\r\n");
}

/**
  * @brief  ESP8266_Simple_TCP_Test 简单TCP连接测试
  * @param  无
  * @retval 测试结果说明字符串
  * @note   用于诊断TCP连接和数据接收问题
  */
void ESP8266_Simple_TCP_Test(void)
{
	char cmd[64];
	
	PC_Usart("\r\n╔══════════════════════════════════════╗\r\n");
	PC_Usart("║    ESP8266 TCP连接诊断测试           ║\r\n");
	PC_Usart("╚══════════════════════════════════════╝\r\n");
	
	// 1. 关闭旧连接
	PC_Usart("\r\n[1] 关闭旧连接...\r\n");
	ESP8266_Cmd("AT+CIPCLOSE", "OK", "CLOSED", 1000);
	delay_ms(500);
	
	// 2. 设置单连接模式
	PC_Usart("\r\n[2] 设置单连接模式...\r\n");
	if(!ESP8266_Cmd("AT+CIPMUX=0", "OK", NULL, 1000))
	{
		PC_Usart("? 设置单连接模式失败\r\n");
		return;
	}
	delay_ms(500);
	
	// 3. 连接服务器
	PC_Usart("\r\n[3] 连接服务器 47.83.221.161:80...\r\n");
	sprintf(cmd, "AT+CIPSTART=\"TCP\",\"47.83.221.161\",80");
	if(!ESP8266_Cmd(cmd, "CONNECT", "OK", 10000))
	{
		PC_Usart("? TCP连接失败\r\n");
		return;
	}
	delay_ms(1000);
	
	// 4. 检查连接状态
	PC_Usart("\r\n[4] 检查连接状态...\r\n");
	ESP8266_Cmd("AT+CIPSTATUS", "OK", NULL, 1000);
	delay_ms(500);
	
	// 5. 构造简单HTTP请求
	PC_Usart("\r\n[5] 发送HTTP请求...\r\n");
	char http_req[] = "GET /api/weather/ HTTP/1.1\r\nHost: 47.83.221.161\r\nConnection: close\r\n\r\n";
	u16 req_len = strlen(http_req);
	
	sprintf(cmd, "AT+CIPSEND=%d", req_len);
	if(!ESP8266_Cmd(cmd, ">", NULL, 2000))
	{
		PC_Usart("? 进入发送模式失败\r\n");
		ESP8266_Cmd("AT+CIPCLOSE", "OK", NULL, 1000);
		return;
	}
	
	// 6. 发送数据
	PC_Usart("\r\n[6] 发送数据...\r\n");
	PC_Usart("请求内容:\r\n%s\r\n", http_req);
	
	// 清空缓冲区
	strEsp8266_Fram_Record.InfBit.FramLength = 0;
	memset(strEsp8266_Fram_Record.Data_RX_BUF, 0, RX_BUF_MAX_LEN);
	
	ESP8266_Usart("%s", http_req);
	delay_ms(500);
	
	// 7. 等待响应
	PC_Usart("\r\n[7] 等待响应 (10秒)...\r\n");
	u8 wait_count = 0;
	while(wait_count < 50)  // 50 x 200ms = 10秒
	{
		delay_ms(200);
		
		if(strEsp8266_Fram_Record.InfBit.FramLength > 0)
		{
			PC_Usart("\r\n? 收到数据！长度: %d 字节\r\n", strEsp8266_Fram_Record.InfBit.FramLength);
			PC_Usart("\r\n接收到的原始数据:\r\n");
			PC_Usart("─────────────────────────────────\r\n");
			
			// 显示前500字节
			if(strEsp8266_Fram_Record.InfBit.FramLength > 500)
			{
				char preview[501];
				memcpy(preview, strEsp8266_Fram_Record.Data_RX_BUF, 500);
				preview[500] = '\0';
				PC_Usart("%s\r\n...(数据过长，仅显示前500字节)\r\n", preview);
			}
			else
			{
				PC_Usart("%s\r\n", strEsp8266_Fram_Record.Data_RX_BUF);
			}
			PC_Usart("─────────────────────────────────\r\n");
			
			// 分析数据格式
			PC_Usart("\r\n[数据格式分析]\r\n");
			if(strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "+IPD"))
			{
				PC_Usart("? 包含 +IPD 前缀\r\n");
			}
			if(strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "HTTP/"))
			{
				PC_Usart("? 包含 HTTP 响应头\r\n");
			}
			if(strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "{"))
			{
				PC_Usart("? 包含 JSON 数据\r\n");
			}
			if(strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "SEND OK"))
			{
				PC_Usart("? 包含 SEND OK 确认\r\n");
			}
			
			break;
		}
		
		wait_count++;
		if(wait_count % 5 == 0)
		{
			PC_Usart("  等待... %d秒 (已收到%d字节)\r\n", 
				wait_count / 5 * 2, 
				strEsp8266_Fram_Record.InfBit.FramLength);
		}
	}
	
	if(wait_count >= 50)
	{
		PC_Usart("\r\n? 等待超时，未收到响应\r\n");
		PC_Usart("可能原因:\r\n");
		PC_Usart("  1. 服务器未响应\r\n");
		PC_Usart("  2. ESP8266固件问题\r\n");
		PC_Usart("  3. USART3中断未正常工作\r\n");
		PC_Usart("  4. 网络连接已断开\r\n");
	}
	
	// 8. 关闭连接
	PC_Usart("\r\n[8] 关闭连接...\r\n");
	ESP8266_Cmd("AT+CIPCLOSE", "OK", "CLOSED", 2000);
	
	PC_Usart("\r\n测试完成！\r\n");
}


