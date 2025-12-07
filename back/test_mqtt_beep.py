#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测试MQTT beep命令发送
验证STM32能否收到beep=change
"""

import paho.mqtt.client as mqtt
import time
import sys

# MQTT配置
MQTT_BROKER = "47.83.221.161"
MQTT_PORT = 1883
MQTT_TOPIC_CONTROL = "stm32/control"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✓ 已连接到MQTT Broker: {MQTT_BROKER}:{MQTT_PORT}")
    else:
        print(f"✗ 连接失败，返回码: {rc}")

def on_publish(client, userdata, mid):
    print(f"✓ 消息已发布 (mid: {mid})")

def test_beep_command():
    """测试发送beep=change命令"""
    print("=" * 60)
    print("MQTT Beep命令测试")
    print("=" * 60)
    
    # 创建MQTT客户端
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_publish = on_publish
    
    try:
        # 连接到MQTT Broker
        print(f"正在连接到 {MQTT_BROKER}:{MQTT_PORT}...")
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_start()
        
        time.sleep(2)  # 等待连接建立
        
        # 发送beep=change命令
        print(f"\n发送命令到主题: {MQTT_TOPIC_CONTROL}")
        print(f"Payload: beep=change")
        
        result = client.publish(MQTT_TOPIC_CONTROL, "beep=change", qos=1)
        
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            print("✓ 命令发送成功！")
            print("\n请观察STM32串口输出，应该看到：")
            print("  1. [MQTT Recv] XX bytes")
            print("  2. MQTT Message Received")
            print("  3. Topic: stm32/control")
            print("  4. Payload: beep=change")
            print("  5. >>> MQTT BEEP COMMAND DETECTED <<<")
            print("  6. 蜂鸣器应该响0.5秒")
        else:
            print(f"✗ 发送失败，错误码: {result.rc}")
        
        # 等待消息发送完成
        time.sleep(2)
        
        # 断开连接
        client.loop_stop()
        client.disconnect()
        print("\n✓ 已断开连接")
        
    except Exception as e:
        print(f"\n✗ 错误: {e}")
        return False
    
    return True

def test_multiple_beeps():
    """测试发送多个beep命令（间隔5秒）"""
    print("\n" + "=" * 60)
    print("连续测试：每5秒发送一次beep命令")
    print("按Ctrl+C停止")
    print("=" * 60)
    
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_publish = on_publish
    
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_start()
        time.sleep(2)
        
        count = 1
        while True:
            print(f"\n[{count}] 发送 beep=change...")
            result = client.publish(MQTT_TOPIC_CONTROL, "beep=change", qos=1)
            
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                print(f"✓ 第{count}次命令已发送")
            else:
                print(f"✗ 发送失败")
            
            count += 1
            time.sleep(5)
            
    except KeyboardInterrupt:
        print("\n\n用户中断测试")
    except Exception as e:
        print(f"\n✗ 错误: {e}")
    finally:
        client.loop_stop()
        client.disconnect()
        print("✓ 已断开连接")

if __name__ == '__main__':
    print("STM32 MQTT Beep命令测试工具\n")
    
    if len(sys.argv) > 1 and sys.argv[1] == '--loop':
        test_multiple_beeps()
    else:
        test_beep_command()
        
        print("\n提示：")
        print("  运行 'python test_mqtt_beep.py --loop' 进行连续测试")
