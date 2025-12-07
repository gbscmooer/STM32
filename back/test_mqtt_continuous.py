#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
持续发送beep命令测试
用于测试HTTP上传期间MQTT消息接收
"""

import paho.mqtt.client as mqtt
import time
import sys

# MQTT配置
MQTT_BROKER = "47.83.221.161"
MQTT_PORT = 1883
MQTT_TOPIC = "weather/update"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✓ 已连接到MQTT Broker: {MQTT_BROKER}:{MQTT_PORT}")
    else:
        print(f"✗ 连接失败，返回码: {rc}")

def on_publish(client, userdata, mid):
    print(f"  [发布完成] mid: {mid}")

def continuous_beep_test(interval=5, total_count=20):
    """
    持续发送beep命令
    
    参数:
        interval: 发送间隔（秒）
        total_count: 总发送次数
    """
    print("=" * 70)
    print("MQTT Beep命令 - 持续测试")
    print("=" * 70)
    print(f"配置: 每{interval}秒发送一次，共{total_count}次")
    print(f"目的: 确保HTTP上传期间（45秒）至少有一次命令被接收")
    print("=" * 70)
    
    # 创建MQTT客户端
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_publish = on_publish
    
    try:
        # 连接到MQTT Broker
        print(f"\n正在连接到 {MQTT_BROKER}:{MQTT_PORT}...")
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_start()
        
        time.sleep(2)  # 等待连接建立
        
        print(f"\n开始发送命令到主题: {MQTT_TOPIC}")
        print("=" * 70)
        
        for i in range(1, total_count + 1):
            # 显示时间戳
            timestamp = time.strftime("%H:%M:%S")
            
            # 发送beep命令
            result = client.publish(MQTT_TOPIC, "beep=change", qos=1)
            
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                print(f"[{timestamp}] [{i:2d}/{total_count}] beep=change 已发送")
            else:
                print(f"[{timestamp}] [{i:2d}/{total_count}] 发送失败，错误码: {result.rc}")
            
            # 等待下一次发送
            if i < total_count:
                time.sleep(interval)
        
        print("=" * 70)
        print(f"\n✓ 测试完成！共发送 {total_count} 次命令")
        print("\n观察STM32串口输出：")
        print("  - 应该至少收到1次 '>>> MQTT BEEP COMMAND DETECTED <<<'")
        print("  - 如果HTTP上传期间（45秒）没收到，说明MQTT断开了")
        print("  - 如果HTTP完成后收到，说明重连成功")
        
        # 等待最后的消息发送完成
        time.sleep(2)
        
        # 断开连接
        client.loop_stop()
        client.disconnect()
        print("\n✓ 已断开连接")
        
    except KeyboardInterrupt:
        print("\n\n用户中断测试")
        client.loop_stop()
        client.disconnect()
    except Exception as e:
        print(f"\n✗ 错误: {e}")
        return False
    
    return True

if __name__ == '__main__':
    print("\nSTM32 MQTT Beep命令 - 持续测试工具\n")
    
    # 默认：每5秒一次，共20次（100秒）
    continuous_beep_test(interval=5, total_count=20)
    
    print("\n提示：")
    print("  - 修改间隔：在代码中修改 interval 参数")
    print("  - 修改次数：在代码中修改 total_count 参数")
    print("  - 推荐：5秒间隔，20次（覆盖HTTP上传时间）")
