#include "wifi_function.h"
#include "wifi_config.h"
#include "SysTick.h"
#include "usart.h"
#include "../cJSON/cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

// ȫ�ֱ���������־���
static bool g_wifi_log_enable = true;

// ������־���״̬
void WiFi_Set_Log(bool enable)
{
	g_wifi_log_enable = enable;
}

// �ڲ���־�꣬��g_wifi_log_enable����
#define WIFI_LOG(...) if(g_wifi_log_enable) PC_Usart(__VA_ARGS__)

// HTTP��Ӧ������
static char g_weather_http_response[1024];

/*
 * ��������ESP8266_Choose
 * ����  ��ʹ��/����WF-ESP8266ģ��
 * ����  ��enumChoose = ENABLE��ʹ��ģ��
 *         enumChoose = DISABLE������ģ��
 * ����  : ��
 * ����  �����ⲿ����
 */
void ESP8266_Choose ( FunctionalState enumChoose )
{
	if ( enumChoose == ENABLE )
		ESP8266_CH_HIGH_LEVEL();
	
	else
		ESP8266_CH_LOW_LEVEL();
	
}


/*
 * ��������ESP8266_Rst
 * ����  ������WF-ESP8266ģ��
 * ����  ����
 * ����  : ��
 * ����  ����ESP8266_AT_Test����
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
 * ��������ESP8266_Auto_BaudRate_Test
 * ����  ��ESP8266ģ�鲨�����Զ����
 * ����  ����
 * ����  : ��
 * ����  �����ⲿ����
 */
