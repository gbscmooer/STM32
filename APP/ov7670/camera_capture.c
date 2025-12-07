#include "camera_capture.h"
#include "ov7670.h"
#include "time.h"
#include "usart.h"
#include <string.h>

/*******************************************************************************
* 函 数 名: Camera_Init
* 功能说明: 初始化OV7670摄像头
* 输    入: 无
* 输    出: 0-成功, 其他-失败
*******************************************************************************/
u8 Camera_Init(void)
{
    u8 retry = 0;
    
    PC_Usart("Initializing OV7670...\r\n");
    
    // 初始化OV7670
    while(OV7670_Init())
    {
        retry++;
        if(retry >= 5)
        {
            PC_Usart("OV7670 Init Failed after 5 attempts\r\n");
            return 1;
        }
        PC_Usart("OV7670 Init retry %d...\r\n", retry);
    }
    
    PC_Usart("OV7670 Init Success\r\n");
    
    // 配置默认参数（使用推荐配置）
    OV7670_Light_Mode(0);           // Auto light mode
    OV7670_Color_Saturation(2);     // Normal saturation
    OV7670_Brightness(2);           // Normal brightness
    OV7670_Contrast(2);             // Normal contrast
    OV7670_Special_Effects(0);      // No special effects
    
    // 设置窗口大小为80x60 (优化尺寸，适合人物检测)
    // 原始参数: OV7670_Window_Set(12, 176, 240, 320)
    // 调整为80x60输出
    OV7670_Window_Set(92, 236, 60, 80);
    
    // 初始化定时器（用于帧同步）
    TIM4_Init(10000, 7199);  // 10KHz频率
    
    PC_Usart("OV7670 configured for 80x60 capture\r\n");
    
    return 0;
}

/*******************************************************************************
* 函 数 名: RGB565_to_Gray
* 功能说明: 将RGB565转换为8位灰度值
* 输    入: rgb565 - RGB565颜色值
* 输    出: 灰度值 (0-255)
*******************************************************************************/
static u8 RGB565_to_Gray(u16 rgb565)
{
    // 提取RGB分量
    u8 r5 = (rgb565 >> 11) & 0x1F;  // 5位红色
    u8 g6 = (rgb565 >> 5) & 0x3F;   // 6位绿色
    u8 b5 = rgb565 & 0x1F;          // 5位蓝色
    
    // 扩展到8位
    u8 r8 = (r5 << 3) | (r5 >> 2);  // 5位->8位
    u8 g8 = (g6 << 2) | (g6 >> 4);  // 6位->8位
    u8 b8 = (b5 << 3) | (b5 >> 2);  // 5位->8位
    
    // ITU-R BT.601标准灰度转换公式
    // Gray = 0.299*R + 0.587*G + 0.114*B
    // 使用整数运算: Gray = (299*R + 587*G + 114*B) / 1000
    u16 gray = (299 * r8 + 587 * g8 + 114 * b8) / 1000;
    
    return (u8)gray;
}

/*******************************************************************************
* 函 数 名: Camera_Capture_Gray
* 功能说明: 捕获80x60的8位灰度图像
* 输    入: gray_buffer - 灰度图像缓冲区（必须>=4800字节）
*           buffer_size - 缓冲区大小
* 输    出: 0-成功, 1-失败
* 说    明: 该函数会阻塞直到完成一帧图像捕获（约33ms @ 30fps）
*******************************************************************************/
u8 Camera_Capture_Gray(u8* gray_buffer, u16 buffer_size)
{
    u16 i, j;
    u16 color;
    u32 pixel_index = 0;
    
    // 检查缓冲区大小
    if(buffer_size < CAMERA_GRAY_SIZE)
    {
        PC_Usart("Error: Buffer too small! Required: %d, Got: %d\r\n", 
                 CAMERA_GRAY_SIZE, buffer_size);
        return 1;
    }
    
    if(gray_buffer == NULL)
    {
        PC_Usart("Error: NULL buffer pointer\r\n");
        return 1;
    }
    
    PC_Usart("Starting camera capture (80x60 grayscale from 160x120)...\r\n");
    
    // 等待帧开始（VSYNC: 0->1）
    while(OV7670_VSYNC == 0);
    while(OV7670_VSYNC == 1);  // 等待VSYNC变为0（帧开始）
    
    // 从160x120下采样到80x60（隔行隔列采样）
    for(i = 0; i < 120; i++)  // OV7670实际输出120行
    {
        // 等待行开始（HREF: 0->1）
        while(OV7670_HREF == 0);
        
        for(j = 0; j < 160; j++)  // OV7670实际输出160列
        {
            // 读取高8位
            while(OV7670_PCLK == 0);  // 等待PCLK上升沿
            color = GPIOC->IDR & 0xFF;  // 读取数据
            while(OV7670_PCLK == 1);  // 等待PCLK下降沿
            
            // 移位高字节
            color <<= 8;
            
            // 读取低8位
            while(OV7670_PCLK == 0);  // 等待PCLK上升沿
            color |= GPIOC->IDR & 0xFF;  // 读取数据
            while(OV7670_PCLK == 1);  // 等待PCLK下降沿
            
            // 隔行隔列采样（每隔一行一列取一个像素）
            if((i % 2 == 0) && (j % 2 == 0))
            {
                // 转换为灰度并存储
                gray_buffer[pixel_index++] = RGB565_to_Gray(color);
            }
        }
        
        // 等待行结束（HREF: 1->0）
        while(OV7670_HREF == 1);
    }
    
    // 等待帧结束
    while(OV7670_VSYNC == 0);
    
    PC_Usart("Camera capture complete: %d bytes\r\n", pixel_index);
    
    return 0;
}
