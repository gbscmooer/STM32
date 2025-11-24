特别注意：
不要在main.c中定义函数，main中只调用其他文件中的函数，以保持代码结构清晰。请阅读readme后在做改动，改动后写入readme以便他人了解你的改动。
APP\FREERTOS\examples\template_configuration\FreeRTOSConfig.h

最新改动记录：

2025-11-23 FreeRTOS与MQTT迁移：
1. 引入FreeRTOS操作系统：
   - 添加User/FreeRTOSConfig.h配置文件
   - 修改User/stm32f10x_it.c，屏蔽SVC、PendSV、SysTick中断处理，交由FreeRTOS接管
   - 修改Public/SysTick.c，使delay_ms兼容FreeRTOS调度器
   - 调整USART3中断优先级为5（低于configMAX_SYSCALL_INTERRUPT_PRIORITY=11）
2. 引入MQTT功能：
   - 新增User/mqtt_client.c和User/mqtt_client.h，实现MQTT连接、订阅和消息接收
   - 修改APP/esp8266/wifi_config.c，使用FreeRTOS StreamBuffer接收串口数据，支持MQTT流式处理
   - 优化ESP8266_TCP_Send函数，增加发送后延时避免指令冲突
3. 重构main.c：
   - main函数仅负责硬件初始化和启动FreeRTOS调度器
   - 业务逻辑迁移至User/app_freertos.c中的StartTask和MQTTTask
   - StartTask初始化ESP8266（使能、AT测试、设置STA模式）
   - MQTTTask自动连接WiFi、TCP、发送MQTT CONNECT和SUBSCRIBE
   - 系统启动后订阅weather/push主题等待推送

2025-11-20 重要修复：
1. 修复printf/scanf重定向问题：
   - 在usart.c中添加fputc和fgetc函数实现
   - 在usart.h中添加stdio.h头文件包含
   - 注意：Keil工程需要勾选"Use MicroLIB"选项才能使用printf
   - 路径：Target Options -> C/C++ -> Use MicroLIB
2. PC_Usart宏实际调用printf，现在已可正常使用

2025-11-20 重要修改：
1. 项目使用STM32标准外设库（不是HAL库），LED控制使用GPIO_SetBits/GPIO_ResetBits
2. 新增三个WiFi功能函数到wifi_function.c：
   - WiFi_Connect_Default(): 连接默认WiFi(OPPOx)
   - WiFi_Connect_Menu(): WiFi连接失败菜单处理
   - WiFi_Ping_Test(): 执行网络Ping测试
3. main.c重构为自动WiFi连接和网络测试流程：
   - 步骤1：初始化系统、串口、LED、ESP8266
   - 步骤2：自动连接WiFi，失败时提供交互菜单
   - 步骤3：执行1.1.1.1和8.8.8.8的Ping测试验证网络
4. LED状态指示：
   - LED1：系统初始化时点亮，测试通过后点亮表示成功
   - LED2：WiFi模块就绪时点亮，主循环中闪烁表示运行
5. 注意：ESP8266串口波特率为115200（已从9600修改）

STM32天气项目结构说明

一、项目简介

本项目是一个基于STM32F103ZE单片机的WiFi天气站系统，使用ESP8266 WiFi模块实现网络连接功能。项目采用Keil MDK开发环境，使用STM32标准外设库编程。

二、项目目录结构

1. APP目录：外设驱动模块
   - beep：蜂鸣器驱动
   - esp8266：ESP8266 WiFi模块驱动，包含wifi_config配置和wifi_function功能函数
   - exti：外部中断驱动
   - iwdg：独立看门狗驱动
   - key：按键驱动
   - led：LED灯驱动
   - lsens：光敏传感器驱动
   - pwm：PWM输出驱动
   - time：定时器驱动
   - wwdg：窗口看门狗驱动

2. User目录：用户应用程序
   - main.c：主程序文件，包含程序入口和主要业务逻辑
   - stm32f10x_it.c：中断服务函数
   - stm32f10x_conf.h：STM32外设库配置文件
   - stm32f10x.h：STM32寄存器定义文件

3. Public目录：公共功能模块
   - system.c/h：系统初始化和基本配置
   - SysTick.c/h：系统滴答定时器，提供延时功能
   - usart.c/h：串口通信驱动，包含printf重定向

4. Libraries目录：底层库文件
   - CMSIS：ARM Cortex-M3内核支持文件
     - core_cm3.c/h：内核函数
     - system_stm32f10x.c/h：系统时钟配置
     - startup_stm32f10x_hd.s：启动文件，大容量芯片
   - STM32F10x_StdPeriph_Driver：ST官方标准外设驱动库
     - src：外设驱动源文件
     - inc：外设驱动头文件

