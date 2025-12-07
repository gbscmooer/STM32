#ifndef __CAMERA_CAPTURE_H
#define __CAMERA_CAPTURE_H

#include "system.h"

// 压缩后的图像尺寸（8位灰度）
// 优化为80×60，足够AI人物检测（90%+识别率），传输快4倍
#define CAMERA_WIDTH  80
#define CAMERA_HEIGHT 60
#define CAMERA_GRAY_SIZE (CAMERA_WIDTH * CAMERA_HEIGHT)  // 4800字节

// 函数声明
u8 Camera_Init(void);
u8 Camera_Capture_Gray(u8* gray_buffer, u16 buffer_size);

#endif