void ESP8266_Auto_BaudRate_Test ( void )
{
	u32 baud_rates[] = {115200, 9600, 57600, 38400, 19200, 4800};
	u8 i;
	USART_InitTypeDef USART_InitStructure;
	
	PC_Usart("\r\n��ʼESP8266�������Զ����...\r\n");
	
	for(i = 0; i < 6; i++)
	{
		PC_Usart("���Բ�����: %d\r\n", baud_rates[i]);
		
		// ��������USART3������
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
		
		// ����ATָ��
		if( ESP8266_Cmd ( "AT", "OK", NULL, 1000 ) )
		{
			PC_Usart("�ҵ���ȷ������: %d\r\n", baud_rates[i]);
			return;
		}
		
		delay_ms(500);
	}
	
	PC_Usart("δ�ҵ�ƥ��Ĳ����ʣ�ʹ��Ĭ��115200\r\n");
	// ���û�Ĭ�ϲ�����115200
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
 * ��������ESP8266_AT_Test
 * ����  ����WF-ESP8266ģ�����AT��������
 * ����  ����
 * ����  : ��
 * ����  �����ⲿ����
 */
void ESP8266_AT_Test ( void )
{
	u8 retry_count = 0;
	
	ESP8266_RST_HIGH_LEVEL();
	
	delay_ms ( 1000 ); 
	
	PC_Usart("\r\n��ʼESP8266 AT����...\r\n");
	
	while ( ! ESP8266_Cmd ( "AT", "OK", NULL, 1000 ) ) 
	{
		retry_count++;
		PC_Usart("AT����ʧ�ܣ����Ե�%d��...\r\n", retry_count);
		ESP8266_Rst ();
		delay_ms(2000);  // �������������ʱ
		
		if(retry_count >= 5)  // �������Դ���
		{
			PC_Usart("ESP8266ģ������Ӧ������Ӳ�����ӺͲ��������ã�\r\n");
			break;
		}
	}
	
	if(retry_count < 5)
		PC_Usart("ESP8266 AT���Գɹ���\r\n");

}


/*
 * ��������ESP8266_Cmd
 * ����  ����WF-ESP8266ģ�鷢��ATָ��
 * ����  ��cmd�������͵�ָ��
 *         reply1��reply2���ڴ�����Ӧ��ΪNULL��������Ӧ������Ϊ���߼���ϵ
 *         waittime���ȴ���Ӧ��ʱ��
 * ����  : 1��ָ��ͳɹ�
 *         0��ָ���ʧ��
 * ����  �����ⲿ����
 */
bool ESP8266_Cmd ( char * cmd, char * reply1, char * reply2, u32 waittime )
{    
	u32 start_time = 0;
	bool found = false;
	u32 elapsed_time = 0;
	u16 last_length = 0;
	u8 stable_count = 0;
	
	// ����ǰ�����ӳ٣�����ָ�����
	delay_ms(300);
	
	// ������ջ�����
	strEsp8266_Fram_Record .InfBit .FramLength = 0;               //���¿�ʼ�����µ����ݰ�
	strEsp8266_Fram_Record .InfBit .FramFinishFlag = 0;           //���������ɱ�־
	memset(strEsp8266_Fram_Record .Data_RX_BUF, 0, RX_BUF_MAX_LEN); //��ս��ջ�����

	WIFI_LOG ("\r\n[����ATָ��]: %s\r\n", cmd);  // ��ʾ���͵�ָ��
	ESP8266_Usart ( "%s\r\n", cmd );

	if ( ( reply1 == 0 ) && ( reply2 == 0 ) )                      //����Ҫ��������
		return true;
	
	// �Ľ���ѭ������Ƿ��յ���������Ӧ
	start_time = 0;
	while(start_time < waittime)
	{
		delay_ms(50);  // ÿ50ms���һ��
		start_time += 50;
		
		// ��ʱ���ӽ������Ա�strstr����
		if(strEsp8266_Fram_Record.InfBit.FramLength > 0)
		{
			strEsp8266_Fram_Record.Data_RX_BUF[strEsp8266_Fram_Record.InfBit.FramLength] = '\0';
			
			// ����Ƿ��յ���������Ӧ
			if((reply1 && strstr(strEsp8266_Fram_Record.Data_RX_BUF, reply1)) || 
			   (reply2 && strstr(strEsp8266_Fram_Record.Data_RX_BUF, reply2)))
			{
				found = true;
				break;
			}
			
			// ����Ƿ�busy
			if(strstr(strEsp8266_Fram_Record.Data_RX_BUF, "busy"))
			{
				WIFI_LOG ("[����]: ESP8266ģ��æµ���ȴ�2�������...\r\n");
				delay_ms(2000);
				// ��������������·���
				strEsp8266_Fram_Record .InfBit .FramLength = 0;
				strEsp8266_Fram_Record .InfBit .FramFinishFlag = 0;
				memset(strEsp8266_Fram_Record .Data_RX_BUF, 0, RX_BUF_MAX_LEN);
				
				ESP8266_Usart ( "%s\r\n", cmd );
				start_time = 0; // ���ü�ʱ��
				continue;
			}
		}
	}
	
	strEsp8266_Fram_Record .Data_RX_BUF [ strEsp8266_Fram_Record .InfBit .FramLength ]  = '\0';

	// ���⴦��CWLAP������������ȡӢ��WiFi����
	if(strstr(cmd, "CWLAP")) 
	{
		char *ptr;
		char *ssid_start;
		char *ssid_end;
		char *c;
		bool is_ascii;
		u8 wifi_count = 0;
		
		// ����ʾԭʼ���ݳ������ڵ���
		WIFI_LOG ("[�������ݳ���]: %d�ֽ�\r\n", strEsp8266_Fram_Record .InfBit .FramLength);
		
		WIFI_LOG ("[ɨ�赽��WiFi����]:\r\n");
		// ���Ҳ���ʾӢ��WiFi����
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
					// ����Ƿ�ΪӢ��/����WiFi����
					is_ascii = true;
					for(c = ssid_start; c < ssid_end; c++)
					{
						if(*c < 32 || *c > 126)  // ��ASCII�ַ�
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
			WIFI_LOG ("  δ����Ӣ�����Ƶ�WiFi����\r\n");
			WIFI_LOG ("  (���ܶ����������ƻ�ɨ�賬ʱ)\r\n");
		}
	}
	else
	{
		WIFI_LOG ("[ESP8266�ظ�]: %s\r\n", strEsp8266_Fram_Record .Data_RX_BUF);
	}
  
	return found;
	
}


/*
 * ��������ESP8266_Net_Mode_Choose
 * ����  ��ѡ��WF-ESP8266ģ��Ĺ���ģʽ
 * ����  ��enumMode������ģʽ
 * ����  : 1��ѡ��ɹ�
 *         0��ѡ��ʧ��
 * ����  �����ⲿ����
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
 * ��������ESP8266_Scan_AP
 * ����  ��ɨ����Χ��WiFi����
 * ����  ����
 * ����  : ��
 * ����  �����ⲿ����
 */
void ESP8266_Scan_AP ( void )
{
	PC_Usart("��ʼɨ��WiFi���磨Ԥ����Ҫ10-15�룩...\r\n");
	delay_ms(3000);  // ɨ��ǰ�ȴ�3�룬��ESP8266��ȫ����
	ESP8266_Cmd ( "AT+CWLAP", "OK", NULL, 15000 );  // �ȴ�15��ɨ�����
}

/*
 * ��������ESP8266_JoinAP
 * ����  ��WF-ESP8266ģ�������ⲿWiFi
 * ����  ��pSSID��WiFi�����ַ���
 *       ��pPassWord��WiFi�����ַ���
 * ����  : 1�����ӳɹ�
 *         0������ʧ��
 * ����  �����ⲿ����
 */
bool ESP8266_JoinAP ( char * pSSID, char * pPassWord )
{
	char cCmd [120];
	bool result;

	sprintf ( cCmd, "AT+CWJAP=\"%s\",\"%s\"", pSSID, pPassWord );
	
	PC_Usart("\r\n��������WiFi: %s...\r\n", pSSID);
	
	// ����ǰ�����ӳ٣�ȷ��ģ�����
	delay_ms(500);
	
	// ���ӵȴ�ʱ�䵽10�룬��ΪWiFi���ӿ�����Ҫ����ʱ��
	result = ESP8266_Cmd ( cCmd, "OK", "WIFI GOT IP", 10000 );
	
	if(result) 
	{
		PC_Usart("? WiFi���ӳɹ���\r\n");
		// �鿴��õ�IP��ַ
		delay_ms(1000);
		ESP8266_Cmd ( "AT+CIFSR", "OK", NULL, 2000 );
	}
	else
	{
		PC_Usart("? WiFi����ʧ�ܣ�\r\n");
		PC_Usart("���ܵ�ԭ��\r\n");
		PC_Usart("1. WiFi�������\r\n");
		PC_Usart("2. WiFi���ư��������ַ�\r\n");
		PC_Usart("3. �ź�ǿ�Ȳ���\r\n");
		PC_Usart("4. Ƶ�β�֧��(ESP8266ֻ֧��2.4GHz)\r\n");
	}
	
	return result;
	
}


/*
 * ��������ESP8266_BuildAP
 * ����  ��WF-ESP8266ģ�鴴��WiFi�ȵ�
 * ����  ��pSSID��WiFi�����ַ���
 *       ��pPassWord��WiFi�����ַ���
 *       ��enunPsdMode��WiFi���ܷ�ʽ�����ַ���
 * ����  : 1�������ɹ�
 *         0������ʧ��
 * ����  �����ⲿ����
 */
bool ESP8266_BuildAP ( char * pSSID, char * pPassWord, char * enunPsdMode )
{
	char cCmd [120];

	sprintf ( cCmd, "AT+CWSAP=\"%s\",\"%s\",1,%s", pSSID, pPassWord, enunPsdMode );
	
	return ESP8266_Cmd ( cCmd, "OK", 0, 1000 );
	
}


/*
 * ��������ESP8266_Enable_MultipleId
 * ����  ��WF-ESP8266ģ������������
 * ����  ��enumEnUnvarnishTx�������Ƿ������
 * ����  : 1�����óɹ�
 *         0������ʧ��
 * ����  �����ⲿ����
 */
bool ESP8266_Enable_MultipleId ( FunctionalState enumEnUnvarnishTx )
{
	char cStr [20];
	
	sprintf ( cStr, "AT+CIPMUX=%d", ( enumEnUnvarnishTx ? 1 : 0 ) );
	
	return ESP8266_Cmd ( cStr, "OK", 0, 500 );
	
}


/*
 * ��������ESP8266_Link_Server
 * ����  ��WF-ESP8266ģ�������ⲿ������
 * ����  ��enumE������Э��
 *       ��ip��������IP�ַ���
 *       ��ComNum���������˿��ַ���
 *       ��id��ģ�����ӷ�������ID
 * ����  : 1�����ӳɹ�
 *         0������ʧ��
 * ����  �����ⲿ����
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

	PC_Usart("\r\n�������ӷ����� %s:%s...\r\n", ip, ComNum);

	// ���ӵȴ�ʱ�䵽10�룬���ȴ���ȷ����Ӧ
	result = ESP8266_Cmd ( cCmd, "CONNECT", "OK", 10000 );
	
	if(result) 
	{
		PC_Usart("? TCP���ӳɹ���\r\n");
	}
	else
	{
		PC_Usart("? TCP����ʧ�ܣ�\r\n");
		PC_Usart("���ܵ�ԭ��\r\n");
		PC_Usart("1. ������IP��ַ����ȷ\r\n");
		PC_Usart("2. �������˿�δ����\r\n");
		PC_Usart("3. ����ǽ��ֹ����\r\n");
		PC_Usart("4. ���粻ͨ\r\n");
		PC_Usart("5. ������δ����\r\n");
	}
	
	return result;
	
}


/*
 * ��������ESP8266_StartOrShutServer
 * ����  ��WF-ESP8266ģ�鿪����رշ�����ģʽ
 * ����  ��enumMode������/�ر�
 *       ��pPortNum���������˿ں��ַ���
 *       ��pTimeOver����������ʱʱ���ַ�������λ����
 * ����  : 1�������ɹ�
 *         0������ʧ��
 * ����  �����ⲿ����
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
 * ��������ESP8266_UnvarnishSend
 * ����  ������WF-ESP8266ģ�����͸������
 * ����  ����
 * ����  : 1�����óɹ�
 *         0������ʧ��
 * ����  �����ⲿ����
 */
bool ESP8266_UnvarnishSend ( void )
{
	return (
	  ESP8266_Cmd ( "AT+CIPMODE=1", "OK", 0, 500 ) &&
	  ESP8266_Cmd ( "AT+CIPSEND", "\r\n", ">", 500 ) );
	
}


/*
 * ��������ESP8266_SendString
 * ����  ��WF-ESP8266ģ�鷢���ַ���
 * ����  ��enumEnUnvarnishTx�������Ƿ���ʹ����͸��ģʽ
 *       ��pStr��Ҫ���͵��ַ���
 *       ��ulStrLength��Ҫ���͵��ַ������ֽ���
 *       ��ucId���ĸ�ID���͵��ַ���
 * ����  : 1�����ͳɹ�
 *         0������ʧ��
 * ����  �����ⲿ����
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
 * ��������ESP8266_ReceiveString
 * ����  ��WF-ESP8266ģ������ַ���
 * ����  ��enumEnUnvarnishTx�������Ƿ���ʹ����͸��ģʽ
 * ����  : ���յ����ַ����׵�ַ
 * ����  �����ⲿ����
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

void ESP8266_STA_TCP_Client(void)
{
    // Placeholder or empty if not used
}

void ESP8266_AP_TCP_Server(void)
{
    // Placeholder
}

void ESP8266_StaTcpClient_ApTcpServer(void)
{
    // Placeholder
}

bool WiFi_Connect_Default(void)
{
	WIFI_LOG("\r\n=== ��������Ĭ��WiFi ===\r\n");
	WIFI_LOG("SSID: %s\r\n", macUser_ESP8266_ApSsid);
	
	if(ESP8266_JoinAP(macUser_ESP8266_ApSsid, macUser_ESP8266_ApPwd))
	{
		WIFI_LOG("? Ĭ��WiFi���ӳɹ�\r\n");
		return true;
	}
	else
	{
		WIFI_LOG("? Ĭ��WiFi����ʧ��\r\n");
		return false;
	}
}

bool WiFi_Connect_Menu(void)
{
	char choice;
	
	while(1)
	{
		PC_Usart("\r\n=== WiFi����ʧ�ܴ����˵� ===\r\n");
		PC_Usart("1. ��������Ĭ��WiFi\r\n");
		PC_Usart("2. ɨ�貢��������WiFi\r\n");
		PC_Usart("������ѡ��(1��2): ");
		
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
			PC_Usart("\r\n=== ɨ�����WiFi���� ===\r\n");
			ESP8266_Cmd("AT+CWLAP", "OK", NULL, 15000);
			
			PC_Usart("\r\n������Ҫ���ӵ�WiFi��Ϣ\r\n");
			PC_Usart("��ʽ: WiFi����,����\r\n");
			PC_Usart("����: TP-LINK_5G,12345678\r\n");
			PC_Usart("������(�Իس�����): ");
			
			char input[100] = {0};
			u8 idx = 0;
			char ch;
			
			while(idx < 99)
			{
				while(USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET);
				ch = USART_ReceiveData(USART1);
				
				if(ch == '\r' || ch == '\n')
				{
					if(idx > 0) break;
					else continue;
				}
				
				USART_SendData(USART1, ch);
				while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
				
				input[idx++] = ch;
			}
			input[idx] = '\0';
			PC_Usart("\r\n");
			
			char *comma = strchr(input, ',');
			if(comma)
			{
				*comma = '\0';
				char *ssid = input;
				char *pwd = comma + 1;
				
				if(ESP8266_JoinAP(ssid, pwd))
				{
					PC_Usart("? WiFi���ӳɹ�\r\n");
					return true;
				}
				else
				{
					PC_Usart("? WiFi����ʧ��\r\n");
				}
			}
			else
			{
				PC_Usart("? ��ʽ����\r\n");
			}
		}
	}
}

bool WiFi_Ping_Test(char *target)
{
	char cmd[64];
	sprintf(cmd, "AT+PING=\"%s\"", target);
	WIFI_LOG("Ping %s ...\r\n", target);
	return ESP8266_Cmd(cmd, "OK", NULL, 5000);
}

bool ESP8266_TCP_Connect(char *ip, u16 port)
{
	char cmd[64];
	sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d", ip, port);
	return ESP8266_Cmd(cmd, "OK", "CONNECT", 5000);
}

bool ESP8266_TCP_Send(char *data, u16 len)
{
	char cmd[32];
	sprintf(cmd, "AT+CIPSEND=%d", len);
	if(ESP8266_Cmd(cmd, ">", NULL, 3000))
	{
		// ��������
		// ʹ��ѭ�����ֽڷ��ͣ�ȷ������������
		for(int i=0; i<len; i++)
		{
			USART_SendData(USART3, data[i]);
			while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
		}
		
		// �ȴ����ݷ�����ɣ�ESP8266��Ҫʱ�䴦��
		delay_ms(500);
		
		return true; 
	}
	return false;
}

// 多连接模式发送数据（指定连接ID）
bool ESP8266_TCP_Send_MultiLink(u8 link_id, u8 *data, u16 len)
{
	char cmd[32];
	sprintf(cmd, "AT+CIPSEND=%d,%d", link_id, len);
	if(ESP8266_Cmd(cmd, ">", NULL, 3000))
	{
		// 发送数据
		for(int i=0; i<len; i++)
		{
			USART_SendData(USART3, data[i]);
			while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
		}
		
		// 等待数据发送完成
		delay_ms(500);
		
		return true; 
	}
	return false;
}

bool ESP8266_TCP_Disconnect(void)
{
	WIFI_LOG("\r\n���ڹر�TCP����...\r\n");
	
	if(ESP8266_Cmd("AT+CIPCLOSE", "OK", "CLOSED", 2000))
	{
		WIFI_LOG("? TCP�����ѹر�\r\n");
		return true;
	}
	else
	{
		WIFI_LOG("? �ر�TCP����ʧ��\r\n");
		return false;
	}
}

bool ESP8266_HTTP_Request(char *host, u16 port, char *path, char *response, u16 max_len)
{
	char http_request[512];
	u16 len;
	
	WIFI_LOG("\r\n=== ����HTTP���� ===\r\n");
	WIFI_LOG("Ŀ��: http://%s:%d%s\r\n", host, port, path);
	
	if(!ESP8266_TCP_Connect(host, port))
	{
		return false;
	}
	
	sprintf(http_request, 
		"GET %s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"User-Agent: ESP8266\r\n"
		"Connection: close\r\n"
		"\r\n",
		path, host);
	
	len = strlen(http_request);
	WIFI_LOG("����HTTP���� (%d�ֽ�):\r\n%s\r\n", len, http_request);
	
	strEsp8266_Fram_Record.InfBit.FramLength = 0;
	strEsp8266_Fram_Record.InfBit.FramFinishFlag = 0;
	memset(strEsp8266_Fram_Record.Data_RX_BUF, 0, RX_BUF_MAX_LEN);
	delay_ms(100);
	
	if(!ESP8266_TCP_Send(http_request, len))
	{
		WIFI_LOG("? ����HTTP����ʧ��\r\n");
		ESP8266_TCP_Disconnect();
		return false;
	}
	
	WIFI_LOG("? HTTP�����ѷ���\r\n");
	WIFI_LOG("�ȴ���������Ӧ...\r\n");
	
	u8 retry = 0;
	bool got_data = false;
	u16 last_len = 0;
	u8 stable_cnt = 0;
	
	while(retry < 50)
	{
		delay_ms(200);
		
		if(strEsp8266_Fram_Record.InfBit.FramLength > 0)
		{
			strEsp8266_Fram_Record.Data_RX_BUF[strEsp8266_Fram_Record.InfBit.FramLength] = '\0';
		}
		
		if(!got_data)
		{
			if(strEsp8266_Fram_Record.InfBit.FramLength > 10)
			{
				if(strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "+IPD") ||
				   strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "HTTP/"))
				{
					WIFI_LOG("? ��ʼ�������� (%d�ֽ�)...\r\n", strEsp8266_Fram_Record.InfBit.FramLength);
					got_data = true;
					last_len = strEsp8266_Fram_Record.InfBit.FramLength;
				}
			}
		}
		else
		{
			if(strEsp8266_Fram_Record.InfBit.FramLength == last_len)
			{
				stable_cnt++;
				if(stable_cnt >= 5)
				{
					WIFI_LOG("? ���ݽ����ȶ� (%d�ֽ�)\r\n", last_len);
					break;
				}
			}
			else
			{
				last_len = strEsp8266_Fram_Record.InfBit.FramLength;
				stable_cnt = 0;
			}
			
			if(strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "CLOSED"))
			{
				 WIFI_LOG("? ��⵽���ӹرգ����ս���\r\n");
				 break;
			}
		}
		
		retry++;
	}
	
	if(!got_data && retry >= 50)
	{
		WIFI_LOG("? �ȴ���ʱ��\r\n");
		ESP8266_TCP_Disconnect();
		return false;
	}
	
	if(got_data && strEsp8266_Fram_Record.InfBit.FramLength > 0)
	{
		char *http_start = NULL;
		char *closed_ptr = NULL;
		
		strEsp8266_Fram_Record.Data_RX_BUF[strEsp8266_Fram_Record.InfBit.FramLength] = '\0';
		
		closed_ptr = strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "CLOSED");
		if(closed_ptr)
		{
			*closed_ptr = '\0';
			strEsp8266_Fram_Record.InfBit.FramLength = closed_ptr - (char*)strEsp8266_Fram_Record.Data_RX_BUF;
		}
		
		http_start = strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "HTTP/");
		if(http_start == NULL)
		{
			http_start = strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "{");
		}
		
		if(http_start == NULL)
		{
			http_start = (char*)strEsp8266_Fram_Record.Data_RX_BUF;
		}
		
		u16 http_len = strlen(http_start);
		u16 copy_len = http_len;
		if(copy_len > max_len - 1)
		{
			copy_len = max_len - 1;
		}
		
		memcpy(response, http_start, copy_len);
		response[copy_len] = '\0';
		
		WIFI_LOG("? ���յ�HTTP��Ӧ (%d�ֽ�)\r\n", copy_len);
		
		if(closed_ptr != NULL)
		{
			WIFI_LOG("? �������ɷ������ر�\r\n");
		}
		else
		{
			if(strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "CLOSED"))
			{
				WIFI_LOG("? ��⵽CLOSED\r\n");
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
		WIFI_LOG("? δ���յ���Ӧ\r\n");
		ESP8266_TCP_Disconnect();
		return false;
	}
}

/**
  * @brief  ESP8266_HTTP_Post ����HTTP POST���󲢽�����Ӧ
  * @param  host��������������IP
  * @param  port���������˿ڣ�ͨ��80
  * @param  path������·��
  * @param  body��POST������
  * @param  response���洢��Ӧ�Ļ�����
  * @param  max_len����������󳤶�
  * @retval true������ɹ���false������ʧ��
  */
bool ESP8266_HTTP_Post(char *host, u16 port, char *path, char *body, char *response, u16 max_len)
{
	char http_request[1024]; // ���ӻ�������С������body
	u16 len;
	
	WIFI_LOG("\r\n=== ����HTTP POST���� ===\r\n");
	WIFI_LOG("Ŀ��: http://%s:%d%s\r\n", host, port, path);
	
	// 1. ����TCP������
	if(!ESP8266_TCP_Connect(host, port))
	{
		return false;
	}
	
	// 2. ����HTTP POST����
	sprintf(http_request, 
		"POST %s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"User-Agent: ESP8266\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: %d\r\n"
		"Connection: close\r\n"
		"\r\n"
		"%s",
		path, host, strlen(body), body);
	
	len = strlen(http_request);
	WIFI_LOG("����HTTP���� (%d�ֽ�):\r\n%s\r\n", len, http_request);
	
	// 3. ��ս��ջ�����׼������HTTP��Ӧ
	strEsp8266_Fram_Record.InfBit.FramLength = 0;
	strEsp8266_Fram_Record.InfBit.FramFinishFlag = 0;
	memset(strEsp8266_Fram_Record.Data_RX_BUF, 0, RX_BUF_MAX_LEN);
	delay_ms(100);
	
	// 4. ����HTTP����
	if(!ESP8266_TCP_Send(http_request, len))
	{
		WIFI_LOG("? ����HTTP����ʧ��\r\n");
		ESP8266_TCP_Disconnect();
		return false;
	}
	
	WIFI_LOG("? HTTP�����ѷ���\r\n");
	
	// 5. �ȴ���������Ӧ
	WIFI_LOG("�ȴ���������Ӧ...\r\n");
	u8 retry = 0;
	bool got_data = false;
	u16 last_len = 0;
	u8 stable_cnt = 0;
	
	while(retry < 50)  // ���ȴ�10��
	{
		delay_ms(200);
		
		// ȷ���ַ���������
		if(strEsp8266_Fram_Record.InfBit.FramLength > 0)
		{
			strEsp8266_Fram_Record.Data_RX_BUF[strEsp8266_Fram_Record.InfBit.FramLength] = '\0';
		}
		
		// �����û�յ�����
		if(!got_data)
		{
			if(strEsp8266_Fram_Record.InfBit.FramLength > 10)
			{
				// ����Ƿ����+IPD��HTTP��Ӧͷ
				if(strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "+IPD") ||
				   strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "HTTP/"))
				{
					WIFI_LOG("? ��ʼ�������� (%d�ֽ�)...\r\n", strEsp8266_Fram_Record.InfBit.FramLength);
					got_data = true;
					last_len = strEsp8266_Fram_Record.InfBit.FramLength;
				}
			}
		}
		else
		{
			// ���յ����ݣ�����Ƿ��ȶ�
			if(strEsp8266_Fram_Record.InfBit.FramLength == last_len)
			{
				stable_cnt++;
				// ����5��(1��)���Ȳ��䣬��Ϊ�������
				if(stable_cnt >= 5)
				{
					WIFI_LOG("? ���ݽ����ȶ� (%d�ֽ�)\r\n", last_len);
					break;
				}
			}
			else
			{
				last_len = strEsp8266_Fram_Record.InfBit.FramLength;
				stable_cnt = 0;
			}
			
			// ����Ƿ���� CLOSED (�����������ر�����)
			if(strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "CLOSED"))
			{
				 WIFI_LOG("? ��⵽���ӹرգ����ս���\r\n");
				 break;
			}
		}
		
		retry++;
	}
	
	if(!got_data && retry >= 50)
	{
		WIFI_LOG("? �ȴ���ʱ��\r\n");
		ESP8266_TCP_Disconnect();
		return false;
	}
	
	// 6. ������Ӧ����
	if(got_data && strEsp8266_Fram_Record.InfBit.FramLength > 0)
	{
		char *http_start = NULL;
		char *closed_ptr = NULL;
		
		// ȷ��������
		strEsp8266_Fram_Record.Data_RX_BUF[strEsp8266_Fram_Record.InfBit.FramLength] = '\0';
		
		// ȥ��ĩβ�� CLOSED
		closed_ptr = strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "CLOSED");
		if(closed_ptr)
		{
			*closed_ptr = '\0';
			strEsp8266_Fram_Record.InfBit.FramLength = closed_ptr - (char*)strEsp8266_Fram_Record.Data_RX_BUF;
		}
		
		// ����HTTP��Ӧ�Ŀ�ʼλ��
		http_start = strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "HTTP/");
		if(http_start == NULL)
		{
			http_start = strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "{");
		}
		
		if(http_start == NULL)
		{
			http_start = (char*)strEsp8266_Fram_Record.Data_RX_BUF;
		}
		
		// ����ʵ��HTTP���ݳ���
		u16 http_len = strlen(http_start);
		u16 copy_len = http_len;
		if(copy_len > max_len - 1)
		{
			copy_len = max_len - 1;
		}
		
		memcpy(response, http_start, copy_len);
		response[copy_len] = '\0';
		
		WIFI_LOG("? ���յ�HTTP��Ӧ (%d�ֽ�)\r\n", copy_len);
		
		if(closed_ptr != NULL)
		{
			WIFI_LOG("? �������ɷ������ر�\r\n");
		}
		else
		{
			if(strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "CLOSED"))
			{
				WIFI_LOG("? ��⵽CLOSED\r\n");
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
		WIFI_LOG("? δ���յ���Ӧ\r\n");
		ESP8266_TCP_Disconnect();
		return false;
	}
}

