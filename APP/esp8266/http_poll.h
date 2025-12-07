#ifndef __HTTP_POLL_H
#define __HTTP_POLL_H

#include "system.h"

// HTTP轮询配置
#define POLL_SERVER_IP   "47.83.221.161"
#define POLL_SERVER_PORT 6637
#define POLL_PATH        "/api/control/poll"
#define ACK_PATH         "/api/control/ack"

// 命令结构
typedef struct {
    u8 beep;        // 0=off, 1=on, 2=change
    u8 door;        // 0=off, 1=on
    u8 has_command; // 是否有新命令
} ControlCommand_t;

// 函数声明
u8 HTTP_Poll_Commands(ControlCommand_t* cmd);
u8 HTTP_Ack_Commands(u8 beep_ack, u8 door_ack);

#endif
