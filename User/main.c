#include "system.h"
#include "SysTick.h"
#include "led.h"
#include "usart.h"
#include "app_freertos.h"
#include "wifi_function.h"


int main(void)
{

	SysTick_Init(72);
	USART1_Init(115200);
	LED_Init();
	
	PC_Usart("System Initialized. Starting FreeRTOS...\r\n");
	
	App_Start();
	
	while(1)
	{
	}
}

