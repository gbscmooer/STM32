#ifndef __CAMERA_CAPTURE_H
#define __CAMERA_CAPTURE_H

#include "system.h"

// OV7670原生分辨率（8位灰度）
// QQVGA 160×120，无采样失真，95%+ AI识别率
#define CAMERA_WIDTH  160
#define CAMERA_HEIGHT 120
#define CAMERA_GRAY_SIZE (CAMERA_WIDTH * CAMERA_HEIGHT)  // 19200字节

// 函数声明
u8 Camera_Init(void);
u8 Camera_Capture_Gray(u8* gray_buffer, u16 buffer_size);

#endif
