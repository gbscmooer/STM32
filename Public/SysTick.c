#include "stm32f10x.h"
#include "SysTick.h"
#include "FreeRTOS.h"
#include "task.h"

// Manually define DWT if not present in core_cm3.h
#ifndef DWT
typedef struct
{
  __IO uint32_t CTRL;
  __IO uint32_t CYCCNT;
  __IO uint32_t CPICNT;
  __IO uint32_t EXCCNT;
  __IO uint32_t SLEEPCNT;
  __IO uint32_t LSUCNT;
  __IO uint32_t FOLDCNT;
  __IO uint32_t PCSR;
} DWT_Type;

#define DWT_BASE            0xE0001000UL
#define DWT                 ((DWT_Type *) DWT_BASE)

#define DWT_CTRL_CYCCNTENA_Pos  0
#define DWT_CTRL_CYCCNTENA_Msk  (1UL << DWT_CTRL_CYCCNTENA_Pos)
#endif

static u8  fac_us=0;							//us延时倍乘数			   
static u16 fac_ms=0;							//ms延时倍乘数


//初始化延迟函数
//SYSTICK的时钟固定为AHB时钟的1/8
//SYSCLK:系统时钟频率
void SysTick_Init(u8 SYSCLK)
{
    // FreeRTOS will configure SysTick, so we don't need to configure it here for the OS.
    // However, we need to enable DWT for delay_us.
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    
	fac_us=SYSCLK; // SystemCoreClock is in MHz (e.g., 72)
}								    


//延时nus
//nus为要延时的us数.		    								   
void delay_us(u32 nus)
{		
    u32 ticks;
    u32 told, tnow, tcnt = 0;
    u32 reload = SysTick->LOAD;
    
    ticks = nus * fac_us; // fac_us is SystemCoreClock in MHz
    told = DWT->CYCCNT;
    
    while(1)
    {
        tnow = DWT->CYCCNT;
        if(tnow != told)
        {
            if(tnow > told) tcnt += tnow - told;
            else tcnt += (0xFFFFFFFF - told) + tnow;
            told = tnow;
            if(tcnt >= ticks) break;
        }
    }
}

//延时nms
//注意nms的范围
//SysTick->LOAD为24位寄存器,所以,最大延时为:
//nms<=0xffffff*8*1000/SYSCLK
//SYSCLK单位为Hz,nms单位为ms
//对72M条件下,nms<=1864 
void delay_ms(u16 nms)
{	 		  	  
	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
	{
		vTaskDelay(pdMS_TO_TICKS(nms));
	}
	else
	{
		u32 temp;		   
		SysTick->LOAD=(u32)nms*fac_ms;				//时间加载(SysTick->LOAD为24bit)
		SysTick->VAL =0x00;							//清空计数器
		SysTick->CTRL|=SysTick_CTRL_ENABLE_Msk ;	//开始倒数  
		do
		{
			temp=SysTick->CTRL;
		}while((temp&0x01)&&!(temp&(1<<16)));		//等待时间到达   
		SysTick->CTRL&=~SysTick_CTRL_ENABLE_Msk;	//关闭计数器
		SysTick->VAL =0X00;							//清空计数器
	}
}


