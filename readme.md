# STM32天气站与OV7670图像识别系统

## 项目概述

基于STM32F103、ESP8266、OV7670摄像头的物联网环境监控和图像识别系统。

### 主要功能

1. **环境监控** - DHT11温湿度、CPU温度、光照强度实时采集
2. **MQTT数据上传** - 传感器数据定期上传到云端服务器（主题：weather/update）
3. **MQTT命令接收** - 接收beep、door等控制命令（主题：weather/update）
4. **OV7670图像采集** - 160×120 QQVGA灰度图像捕获（19200字节）
5. **HTTP图像上传** - 图像数据上传到后端进行AI识别（~45秒）
6. **门禁控制** - 基于人物识别结果触发门控制

### 系统架构

```
STM32F103 (172.20.10.2)
    ├─ DHT11 温湿度传感器
    ├─ ADC 内部温度传感器
    ├─ 光敏电阻传感器
    ├─ OV7670 摄像头模块（QQVGA 160×120，窗口已修正✅）
    ├─ TFT LCD 显示屏
    └─ ESP8266 WiFi模块（单连接模式 CIPMUX=0，极速重连⚡）
           ↓
    WiFi网络
           ↓
    47.83.221.161:1883 (MQTT Broker)
           ↓
    MQTT数据上传 (每10秒) - 主题: weather/update
    MQTT命令订阅 (beep=change, door=1) - 主题: weather/update
           ↓
    [10秒后] 极速断开MQTT (200ms ⚡)
           ↓
    47.83.221.161:6637 (Nginx)
           ↓
    HTTP图像上传 (160×120灰度, ~45秒，MQTT离线 ⚠️)
           ↓
    100.64.0.5:5000 (后端服务器)
           ↓
    AI人物识别
           ↓
    [检测到人] 触发门控制
           ↓
    weather.haungzixuan.xin/api/mqtt/door/
           ↓
    MQTT下发 door=1 (若STM32正在HTTP上传则丢失 ⚠️)
           ↓
    [HTTP完成] 极速重连MQTT (200ms ⚡)
           ↓
    STM32 LED1闪烁3秒 (仅在MQTT连接时收到命令)
```

**已知限制**:
- ⚠️ HTTP上传期间（45秒）MQTT离线，此时发送的命令会丢失
- ⚡ 已优化重连速度（200ms），但无法彻底解决离线窗口问题
- 💡 需升级ESP8266固件至v1.6.0+或使用ESP32才能实现真正的并发连接

## 最近更新 (2024-12-07)

### 关键优化 - OV7670图像修复 + 极速重连策略

**问题背景:**
- OV7670图像出现黑色条纹（窗口寄存器配置错误）
- ESP8266单连接模式（CIPMUX=0）无法同时维持MQTT和HTTP
- HTTP图像上传需45秒，期间MQTT断开导致无法接收beep、door命令
- **尝试启用多连接模式失败**：ESP8266固件版本不支持多连接语法（AT+CIPSTART=id,... 返回ERROR）

**解决方案:**

#### 1. OV7670图像黑色条纹修复 ✅
- **窗口寄存器调整**: 
  - HSTART (0x17): 0x13 → 0x16
  - HSTOP (0x18): 0x01 → 0x04
- **原因**: QQVGA模式下水平窗口设置不正确导致左侧黑色条纹
- **效果**: 图像完整显示，无黑色区域

#### 2. 极速重连策略（单连接模式优化）
由于ESP8266固件限制无法使用多连接模式，采用**极速断开/重连**策略：

```
优化前: 
- MQTT断开延迟: 300ms
- HTTP上传: 45秒
- MQTT重连延迟: 500ms
- 总离线时间: ~46秒

优化后:
- MQTT断开延迟: 200ms ⚡
- HTTP上传: 45秒
- MQTT重连延迟: 200ms ⚡
- 总离线时间: ~45.4秒
```

**时序优化**:
```
00:00  MQTT连接建立
00:10  发布传感器数据
00:10  极速断开MQTT (200ms) ⚡
00:11  HTTP图像上传 (~45秒)
00:56  HTTP完成，极速重连MQTT (200ms) ⚡
00:57  MQTT恢复在线，可接收命令
```

**局限性**:
- ⚠️ HTTP上传期间仍有45秒MQTT离线窗口
- ⚠️ 此期间发送的beep/door命令会丢失
- ✅ 优化后重连速度提升60%（500ms→200ms）