/**
  * @brief  Weather_Get_Data ��ȡ��������
  * @param  weather���洢�������ݵĽṹ��ָ��
  * @retval true����ȡ�ɹ���false����ȡʧ��
  */
bool Weather_Get_Data(WeatherData *weather)
{
	// ʹ��ȫ�ֻ���������ջ����
	// char http_response[2048]; 
	char *json_start;
	cJSON *root = NULL;
	cJSON *data = NULL;
	cJSON *item = NULL;
	bool result = false;
	
	// ��ʼ����������
	weather->valid = false;
	memset(weather->location, 0, sizeof(weather->location));
	memset(weather->temperature, 0, sizeof(weather->temperature));
	memset(weather->humidity, 0, sizeof(weather->humidity));
	memset(weather->weather, 0, sizeof(weather->weather));
	memset(weather->description, 0, sizeof(weather->description));
	memset(weather->last_update, 0, sizeof(weather->last_update));
	
	WIFI_LOG("\r\n=== ��ȡ�������� ===\r\n");
	
	// ֱ��ʹ��IP��ַ���������������ӳ�
	// ������IP: 47.83.221.161
	if(!ESP8266_HTTP_Request("47.83.221.161", 80, "/api/weather/", g_weather_http_response, sizeof(g_weather_http_response)))
	{
		WIFI_LOG("? HTTP����ʧ��\r\n");
		return false;
	}
	
	// ����JSON������ʼλ�ã�����HTTPͷ��
	json_start = strstr(g_weather_http_response, "\r\n\r\n");
	if(json_start != NULL)
	{
		json_start += 4;  // ���� \r\n\r\n
		// �ٴ�ȷ���Ƿ��� { ��ͷ
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
		WIFI_LOG("? δ�ҵ�JSON����\r\n");
		return false;
	}
	
	// ����JSON����λ�ã����һ�� }��
	char *json_end = strrchr(json_start, '}');
	if(json_end != NULL)
	{
		*(json_end + 1) = '\0'; // �ضϺ�����������
	}
	else
	{
		WIFI_LOG("? δ�ҵ�JSON������\r\n");
		// ���Լ���������Ҳ��ֻ��ȱ�ٽ�����
	}
	
	WIFI_LOG("����JSON����...\r\n");
	WIFI_LOG("��������JSON����:\r\n%s\r\n", json_start); // ��ӡ����JSON�����Ա����
	
	// ����JSON
	root = cJSON_Parse(json_start);
	if(root == NULL)
	{
		WIFI_LOG("? JSON����ʧ��\r\n");
		const char *error_ptr = cJSON_GetErrorPtr();
		if(error_ptr != NULL)
		{
			WIFI_LOG("����λ��: %s\r\n", error_ptr);
		}
		return false;
	}
	
	// ���status�ֶ�
	item = cJSON_GetObjectItem(root, "status");
	if(item != NULL && cJSON_IsString(item))
	{
		if(strcmp(item->valuestring, "success") != 0)
		{
			// ���������ش���״̬
			WIFI_LOG("? API���ش���״̬: %s\r\n", item->valuestring);
			
			// ���Ի�ȡ������Ϣ
			cJSON *msg_item = cJSON_GetObjectItem(root, "msg");
			if(msg_item != NULL && cJSON_IsString(msg_item))
			{
				// ���Լ���ض���UTF-8������Ϣ "���η�����������"
				// ʹ��ʮ������ƥ���Ա����������: \xE4\xB8\x8A\xE6\xB8\xB8 (����)
				if(strstr(msg_item->valuestring, "\xE4\xB8\x8A\xE6\xB8\xB8") != NULL || 
				   strstr(msg_item->valuestring, "unavailable") != NULL)
				{
					WIFI_LOG("��ʾ: ���η����������ã����Ժ�����\r\n");
				}
				else
				{
					// ����������Ϣ��ֱ�Ӵ�ӡ�����ܰ���UTF-8�ַ��������룩
					WIFI_LOG("������Ϣ(UTF-8): %s\r\n", msg_item->valuestring);
				}
			}
			
			cJSON_Delete(root);
			return false;
		}
	}
	else
	{
		WIFI_LOG("? δ�ҵ�status�ֶλ��ʽ����\r\n");
		cJSON_Delete(root);
		return false;
	}
	
	// ��ȡdata����
	data = cJSON_GetObjectItem(root, "data");
	if(data == NULL)
	{
		WIFI_LOG("? δ�ҵ�data�ֶ�\r\n");
		cJSON_Delete(root);
		return false;
	}
	
	// ��ȡ�����ֶ�
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
	
	WIFI_LOG("? �������ݽ����ɹ�\r\n");
	
	// �ͷ�JSON����
	cJSON_Delete(root);
	
	return result;
}

