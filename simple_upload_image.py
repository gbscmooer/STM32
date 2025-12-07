#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成80x60的8bit灰度图片并上传到服务器 - 简化版（无需numpy）
"""

import requests
import random
import time

def generate_gray_image_bytes(width=80, height=60):
    """生成80x60的8bit灰度图片字节数据"""
    total_pixels = width * height
    image_bytes = bytearray()
    
    # 创建渐变灰度图像
    for y in range(height):
        for x in range(width):
            # 基础渐变：从左到右，从上到下
            base_value = int((x / width) * 100 + (y / height) * 100)
            
            # 添加一些随机噪声
            noise = random.randint(-20, 20)
            
            # 在中间区域添加亮斑（模拟人物）
            center_x, center_y = width // 2, height // 2
            distance = abs(x - center_x) + abs(y - center_y)
            
            if distance < 15:  # 中心区域
                brightness_boost = 80
            else:
                brightness_boost = 0
            
            # 计算最终像素值并限制在0-255范围内
            pixel_value = base_value + noise + brightness_boost
            pixel_value = max(0, min(255, pixel_value))
            
            image_bytes.append(pixel_value)
    
    return bytes(image_bytes)

def upload_image(image_bytes, url="http://100.64.0.5:5000/video/upload"):
    """上传图片到服务器"""
    try:
        print(f"\n准备上传图片...")
        print(f"目标URL: {url}")
        print(f"图片数据大小: {len(image_bytes)} 字节")
        print(f"图片尺寸: 80x60")
        print(f"期望大小: 80*60 = 4800 字节")
        
        # 发送POST请求
        print("正在发送请求...")
        start_time = time.time()
        
        response = requests.post(
            url,
            data=image_bytes,
            headers={
                'Content-Type': 'application/octet-stream',
                'Content-Length': str(len(image_bytes))
            },
            timeout=10
        )
        
        end_time = time.time()
        elapsed_time = (end_time - start_time) * 1000  # 转换为毫秒
        
        print(f"\n上传完成!")
        print(f"响应时间: {elapsed_time:.2f}ms")
        print(f"HTTP状态码: {response.status_code}")
        
        if response.status_code == 200:
            try:
                result = response.json()
                print(f"服务器响应: {result}")
                
                if result.get('status') == 'success':
                    print("✓ 上传成功!")
                    if 'ai_result' in result:
                        ai_result = result['ai_result']
                        print(f"AI识别结果:")
                        print(f"  检测到人物: {ai_result.get('person_detected', 'N/A')}")
                        print(f"  置信度: {ai_result.get('confidence', 'N/A')}")
                        if 'door_triggered' in ai_result:
                            print(f"  门控制触发: {ai_result.get('door_triggered', 'N/A')}")
                else:
                    print(f"✗ 服务器返回错误: {result.get('message', 'Unknown error')}")
            except:
                print(f"响应内容: {response.text}")
        else:
            print(f"✗ 服务器错误: HTTP {response.status_code}")
            print(f"响应内容: {response.text}")
            
        return response
        
    except requests.exceptions.ConnectionError:
        print("✗ 连接失败: 无法连接到服务器")
        print("请检查:")
        print("1. 服务器地址是否正确: http://100.64.0.5:5000")
        print("2. 服务器是否正在运行")
        print("3. 网络连接是否正常")
        return None
    except requests.exceptions.Timeout:
        print("✗ 请求超时: 服务器响应时间过长")
        return None
    except Exception as e:
        print(f"✗ 上传过程中发生错误: {e}")
        import traceback
        traceback.print_exc()
        return None

def main():
    """主函数"""
    print("=" * 60)
    print("STM32 图片生成与上传工具 - 简化版")
    print("=" * 60)
    
    # 1. 生成图片
    print("\n[1/2] 生成80x60 8bit灰度图片...")
    image_bytes = generate_gray_image_bytes(80, 60)
    print(f"✓ 图片生成完成: {len(image_bytes)} 字节")
    
    # 2. 上传图片
    print("\n[2/2] 上传图片到服务器...")
    url = "http://100.64.0.5:5000/video/upload"
    print(f"使用默认URL: {url}")
    
    # 自动保存图片到文件
    filename = "test_gray_image.raw"
    with open(filename, 'wb') as f:
        f.write(image_bytes)
    print(f"✓ 图片已保存: {filename}")
    
    upload_image(image_bytes, url)
    
    print("\n" + "=" * 60)
    print("任务完成!")
    print("=" * 60)

if __name__ == "__main__":
    main()
