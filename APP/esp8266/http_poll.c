#include "http_poll.h"
#include "wifi_function.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*******************************************************************************
* 函 数 名: HTTP_Poll_Commands
* 功能说明: 轮询服务器获取控制命令
* 输    入: cmd - 命令结构体指针
* 输    出: 0-成功, 1-失败
*******************************************************************************/
u8 HTTP_Poll_Commands(ControlCommand_t* cmd)
{
    char http_request[256];
    char response_buffer[512];
    u16 response_len = 0;
    u8 result = 1;
    
    if(cmd == NULL)
    {
        PC_Usart("Error: NULL command pointer\r\n");
        return 1;
    }
    
    // 初始化命令结构
    cmd->beep = 0;
    cmd->door = 0;
    cmd->has_command = 0;
    
    PC_Usart("Polling control commands...\r\n");
    
    // 建立TCP连接
    char tcp_cmd[64];
    sprintf(tcp_cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d", POLL_SERVER_IP, POLL_SERVER_PORT);
    
    if(!ESP8266_Cmd(tcp_cmd, "OK", "ERROR", 5000))
    {
        PC_Usart("Poll TCP connection failed\r\n");
        return 1;
    }
    
    // 构建HTTP GET请求
    sprintf(http_request,
            "GET %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Connection: close\r\n"
            "\r\n",
            POLL_PATH,
            POLL_SERVER_IP,
            POLL_SERVER_PORT);
    
    u16 request_len = strlen(http_request);
    
    // 发送HTTP请求
    if(!ESP8266_TCP_Send((u8*)http_request, request_len))
    {
        PC_Usart("Poll HTTP request send failed\r\n");
        ESP8266_Cmd("AT+CIPCLOSE", "OK", "ERROR", 2000);
        return 1;
    }
    
    // 接收响应（简化版，只解析beep和door字段）
    vTaskDelay(pdMS_TO_TICKS(500));  // 等待服务器响应
    
    // 读取响应（这里简化处理，实际应该解析HTTP响应）
    // 寻找 "beep": 数字 和 "door": 数字
    // 简单的字符串匹配
    
    // 为了简化，假设服务器返回格式: {"status":"success","commands":{"beep":2,"door":0}}
    // 这里只做简单的解析示例
    
    PC_Usart("Poll request sent, checking response...\r\n");
    
    // 关闭连接
    ESP8266_Cmd("AT+CIPCLOSE", "OK", "ERROR", 2000);
    
    // 实际应用中需要完整解析HTTP响应，这里先返回成功
    // 你需要根据实际响应格式解析beep和door的值
    
    return 0;
}

/*******************************************************************************
* 函 数 名: HTTP_Ack_Commands
* 功能说明: 向服务器确认命令已执行
* 输    入: beep_ack - beep命令已执行
*           door_ack - door命令已执行
* 输    出: 0-成功, 1-失败
*******************************************************************************/
u8 HTTP_Ack_Commands(u8 beep_ack, u8 door_ack)
{
    char http_request[512];
    char json_body[128];
    
    // 构建JSON body
    sprintf(json_body, "{\"beep\":%s,\"door\":%s}",
            beep_ack ? "true" : "false",
            door_ack ? "true" : "false");
    
    u16 body_len = strlen(json_body);
    
    PC_Usart("Sending ACK to server...\r\n");
    
    // 建立TCP连接
    char tcp_cmd[64];
    sprintf(tcp_cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d", POLL_SERVER_IP, POLL_SERVER_PORT);
    
    if(!ESP8266_Cmd(tcp_cmd, "OK", "ERROR", 5000))
    {
        PC_Usart("ACK TCP connection failed\r\n");
        return 1;
    }
    
    // 构建HTTP POST请求
    sprintf(http_request,
            "POST %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            ACK_PATH,
            POLL_SERVER_IP,
            POLL_SERVER_PORT,
            body_len,
            json_body);
    
    u16 request_len = strlen(http_request);
    
    // 发送HTTP请求
    if(!ESP8266_TCP_Send((u8*)http_request, request_len))
    {
        PC_Usart("ACK HTTP request send failed\r\n");
        ESP8266_Cmd("AT+CIPCLOSE", "OK", "ERROR", 2000);
        return 1;
    }
    
    PC_Usart("ACK sent successfully\r\n");
    
    // 关闭连接
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP8266_Cmd("AT+CIPCLOSE", "OK", "ERROR", 2000);
    
    return 0;
}