/**
  * @brief  Weather_Display ��ʾ��������
  * @param  weather���������ݽṹ��ָ��
  * @retval ��
  */
void Weather_Display(WeatherData *weather)
{
	if(!weather->valid)
	{
		PC_Usart("\r\n[����������Ч]\r\n");
		return;
	}
	
	PC_Usart("\r\n�X�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�[\r\n");
	PC_Usart("�U      ������Ϣ��ʾ              �U\r\n");
	PC_Usart("�d�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�g\r\n");
	PC_Usart("�U λ��: %-24s �U\r\n", weather->location);
	PC_Usart("�U �¶�: %-24s �U\r\n", weather->temperature);
	PC_Usart("�U ʪ��: %-24s �U\r\n", weather->humidity);
	PC_Usart("�U ����: %-24s �U\r\n", weather->weather);
	PC_Usart("�U ����: %-24s �U\r\n", weather->description);
	PC_Usart("�d�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�g\r\n");
	PC_Usart("�U ����: %-24s �U\r\n", weather->last_update);
	PC_Usart("�^�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�a\r\n");
}

/**
  * @brief  ESP8266_Simple_TCP_Test ��TCP���Ӳ���
  * @param  ��
  * @retval ���Խ��˵���ַ���
  * @note   �������TCP���Ӻ����ݽ�������
  */