5. MDK_Project目录：Keil工程文件
   - STM32weather.uvprojx：Keil工程配置文件
   - Objects：编译输出的目标文件
   - Listings：编译生成的列表文件

三、主要功能说明

1. WiFi通信功能
   通过USART3与ESP8266模块通信，波特率115200，支持STA模式、AP模式和STA加AP混合模式，可实现TCP客户端和服务器功能。

2. 串口调试功能
   使用USART1进行PC串口调试，波特率115200，已重定向printf和scanf函数。

3. LED指示功能
   提供两路LED灯，用于系统状态指示和调试。

4. 看门狗功能
   支持独立看门狗IWDG和窗口看门狗WWDG，提高系统可靠性。

5. 定时器功能
   支持定时器PWM输出和定时中断。

四、编程规范

1. 文件命名规范
   - 头文件使用.h扩展名，源文件使用.c扩展名
   - 文件名使用小写字母，多个单词用下划线连接
   - 头文件需要使用条件编译防止重复包含

2. 函数注释规范
   每个函数前必须添加标准注释说明，格式如下：

/**
  * @brief  函数功能简要说明
  * @param  参数名称：参数说明
  * @retval 返回值说明
  */

例如：

/**
  * @brief  WiFi_Config wifi 初始化
  * @param  无
  * @retval 无
  */
void WiFi_Config(void)
{
    // 函数实现
}

/**
  * @brief  发送AT指令到ESP8266模块
  * @param  cmd：待发送的AT指令字符串
  * @param  reply1：期待的响应字符串1
  * @param  reply2：期待的响应字符串2
  * @param  waittime：等待响应的超时时间，单位毫秒
  * @retval true：指令发送成功，false：指令发送失败
  */
bool ESP8266_Cmd(char *cmd, char *reply1, char *reply2, u32 waittime)
{
    // 函数实现
}

3. 代码格式规范
   - 使用Tab键缩进，保持代码对齐
   - 左大括号不换行，右大括号单独一行
   - 运算符两边加空格
   - 指针星号紧跟类型名
   - 注释使用中文，放在代码上方或右侧

4. 变量命名规范
   - 局部变量使用驼峰命名法，首字母小写
   - 全局变量使用下划线分隔的小写字母
   - 宏定义使用全大写字母，下划线分隔
   - 结构体类型名使用大写字母开头的驼峰命名

5. 头文件包含规范
   - 系统头文件使用尖括号
   - 用户头文件使用双引号
   - 按照标准库、芯片库、用户头文件的顺序包含

6. 宏定义规范
   - 使用有意义的宏名称
   - 复杂宏定义使用括号保护
   - 避免在宏中使用副作用表达式

五、编译说明

1. 开发环境
   - IDE：Keil MDK 5.06或更高版本
   - 编译器：ARM Compiler 5.06
   - 芯片型号：STM32F103ZE（大容量产品）

2. 编译配置
   - 使用标准外设库，需定义USE_STDPERIPH_DRIVER宏
   - 优化等级：根据需要选择
   - C99标准支持

3. 注意事项
   - 确保启动文件startup_stm32f10x_hd.s已添加到工程
   - 确保所有外设驱动源文件都已添加到工程
   - 链接脚本使用默认配置，Flash起始地址0x08000000，大小512KB
   - SRAM起始地址0x20000000，大小64KB

六、调试说明

1. 串口调试
   - 使用USART1连接PC，波特率115200
   - 支持printf输出和scanf输入
   - 使用串口助手查看调试信息

2. WiFi调试
   - 115200
   - 可通过串口助手查看ESP8266返回信息
   - 注意CH引脚PA4用于模块使能，RST引脚PA15用于复位

3. LED指示
   - LED1和LED2可用于状态指示
   - GPIO_SetBits点亮，GPIO_ResetBits熄灭

七、常见问题

1. 编译错误处理
   - 检查USE_STDPERIPH_DRIVER宏是否定义
   - 检查头文件包含路径是否正确
   - 检查启动文件是否添加到工程

2. WiFi模块通信失败
   - 检查USART3配置是否正确
   - 检查波特率是否匹配
   - 检查CH和RST引脚电平

3. 串口无输出
   - 检查printf是否正确重定向到USART1
   - 检查USART1波特率配置
   - 检查串口助手端口和波特率设置

