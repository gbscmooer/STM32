# STM32 天气监测系统使用指南

## 功能概述

本系统通过ESP8266 WiFi模块连接互联网，定时从服务器获取天气数据并显示。

### 主要功能
- ? 自动连接WiFi（默认：OPPOx）
- ? WiFi连接失败时提供菜单选择
- ? 网络连通性测试（Ping）
- ? HTTP天气数据获取
- ? JSON数据解析
- ? 每60秒自动更新天气信息
- ? LED状态指示

## 硬件连接

### ESP8266模块（USART3）
- TX → PA10
- RX → PA9
- CH_EN → PA4（使能控制）
- RST → PA15（复位控制）
- 波特率：115200

### 调试串口（USART1）
- TX → PA9
- RX → PA10
- 波特率：115200

### LED指示灯
- LED1（红色）→ PB5：错误/警告状态
- LED2（绿色）→ PE5：正常/成功状态

## 系统运行流程

### 1. 初始化阶段
```
系统启动 → LED闪烁测试 → USART初始化 → WiFi模块初始化 → 设置STA模式
```

### 2. WiFi连接阶段
```
尝试连接默认WiFi（OPPOx）
  ↓ 成功
进入网络测试
  ↓ 失败
显示菜单：
  1. 重新连接默认WiFi
  2. 扫描并连接其他WiFi
```

### 3. 网络测试阶段
```
Ping 1.1.1.1 → Ping 8.8.8.8 → 验证网络连通性
```

### 4. 主循环阶段
```
while(1) {
  每60秒：
    - 发送HTTP请求到 weather.haungzixuan.xin
    - 解析JSON响应
    - 格式化显示天气信息
  
  每1秒：
    - LED2闪烁表示系统运行
}
```

## 天气数据格式

### 服务器API
- **URL**: `http://weather.haungzixuan.xin/api/weather/`
- **方法**: GET
- **响应格式**: JSON

### JSON响应示例
```json
{
    "status": "success",
    "message": null,
    "data": {
        "location": "Beijing",
        "temperature": "25.5",
        "humidity": "60",
        "weather": "cloudy",
        "description": "partly cloudy",
        "last_update": "2025-11-20T17:18:53.360599"
    },
    "timestamp": "2025-11-20T17:19:31.897883"
}
```

### 显示格式
```
╔════════════════════════════════╗
║      天气信息显示              ║
╠════════════════════════════════╣
║ 位置: Beijing                  ║
║ 温度: 25.5                     ║
║ 湿度: 60                       ║
║ 天气: cloudy                   ║
║ 描述: partly cloudy            ║
╠════════════════════════════════╣
║ 更新: 2025-11-20T17:18:53      ║
╚════════════════════════════════╝
```

## LED状态指示

### LED2（绿色）
- 常亮：WiFi模块就绪
- 闪烁（1Hz）：系统正常运行
- 熄灭：系统错误或停止

### LED1（红色）
- 常亮：系统错误
- 闪烁（2Hz）：WiFi连接失败
- 熄灭：系统正常

## 编译配置

### Keil MDK设置
1. **必须勾选 Use MicroLIB**
   - 右键项目 → Options for Target
   - C/C++选项卡 → 勾选 "Use MicroLIB"
   
2. **项目分组**
   - User：main.c
   - Public：system.c, SysTick.c, usart.c
   - APP：led.c, key.c, esp8266相关
   - **cJSON**：cJSON.c（JSON解析库）
   - CMSIS：启动文件和核心库
   - STM32StdPeriph：外设驱动库

### 头文件路径
确保以下路径在Include Paths中：
- `..\User`
- `..\Public`
- `..\APP\led`
- `..\APP\esp8266`
- `..\APP\cJSON`
- `..\Libraries\CMSIS`
- `..\Libraries\STM32F10x_StdPeriph_Driver\inc`

## 代码架构

### WeatherData 结构体
```c
typedef struct {
    char location[32];       // 位置
    char temperature[16];    // 温度
    char humidity[16];       // 湿度
    char weather[32];        // 天气状况
    char description[64];    // 详细描述
    char last_update[32];    // 最后更新时间
    bool valid;              // 数据有效性
} WeatherData;
```

### 核心函数