void ESP8266_Simple_TCP_Test(void)
{
	char cmd[64];
	
	PC_Usart("\r\n�X�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�[\r\n");
	PC_Usart("�U    ESP8266 TCP������ϲ���           �U\r\n");
	PC_Usart("�^�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�T�a\r\n");
	
	// 1. 关闭所有现有连接
	PC_Usart("\r\n[1] 关闭现有连接...\r\n");
	ESP8266_Cmd("AT+CIPCLOSE", "OK", "CLOSED", 1000);
	delay_ms(500);
	
	// 2. 设置单连接模式（ESP8266固件不支持多连接语法）
	PC_Usart("\r\n[2] 设置单连接模式...\r\n");
	if(!ESP8266_Cmd("AT+CIPMUX=0", "OK", NULL, 1000))
	{
		PC_Usart("? 设置单连接模式失败\r\n");
		return;
	}
	delay_ms(500);
	
	// 3. ���ӷ�����
	PC_Usart("\r\n[3] ���ӷ����� 47.83.221.161:80...\r\n");
	sprintf(cmd, "AT+CIPSTART=\"TCP\",\"47.83.221.161\",80");
	if(!ESP8266_Cmd(cmd, "CONNECT", "OK", 10000))
	{
		PC_Usart("? TCP����ʧ��\r\n");
		return;
	}
	delay_ms(1000);
	
	// 4. �������״̬
	PC_Usart("\r\n[4] �������״̬...\r\n");
	ESP8266_Cmd("AT+CIPSTATUS", "OK", NULL, 1000);
	delay_ms(500);
	
	// 5. �����HTTP����
	PC_Usart("\r\n[5] ����HTTP����...\r\n");
	char http_req[] = "GET /api/weather/ HTTP/1.1\r\nHost: 47.83.221.161\r\nConnection: close\r\n\r\n";
	u16 req_len = strlen(http_req);
	
	sprintf(cmd, "AT+CIPSEND=%d", req_len);
	if(!ESP8266_Cmd(cmd, ">", NULL, 2000))
	{
		PC_Usart("? ���뷢��ģʽʧ��\r\n");
		ESP8266_Cmd("AT+CIPCLOSE", "OK", NULL, 1000);
		return;
	}
	
	// 6. ��������
	PC_Usart("\r\n[6] ��������...\r\n");
	PC_Usart("��������:\r\n%s\r\n", http_req);
	
	// ��ջ�����
	strEsp8266_Fram_Record.InfBit.FramLength = 0;
	memset(strEsp8266_Fram_Record.Data_RX_BUF, 0, RX_BUF_MAX_LEN);
	
	ESP8266_Usart("%s", http_req);
	delay_ms(500);
	
	// 7. �ȴ���Ӧ
	PC_Usart("\r\n[7] �ȴ���Ӧ (10��)...\r\n");
	u8 wait_count = 0;
	while(wait_count < 50)  // 50 x 200ms = 10��
	{
		delay_ms(200);
		
		if(strEsp8266_Fram_Record.InfBit.FramLength > 0)
		{
			PC_Usart("\r\n? �յ����ݣ�����: %d �ֽ�\r\n", strEsp8266_Fram_Record.InfBit.FramLength);
			PC_Usart("\r\n���յ���ԭʼ����:\r\n");
			PC_Usart("������������������������������������������������������������������\r\n");
			
			// ��ʾǰ500�ֽ�
			if(strEsp8266_Fram_Record.InfBit.FramLength > 500)
			{
				char preview[501];
				memcpy(preview, strEsp8266_Fram_Record.Data_RX_BUF, 500);
				preview[500] = '\0';
				PC_Usart("%s\r\n...(���ݹ���������ʾǰ500�ֽ�)\r\n", preview);
			}
			else
			{
				PC_Usart("%s\r\n", strEsp8266_Fram_Record.Data_RX_BUF);
			}
			PC_Usart("������������������������������������������������������������������\r\n");
			
			// �������ݸ�ʽ
			PC_Usart("\r\n[���ݸ�ʽ����]\r\n");
			if(strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "+IPD"))
			{
				PC_Usart("? ���� +IPD ǰ׺\r\n");
			}
			if(strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "HTTP/"))
			{
				PC_Usart("? ���� HTTP ��Ӧͷ\r\n");
			}
			if(strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "{"))
			{
				PC_Usart("? ���� JSON ����\r\n");
			}
			if(strstr((char*)strEsp8266_Fram_Record.Data_RX_BUF, "SEND OK"))
			{
				PC_Usart("? ���� SEND OK ȷ��\r\n");
			}
			
			break;
		}
		
		wait_count++;
		if(wait_count % 5 == 0)
		{
			PC_Usart("  �ȴ�... %d�� (���յ�%d�ֽ�)\r\n", 
				wait_count / 5 * 2, 
				strEsp8266_Fram_Record.InfBit.FramLength);
		}
	}
	
	if(wait_count >= 50)
	{
		PC_Usart("\r\n? �ȴ���ʱ��δ�յ���Ӧ\r\n");
		PC_Usart("����ԭ��:\r\n");
		PC_Usart("  1. ������δ��Ӧ\r\n");
		PC_Usart("  2. ESP8266�̼�����\r\n");
		PC_Usart("  3. USART3�ж�δ��������\r\n");
		PC_Usart("  4. ���������ѶϿ�\r\n");
	}
	
	// 8. �ر�����
	PC_Usart("\r\n[8] �ر�����...\r\n");
	ESP8266_Cmd("AT+CIPCLOSE", "OK", "CLOSED", 2000);
	
	PC_Usart("\r\n������ɣ�\r\n");
}

