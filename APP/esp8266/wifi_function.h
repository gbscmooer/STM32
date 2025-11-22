#ifndef __WIFI_FUNCTION_H
#define	__WIFI_FUNCTION_H 


#include "system.h"
#include "wifi_config.h"
#include <stdbool.h>

// �������ݽṹ
typedef struct {
	char location[32];       // λ��
	char temperature[16];    // �¶�
	char humidity[16];       // ʪ��
	char weather[32];        // ����״��
	char description[64];    // ��ϸ����
	char last_update[32];    // ������ʱ��
	bool valid;              // ������Ч��
} WeatherData;


#define     ESP8266_Usart( fmt, ... )           USART3_printf (USART3, fmt, ##__VA_ARGS__ ) 
#define     PC_Usart( fmt, ... )                printf ( fmt, ##__VA_ARGS__ )

#define     ESP8266_CH_HIGH_LEVEL()             GPIO_SetBits( GPIOA, GPIO_Pin_4 )
#define     ESP8266_CH_LOW_LEVEL()              GPIO_ResetBits( GPIOA, GPIO_Pin_4 )

#define     ESP8266_RST_HIGH_LEVEL()            GPIO_SetBits( GPIOA, GPIO_Pin_15 )
#define     ESP8266_RST_LOW_LEVEL()             GPIO_ResetBits( GPIOA, GPIO_Pin_15 )

void        ESP8266_Scan_AP                     ( void );
void        ESP8266_Choose                      ( FunctionalState enumChoose );
void        ESP8266_Rst                         ( void );
void        ESP8266_AT_Test                     ( void );
bool        ESP8266_Cmd                         ( char * cmd, char * reply1, char * reply2, u32 waittime );
bool        ESP8266_Net_Mode_Choose             ( ENUM_Net_ModeTypeDef enumMode );
bool        ESP8266_JoinAP                      ( char * pSSID, char * pPassWord );
bool        ESP8266_BuildAP                     ( char * pSSID, char * pPassWord, char * enunPsdMode );
bool        ESP8266_Enable_MultipleId           ( FunctionalState enumEnUnvarnishTx );
bool        ESP8266_Link_Server                 ( ENUM_NetPro_TypeDef enumE, char * ip, char * ComNum, ENUM_ID_NO_TypeDef id);
bool        ESP8266_StartOrShutServer           ( FunctionalState enumMode, char * pPortNum, char * pTimeOver );
bool        ESP8266_SendString                  ( FunctionalState enumEnUnvarnishTx, char * pStr, u32 ulStrLength, ENUM_ID_NO_TypeDef ucId );
char *      ESP8266_ReceiveString               ( FunctionalState enumEnUnvarnishTx );

void        ESP8266_STA_TCP_Client              ( void );
void        ESP8266_AP_TCP_Server               ( void );
void        ESP8266_StaTcpClient_ApTcpServer    ( void );

bool        WiFi_Connect_Default                ( void );
bool        WiFi_Connect_Menu                   ( void );
bool        WiFi_Ping_Test                      ( char *target );
bool        ESP8266_HTTP_Request                ( char *host, u16 port, char *path, char *response, u16 max_len );
bool        ESP8266_HTTP_Post                   ( char *host, u16 port, char *path, char *body, char *response, u16 max_len );
bool        ESP8266_TCP_Connect                 ( char *ip, u16 port );
bool        ESP8266_TCP_Send                    ( char *data, u16 len );
bool        ESP8266_TCP_Disconnect              ( void );
bool        Weather_Get_Data                    ( WeatherData *weather );
void        Weather_Display                     ( WeatherData *weather );
void        WiFi_Set_Log                        ( bool enable );
void        ESP8266_Simple_TCP_Test             ( void );
bool        Weather_Update_Location             ( void );


#endif    /* __WIFI_FUNCTION_H */