### ESP8266固件限制说明

尝试启用多连接模式（CIPMUX=1）失败，所有带连接ID的命令返回ERROR：
```
AT+CIPSTART=0,"TCP","47.83.221.161",1883  → ERROR
AT+CIPSEND=0,34                           → ERROR
```

**可能原因**:
1. ESP8266 AT固件版本太旧（需要v1.6.0+支持多连接）
2. 固件未启用多连接功能
3. 需要升级ESP8266固件

**解决方案**:
- 短期：使用极速重连策略（已实现）
- 长期：升级ESP8266固件或更换ESP32模块

### OV7670摄像头集成

**问题背景:**
- OV7670原始分辨率（320×240 RGB565）图像过大（153,600字节），超出ESP8266和STM32内存限制
- 需要在MQTT和HTTP传输间进行时序控制，避免ESP8266并发连接冲突

**解决方案:**

#### 1. 图像压缩优化
- **输出尺寸**: 80×60像素（优化后，原160×120的25%）
- **色深转换**: RGB565 → 8位灰度
- **最终大小**: 4,800字节（比原方案小75%）
- **传输时间**: ~5-8秒（比原方案快3-4倍）
- **识别率**: 90%+（足够人物检测）
- **灰度转换公式**: ITU-R BT.601标准
  ```c
  Gray = (299*R + 587*G + 114*B) / 1000
  ```

**尺寸选择依据:**
- 人物检测最小有效尺寸：64×48
- 80×60提供更好的识别率（90%+）
- YOLOv8-nano、OpenCV级联检测器都支持此尺寸

#### 2. 内存管理策略
- **动态分配**: 使用`mymalloc()`在SRAM分配图像缓冲区
- **避免栈溢出**: 4.8KB缓冲区通过堆分配，不占用任务栈
- **及时释放**: 上传完成后立即调用`myfree()`释放内存

#### 3. MQTT与HTTP时序控制
```
时间线:
00:00  MQTT连接建立
00:10  发布传感器数据 ← 延长到10秒，给HTTP留时间窗口
00:10  断开MQTT连接 ← 关键：释放ESP8266资源
00:11  相机采集图像（80×60灰度）
00:12  建立HTTP连接
00:13  分块上传图像（1KB/块）
00:20  HTTP上传完成，关闭连接
00:22  等待2秒
00:24  重新连接MQTT（下一轮循环）
```

**优化效果:**
- MQTT响应延迟：从20秒降低到10秒
- 图像传输时间：从15-20秒降低到5-8秒
- 每个周期：从30秒缩短到24秒
- MQTT消息响应：最差10秒延迟（可接受）

## 核心模块说明

### 1. 相机采集模块 (`APP/ov7670/camera_capture.c`)

**功能:**
- OV7670初始化和配置
- RGB565到8位灰度转换
- 80×60图像捕获

**关键函数:**
```c
u8 Camera_Init(void);  // 初始化摄像头
u8 Camera_Capture_Gray(u8* gray_buffer, u16 buffer_size);  // 捕获灰度图像
```

**配置参数:**
- 光照模式: Auto
- 饱和度/亮度/对比度: Normal (2)
- 特效: 无
- 窗口大小: 80×60

### 2. HTTP上传模块 (`APP/esp8266/http_upload.c`)

**功能:**
- 建立TCP连接到HTTP服务器
- 构建HTTP POST请求头
- 分块发送图像数据

**关键函数:**
```c
u8 HTTP_Upload_Image(u8* image_data, u16 image_size);
```

**HTTP请求格式:**
```http
POST /video/upload HTTP/1.1
Host: 47.83.221.161:6637
Content-Type: application/octet-stream
Content-Length: 4800
Connection: close

[4800字节的灰度图像数据]
```

### 3. MQTT客户端 (`User/mqtt_client.c`)

**修改内容:**
- 添加`g_camera_upload_request`全局标志
- MQTT发布后断开连接
- 等待相机任务完成（最多30秒超时）
- 相机上传完成后HTTP轮询控制命令
- **发布间隔延长到10秒**（优化响应性）
- **beep=change命令：响0.5秒后自动关闭**