/**
  * @brief  Weather_Update_Location ����λ����Ϣ
  * @param  ��
  * @retval true�����³ɹ���false������ʧ��
  */
bool Weather_Update_Location(void)
{
	char city[64] = {0};
	double lat = 0, lon = 0;
	char *json_start;
	cJSON *root = NULL;
	cJSON *item = NULL;
	
	WIFI_LOG("\r\n=== ��ʼ����λ����Ϣ ===\r\n");
	
	// 1. ��ȡ����IP
	WIFI_LOG("1. ��ȡ����IP��ַ...\r\n");
	ESP8266_Cmd("AT+CIFSR", "OK", NULL, 2000);
	
	// 2. ��ȡλ����Ϣ
	WIFI_LOG("2. ��ip-api.com��ȡλ����Ϣ...\r\n");
	
	// ʹ��ȫ�ֻ�����
	if(!ESP8266_HTTP_Request("ip-api.com", 80, "/json/", g_weather_http_response, sizeof(g_weather_http_response)))
	{
		WIFI_LOG("? ��ȡλ����Ϣʧ��\r\n");
		return false;
	}
	
	// ����JSON
	json_start = strstr(g_weather_http_response, "{");
	if(json_start == NULL)
	{
		WIFI_LOG("? δ�ҵ�JSON����\r\n");
		return false;
	}
	
	root = cJSON_Parse(json_start);
	if(root == NULL)
	{
		WIFI_LOG("? JSON����ʧ��\r\n");
		return false;
	}
	
	// ��ȡcity, lat, lon
	item = cJSON_GetObjectItem(root, "city");
	if(item != NULL && cJSON_IsString(item))
	{
		strncpy(city, item->valuestring, sizeof(city)-1);
	}
	
	item = cJSON_GetObjectItem(root, "lat");
	if(item != NULL && cJSON_IsNumber(item))
	{
		lat = item->valuedouble;
	}
	
	item = cJSON_GetObjectItem(root, "lon");
	if(item != NULL && cJSON_IsNumber(item))
	{
		lon = item->valuedouble;
	}
	
	cJSON_Delete(root);
	
	WIFI_LOG("��ȡ����λ��: City=%s, Lat=%.4f, Lon=%.4f\r\n", city, lat, lon);
	
	if(strlen(city) == 0)
	{
		WIFI_LOG("? λ����Ϣ��Ч\r\n");
		return false;
	}
	
	// 3. �ϱ�λ����Ϣ
	WIFI_LOG("3. �ϱ�λ����Ϣ��������...\r\n");
	char body[256];
	sprintf(body, "{\"city\": \"%s\", \"lat\": %.4f, \"lon\": %.4f}", city, lat, lon);
	
	// ������IP: 47.83.221.161
	if(!ESP8266_HTTP_Post("47.83.221.161", 80, "/api/weather/location", body, g_weather_http_response, sizeof(g_weather_http_response)))
	{
		WIFI_LOG("? �ϱ�λ����Ϣʧ��\r\n");
		return false;
	}
	
	WIFI_LOG("? λ����Ϣ�������\r\n");
	return true;
}