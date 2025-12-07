#include "http_upload.h"
#include "wifi_function.h"
#include "usart.h"
#include "SysTick.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

/*******************************************************************************
* 函 数 名: HTTP_Connect
* 功能说明: 连接到HTTP服务器
* 输    入: 无
* 输    出: 0-成功, 1-失败
*******************************************************************************/
static u8 HTTP_Connect(void)
{
    char cmd[64];
    
    PC_Usart("Connecting to HTTP server %s:%d...\r\n", HTTP_SERVER_IP, HTTP_SERVER_PORT);
    
    // 建立TCP连接
    sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d", HTTP_SERVER_IP, HTTP_SERVER_PORT);
    
    if(ESP8266_Cmd(cmd, "OK", "ERROR", 5000))
    {
        PC_Usart("HTTP TCP connection established\r\n");
        return 0;
    }
    else
    {
        PC_Usart("HTTP TCP connection failed\r\n");
        return 1;
    }
}

/*******************************************************************************
* 函 数 名: HTTP_Disconnect
* 功能说明: 断开HTTP服务器连接
* 输    入: 无
* 输    出: 无
*******************************************************************************/
static void HTTP_Disconnect(void)
{
    ESP8266_Cmd("AT+CIPCLOSE", "OK", "ERROR", 2000);
    PC_Usart("HTTP connection closed\r\n");
}

/*******************************************************************************
* 函 数 名: HTTP_Upload_Image
* 功能说明: 上传图像到HTTP服务器
* 输    入: image_data - 图像数据（8位灰度）
*           image_size - 图像大小（字节）
* 输    出: 0-成功, 1-失败
* 说    明: 使用分块传输避免ESP8266缓冲区溢出
*******************************************************************************/
u8 HTTP_Upload_Image(u8* image_data, u16 image_size)
{
    char http_header[256];
    u16 offset = 0;
    u16 bytes_to_send;
    u16 total_sent = 0;
    u8 result = 1;
    
    PC_Usart("Starting HTTP image upload (%d bytes)...\r\n", image_size);
    
    // 连接到服务器
    if(HTTP_Connect() != 0)
    {
        return 1;
    }
    
    // 构建HTTP请求头
    sprintf(http_header,
            "POST %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n",
            HTTP_UPLOAD_PATH,
            HTTP_SERVER_IP,
            HTTP_SERVER_PORT,
            image_size);
    
    u16 header_len = strlen(http_header);
    PC_Usart("HTTP Header (%d bytes):\r\n%s", header_len, http_header);
    
    // 发送HTTP头
    if(!ESP8266_TCP_Send((u8*)http_header, header_len))
    {
        PC_Usart("Failed to send HTTP header\r\n");
        HTTP_Disconnect();
        return 1;
    }
    
    PC_Usart("HTTP header sent successfully\r\n");
    
    // 分块发送图像数据
    PC_Usart("Sending image data in chunks...\r\n");
    
    while(offset < image_size)
    {
        // 计算本次发送的字节数
        bytes_to_send = (image_size - offset) > HTTP_CHUNK_SIZE ? 
                        HTTP_CHUNK_SIZE : (image_size - offset);
        
        // 发送数据块
        if(!ESP8266_TCP_Send(&image_data[offset], bytes_to_send))
        {
            PC_Usart("Failed to send chunk at offset %d\r\n", offset);
            HTTP_Disconnect();
            return 1;
        }
        
        offset += bytes_to_send;
        total_sent += bytes_to_send;
        
        // 打印进度（每4KB打印一次）
        if(total_sent % 4096 == 0 || total_sent == image_size)
        {
            PC_Usart("Upload progress: %d/%d bytes (%.1f%%)\r\n", 
                     total_sent, image_size, 
                     (total_sent * 100.0) / image_size);
        }
    }
    
    PC_Usart("All image data sent (%d bytes)\r\n", total_sent);
    
    // 等待服务器响应
    PC_Usart("Waiting for server response...\r\n");
    
    // 等待ESP8266接收HTTP响应
    u8 response_received = 0;
    
    // 等待3秒接收响应
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 检查接收缓冲区
    extern struct STRUCT_USARTx_Fram strEsp8266_Fram_Record;
    
    if(strEsp8266_Fram_Record.InfBit.FramLength > 0)
    {
        // 打印收到的响应（前200字节）
        char temp_buf[201];
        u16 print_len = strEsp8266_Fram_Record.InfBit.FramLength > 200 ? 200 : strEsp8266_Fram_Record.InfBit.FramLength;
        memcpy(temp_buf, strEsp8266_Fram_Record.Data_RX_BUF, print_len);
        temp_buf[print_len] = '\0';
        PC_Usart("HTTP Response: %s\r\n", temp_buf);
        
        // 检查是否包含HTTP/1.1 200或HTTP/1.0 200
        if(strstr(strEsp8266_Fram_Record.Data_RX_BUF, "HTTP/1.1 200") != NULL ||
           strstr(strEsp8266_Fram_Record.Data_RX_BUF, "HTTP/1.0 200") != NULL)
        {
            PC_Usart("HTTP upload successful (200 OK)\r\n");
            response_received = 1;
            result = 0;
        }
        else if(strstr(strEsp8266_Fram_Record.Data_RX_BUF, "HTTP") != NULL)
        {
            PC_Usart("HTTP upload failed (non-200 response)\r\n");
            response_received = 1;
            result = 1;
        }
        
        // 清空接收缓冲区
        strEsp8266_Fram_Record.InfBit.FramLength = 0;
        strEsp8266_Fram_Record.InfBit.FramFinishFlag = 0;
    }
    
    if(!response_received)
    {
        PC_Usart("HTTP upload timeout (no response)\r\n");
        result = 1;
    }
    
    // 关闭连接
    HTTP_Disconnect();
    
    return result;
}