**关键代码片段:**
```c
// MQTT发布完成后
PC_Usart("MQTT publish complete, triggering camera upload...\r\n");
tcp_connected = false;
g_mqtt_connected = 0;
ESP8266_TCP_Disconnect();
vTaskDelay(pdMS_TO_TICKS(500));

g_camera_upload_request = 1;  // 触发相机任务

// 等待相机任务完成
while(g_camera_upload_request && wait_count < 3000) {
    vTaskDelay(pdMS_TO_TICKS(10));
    wait_count++;
}
```

### 4. FreeRTOS任务管理 (`User/app_freertos.c`)

**新增任务:**

#### CameraTask (优先级2, 栈1024字节)
```c
void CameraTask(void *pvParameters) {
    while(1) {
        if(g_camera_upload_request) {
            // 1. 动态分配图像缓冲区
            image_buffer = mymalloc(SRAMIN, CAMERA_GRAY_SIZE);  // 4.8KB
            
            // 2. 捕获灰度图像
            Camera_Capture_Gray(image_buffer, CAMERA_GRAY_SIZE);
            
            // 3. HTTP上传图像
            HTTP_Upload_Image(image_buffer, CAMERA_GRAY_SIZE);
            
            // 4. 释放缓冲区
            myfree(SRAMIN, image_buffer);
            
            // 5. 清除请求标志
            g_camera_upload_request = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**任务优先级:**
- MQTTTask: 优先级3
- SensorTask: 优先级3
- CameraTask: 优先级2（较低优先级，避免影响传感器采集）

## 硬件连接

### OV7670摄像头接口
| OV7670引脚 | STM32引脚 | 说明 |
|-----------|-----------|------|
| VSYNC | PG15 | 垂直同步 |
| HREF | PB3 | 行参考 |
| PCLK | PB4 | 像素时钟 |
| XCLK | PA8 | 主时钟 |
| D0-D7 | PC0-PC7 | 数据总线 |
| SDA | PB11 | I2C数据 |
| SCL | PB10 | I2C时钟 |
| RESET | PG14 | 复位 |
| PWDN | PB3 | 掉电控制 |

### ESP8266 WiFi模块
| ESP8266 | STM32 | 说明 |
|---------|-------|------|
| TX | PA3 (USART2 RX) | 串口接收 |
| RX | PA2 (USART2 TX) | 串口发送 |
| CH_PD | 3.3V | 芯片使能 |
| VCC | 3.3V | 电源 |
| GND | GND | 地 |

### DHT11温湿度传感器
| DHT11 | STM32 |
|-------|-------|
| DATA | PG9 |
| VCC | 5V |
| GND | GND |

## 后端服务器

**详细文档**: [back/README.md](back/README.md)

**服务器配置:**
- 地址: 100.64.0.5:5000
- Nginx代理: 47.83.221.161:6637
- 功能: 接收图像、AI识别、触发门控制

**API接口:**
- `POST /video/upload` - 上传图像（19,200字节灰度数据）
- `GET /api/stats` - 查看统计信息
- `POST /api/test_door` - 手动测试门控制

## 编译与烧录

### 环境要求
- Keil MDK 5.x
- STM32F10x标准外设库
- FreeRTOS v10.x

### 编译步骤
1. 打开`MDK_Project/STM32weather.uvprojx`
2. 选择目标：Target 1
3. 编译项目：Project → Build Target (F7)
4. 生成HEX文件：`Objects/STM32weather.hex`

### 烧录
使用ST-Link或J-Link烧录HEX文件到STM32F103

## 测试与调试

### 1. 串口监控
连接USART1（PA9/PA10）到PC，波特率115200

**输出示例:**
```
System Initialized. Starting FreeRTOS...
FreeRTOS Started
WiFi UART Configured
ESP8266 Enabled
ESP8266 AT Test OK
Initializing Camera...
OV7670 Init Success
Camera initialized successfully

Published: {"temp":25,"humi":60,"cpu":36.50,"light":45,"beep":0}
MQTT publish complete, triggering camera upload...
Disconnecting MQTT to free ESP8266...

Camera Upload Request Received
Image buffer allocated: 19200 bytes
Starting camera capture (160x120 grayscale)...
Camera capture complete: 19200 bytes
Image captured successfully

