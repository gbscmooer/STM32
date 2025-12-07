#ifndef __HTTP_UPLOAD_H
#define __HTTP_UPLOAD_H

#include "system.h"

// HTTP上传配置
#define HTTP_SERVER_IP   "47.83.221.161"
#define HTTP_SERVER_PORT 6637
#define HTTP_UPLOAD_PATH "/video/upload"

// 分块传输配置（根据ESP8266缓冲区限制）
#define HTTP_CHUNK_SIZE  1024  // 每次发送1KB

// 函数声明
u8 HTTP_Upload_Image(u8* image_data, u16 image_size);

#endif