#### WiFi相关
- `ESP8266_AT_Test()` - AT命令测试
- `ESP8266_Net_Mode_Choose(STA)` - 设置STA模式
- `WiFi_Connect_Default()` - 连接默认WiFi
- `WiFi_Connect_Menu()` - WiFi连接菜单
- `WiFi_Ping_Test(target)` - Ping测试

#### HTTP通信
- `ESP8266_TCP_Connect(ip, port)` - 连接TCP服务器
- `ESP8266_TCP_Send(data, len)` - 发送TCP数据
- `ESP8266_TCP_Disconnect()` - 断开连接
- `ESP8266_HTTP_Request(host, port, path, response, max_len)` - HTTP请求

#### 天气功能
- `Weather_Get_Data(weather)` - 获取天气数据
- `Weather_Display(weather)` - 显示天气数据

## 常见问题

### 1. 编译错误：printf相关
**解决方案**：确保勾选了 "Use MicroLIB" 选项

### 2. WiFi连接失败（busy p...）
**原因**：命令发送过快，ESP8266还在处理上一条命令
**解决方案**：
- 已添加自动busy检测和重试机制
- 命令间自动延迟100ms
- WiFi连接前额外延迟500ms

### 3. 扫描WiFi无结果
**原因**：扫描时间不够或缓冲区溢出
**解决方案**：
- 增加扫描等待时间到15秒
- 注释掉自动扫描，只在菜单中手动扫描

### 4. HTTP请求失败
**检查项**：
- WiFi是否已连接
- 网络是否通畅（Ping测试）
- 服务器是否可访问
- ESP8266只支持HTTP，不支持HTTPS

### 5. JSON解析失败
**检查项**：
- HTTP响应是否完整
- 响应缓冲区是否足够大（2048字节）
- JSON格式是否正确
- 是否正确跳过HTTP头

## 串口调试输出示例

```
USART Initialized OK
LED Initialized OK

=== 初始化ESP8266模块 ===
? ESP8266模块使能
? ESP8266模块AT测试通过
? ESP8266设置为STA模式

=== ESP8266模块初始化完成 ===

=== 尝试连接默认WiFi网络 ===
WiFi名称: OPPOx
正在连接WiFi: OPPOx...
? WiFi连接成功！

=== 开始网络连通性测试 ===
正在Ping 1.1.1.1...
? Ping 1.1.1.1 成功！

正在Ping 8.8.8.8...
? Ping 8.8.8.8 成功！

? Ping测试成功，网络连接正常

=== 进入天气监测主循环 ===
每60秒更新一次天气数据

=== 获取天气数据 ===
[HTTP请求成功]
解析JSON数据...
? 天气数据解析成功

╔════════════════════════════════╗
║      天气信息显示              ║
╠════════════════════════════════╣
║ 位置: Beijing                  ║
║ 温度: 25.5                     ║
║ 湿度: 60                       ║
║ 天气: cloudy                   ║
║ 描述: partly cloudy            ║
╠════════════════════════════════╣
║ 更新: 2025-11-20T17:18:53      ║
╚════════════════════════════════╝
```

## 性能参数

- **内存占用**：
  - HTTP响应缓冲区：2048字节
  - WeatherData结构：约160字节
  - cJSON解析：动态分配

- **时序参数**：
  - WiFi连接超时：20秒
  - HTTP请求超时：3秒
  - 天气更新间隔：60秒
  - Ping超时：5秒

## 扩展功能建议

1. **添加更多城市支持**
   - 修改API路径支持城市参数
   - 例如：`/api/weather?city=shanghai`

2. **添加LCD显示**
   - 集成LCD驱动
   - 图形化显示天气信息

3. **添加温湿度传感器**
   - DHT11/DHT22传感器
   - 对比本地和远程数据

4. **添加SD卡数据记录**
   - 记录历史天气数据
   - 生成统计报告

5. **添加RTC实时时钟**
   - 显示准确时间
   - 定时闹钟功能

## 版本历史

- **v0.0.1** - 基础WiFi连接和HTTP通信
- **v0.1.0** - 添加JSON解析和天气显示功能
- **v0.1.1** - 优化busy处理和命令延迟

## 许可证

本项目使用的cJSON库遵循MIT许可证。

## 联系方式

如有问题或建议，请通过串口调试信息排查或查看源代码注释。