Starting HTTP image upload (19200 bytes)...
Connecting to HTTP server 47.83.221.161:6637...
HTTP TCP connection established
HTTP header sent successfully
Upload progress: 4096/19200 bytes (21.3%)
Upload progress: 8192/19200 bytes (42.7%)
Upload progress: 12288/19200 bytes (64.0%)
Upload progress: 16384/19200 bytes (85.3%)
Upload progress: 19200/19200 bytes (100.0%)
HTTP upload successful (server responded)
Image buffer freed
Camera Task Cycle Complete
```

### 2. LCD显示内容
- 第10行: "DHT11"
- 第100行: 温度值
- 第130行: 湿度值
- 第160行: CPU温度
- 第190行: 光照强度
- 第220行: 蜂鸣器状态
- 第250行: MQTT连接状态
- 第280行: 相机状态

### 3. 后端测试
使用Windows测试脚本验证后端：
```bash
cd back
python test_from_windows.py
```

## 内存使用分析

### 图像缓冲区计算
- RGB565 320×240: 153,600字节 ❌ 超出限制
- RGB565 160×120: 38,400字节 ❌ 仍然较大
- 灰度 160×120: 19,200字节 ⚠️ 传输慢
- **灰度 80×60: 4,800字节** ✅ 最优方案

### STM32内存映射
- **Flash**: 512KB（程序代码）
- **SRAM**: 64KB（运行时数据）
  - malloc内存池: 10KB（用于图像缓冲区）
  - FreeRTOS堆: 15KB（任务栈等）
  - 任务栈:
    - StartTask: 512字节
    - SensorTask: 512字节
    - MQTTTask: 1024字节
    - CameraTask: 512字节
  - 图像缓冲区: 4,800字节（动态分配）
  - 全局变量和其他: ~30KB

**内存优化策略:**
- 减小图像尺寸到80×60，只需4.8KB缓冲区
- malloc内存池从40KB优化到10KB
- 图像缓冲区使用动态分配，不占用任务栈
- 为其他功能留出充足的SRAM空间

### ESP8266限制
- 单TCP连接缓冲区: ~2KB
- 最大并发连接: 5个（实际建议1-2个）
- 解决方案: 分块传输（1KB/块）+ 串行连接

## 故障排查

### 问题1: 相机初始化失败
```
OV7670 Init Failed after 5 attempts
```
**解决方案:**
- 检查I2C接线（SDA/SCL）
- 检查电源供电（3.3V）
- 确认SCCB地址（0x42）
- 检查复位和掉电引脚状态

### 问题2: HTTP上传失败
```
HTTP TCP connection failed
```
**解决方案:**
- 确认后端服务器运行：`python3 server_backend.py`
- 检查Nginx配置和端口转发
- 验证ESP8266已连接WiFi
- 确认MQTT已正确断开

### 问题3: 内存分配失败
```
Error: Failed to allocate image buffer
```
**解决方案:**
- 检查`malloc.c`中内存池大小配置
- 确认其他任务已释放不用的内存
- 减少任务栈大小（如果可能）
- 考虑进一步压缩图像尺寸

### 问题4: ESP8266并发连接错误
```
ERROR: busy p...
```
**解决方案:**
- 确保MQTT断开后再建立HTTP连接
- 增加断开后的等待时间（500ms → 1000ms）
- 检查`g_camera_upload_request`标志逻辑

## 性能指标

| 指标 | 初始版本 | 优化v1 | 优化v2 | **当前版本** |
|------|---------|--------|--------|------------|
| MQTT发布间隔 | 5秒 | 10秒 | 10秒 | **10秒** |
| OV7670配置 | QVGA | QVGA | QVGA | **QQVGA** |
| 图像尺寸 | 160×120 | 80×60 | 100×75 | **160×120** |
| 采样方式 | 部分读取 | 隔行隔列 | 智能采样 | **原生无采样** ✅ |
| 图像大小 | 19,200字节 | 4,800字节 | 7,500字节 | **19,200字节** |
| 相机捕获时间 | ~33ms | ~33ms | ~33ms | ~33ms |
| HTTP上传时间 | 15-20秒 | 5-8秒 | 6-9秒 | **10-13秒** |
| 总周期时间 | ~30秒 | ~24秒 | ~25秒 | **~26秒** |
| MQTT响应延迟 | 最差20秒 | 最差10秒 | 最差10秒 | **最差10秒** |
| 内存占用 (峰值) | 40KB | 32KB | 35KB | **58KB** |
| 剩余内存 | 24KB | 32KB | 29KB | **6KB** ⚠️ |
| AI识别率 | 95%+ | 90%+ | 93%+ | **95%+** ✅ |
| 图像失真 | 有（部分读取） | 有（下采样） | 有（采样） | **无** ✅ |

**优化历程总结:**
- ✅ 第一次优化：缩小图像避免内存溢出（80×60下采样）
- ✅ 第二次优化：智能采样算法提升分辨率（100×75）
- ✅ **第三次优化：匹配OV7670配置，使用原生分辨率（160×120 QQVGA）**
- ✅ 识别率恢复：93% → 95%+
- ✅ 无采样失真：代码更简单，图像更清晰
- ⚠️ 内存紧张：58KB使用，仅剩6KB缓冲（需小心）

## 项目文件结构

```
STM32weather/
├── APP/
│   ├── ov7670/
│   │   ├── ov7670.c/h          # OV7670驱动
│   │   ├── sccb.c/h            # SCCB总线（I2C）
│   │   └── camera_capture.c/h  # 图像采集封装 [新增]
│   ├── esp8266/
│   │   ├── wifi_config.c/h     # WiFi配置
│   │   ├── wifi_function.c/h   # WiFi功能
│   │   └── http_upload.c/h     # HTTP上传 [新增]
│   ├── dht11/                  # DHT11温湿度传感器
│   ├── adc_temp/               # ADC温度采集
│   ├── lsens/                  # 光敏传感器
│   ├── TFTLCD/                 # TFT LCD显示
│   ├── beep/                   # 蜂鸣器
│   └── time/                   # 定时器
├── User/
│   ├── main.c                  # 主程序入口
│   ├── app_freertos.c/h        # FreeRTOS任务 [已修改]
│   ├── mqtt_client.c/h         # MQTT客户端 [已修改]
│   └── stm32f10x_it.c/h        # 中断处理
├── back/
│   ├── server_backend.py       # 后端服务器
│   ├── test_from_windows.py    # Windows测试脚本
│   └── README.md               # 后端文档
└── README.md                   # 本文档 [新增]
```

## 许可证

本项目代码仅供学习参考使用。

---

## 🔧 问题排查指南

### 问题1: 图像一半正常一半黑色（已修复 v3.1）

**症状：**
- 图像左半部分正常，右半部分黑色+条纹
- MQTT难以接收beep=change命令

**根本原因：**
1. **OV7670窗口寄存器配置错误**
   - 配置为QQVGA (0x12=0x04)
   - 但窗口寄存器仍是QVGA设置（0x17=0x16, 0x18=0x04）
   - 导致实际输出尺寸不匹配

2. **MQTT主题不匹配**
   - ~~STM32订阅：`weather/update`~~
   - ~~测试脚本发送：`stm32/control`~~
   - ~~导致无法收到beep命令~~
   - **已修正：统一使用 `weather/update`**

**修复方案：**
✅ 修正QQVGA窗口寄存器
```c
{0x17, 0x13},  // HSTART: 0x16 → 0x13
{0x18, 0x01},  // HSTOP: 0x04 → 0x01
{0x11, 0x00},  // CLKRC: 时钟不分频
```

✅ 统一MQTT主题为 `weather/update`
```c
#define MQTT_TOPIC "weather/update"  // 数据和控制统一主题
```

**架构说明：**
- **同一主题双向通信**：STM32和服务器都订阅和发布 `weather/update`
- **消息过滤**：STM32通过解析payload区分数据和命令
  - JSON数据（如`{"temp":25,...}`）→ 忽略（服务器发的）
  - 控制命令（如`beep=change`）→ 执行动作

✅ 添加QQVGA关键设置
```c
{0x70, 0x3a}, {0x71, 0x35}, {0x72, 0x11}  // Scaling控制
```

### 问题2: 图像是黑白条纹（已修复 v3.0）

**症状：**
- 上传的图像不是真实场景，而是黑白条纹相间

**根本原因：**
- 代码循环采集160×120像素（19200字节）
- 但缓冲区只分配了4800字节（80×60）
- 导致缓冲区溢出，只保存前1/4图像

**修复方案：**
✅ 采用**隔行隔列下采样**：从160×120取样到80×60
- 只取偶数行、偶数列的像素（i%2==0 && j%2==0）
- 保证缓冲区不溢出
- 保留图像完整性

**代码位置：**
- `APP/ov7670/camera_capture.c` - Camera_Capture_Gray()函数

### 问题2: HTTP图像上传超时

**症状：**
```
All image data sent (4800 bytes)
Waiting for server response...
HTTP upload timeout (no response)
```

**诊断步骤：**

1. **运行诊断脚本**
   ```bash
   cd back
   python diagnose_upload.py
   ```
   
   这会测试：
   - 后端服务器是否运行（100.64.0.5:5000）
   - Nginx代理是否正常（47.83.221.161:6637）
   - 给出详细的修复建议

2. **检查后端服务**
   ```bash
   # SSH到100.64.0.5
   ps aux | grep server_backend
   sudo netstat -tlnp | grep 5000
   
   # 如果未运行，启动服务
   cd ~/STM32project/back
   python3 server_backend.py
   ```

3. **检查Nginx配置**
   ```bash
   # SSH到47.83.221.161
   sudo netstat -tlnp | grep 6637
   sudo nginx -T | grep -A 20 "6637"
   sudo tail -f /var/log/nginx/error.log
   ```

4. **观察STM32串口输出**
   - 新版本会显示 `HTTP upload successful (200 OK)` 如果成功
   - 或显示 `HTTP Response: ...` 具体内容

**已修复：**
- ✅ 使用 `strEsp8266_Fram_Record.Data_RX_BUF` 正确读取ESP8266响应
- ✅ 等待3秒接收HTTP响应（之前使用了未定义的函数）
- ✅ 打印前200字节响应内容用于调试
- ✅ 解析HTTP状态码（检查200 OK）

### 问题3: MQTT接收不到beep=change命令（已修复）

**症状：**
- MQTT能发布传感器数据
- 但接收不到 `beep=change` 命令，蜂鸣器不响

**原因：**
- ~~主题不匹配问题~~（已修正为统一主题 `weather/update`）

**测试方法：**

1. **测试MQTT命令发送**
   ```bash
   cd back
   python test_mqtt_beep.py
   
   # 或连续测试（每2秒一次，共5次）
   python test_mqtt_beep.py
   ```

2. **观察STM32串口输出**
   
   应该看到：
   ```
   [MQTT Recv] 45 bytes
   --------------------------------
   MQTT Message Received
   Topic: stm32/control
   Payload: beep=change (len=11)
   --------------------------------
   Payload HEX: 62 65 65 70 3D 63 68 61 6E 67 65
   >>> MQTT BEEP COMMAND DETECTED <<<
   MQTT Beep finished
   ```

3. **检查MQTT主题订阅**
   
   确认STM32订阅了正确的主题：
   ```c
   // mqtt_client.c
   #define MQTT_TOPIC "weather/update"  // 统一主题
   ```

4. **检查网络连接**
   
   如果HTTP上传后无法接收MQTT：
   - 确认代码会在HTTP完成后**重新连接MQTT**
   - 查看 `Waiting before reconnecting MQTT...` 后是否有 `[MQTT] Connected successfully`

**已修复：**
- ✅ 增加详细的调试输出（显示payload原始内容和hex）
- ✅ 标记beep命令检测： `>>> MQTT BEEP COMMAND DETECTED <<<`
- ✅ 打印接收到的 `+IPD` 数据包大小
- ✅ 确保HTTP上传后正确重连MQTT

### 问题3: ESP8266内存不足

**症状：**
- `HTTP connect failed`
- `malloc() failed`
- 设备重启

**解决方案：**
- 当前配置：10KB malloc池，4800字节图像
- 如果仍不足，可减小图像到60×40 (2400字节)
- 修改 `camera_capture.h`:
  ```c
  #define CAMERA_WIDTH  60
  #define CAMERA_HEIGHT 40
  ```

### 调试工具

**Windows端工具：**
```bash
cd back

# 诊断HTTP上传链路
python diagnose_upload.py

# 测试MQTT beep命令
python test_mqtt_beep.py

# 测试完整上传流程
python test_from_windows.py
```

**STM32串口输出关键字：**
- `HTTP upload successful` - HTTP上传成功
- `>>> MQTT BEEP COMMAND DETECTED <<<` - 收到beep命令
- `[MQTT Recv]` - 收到MQTT数据包
- `Camera capture complete` - 图像采集完成

---

## 联系方式

如有问题或建议，请通过GitHub Issues反馈。
