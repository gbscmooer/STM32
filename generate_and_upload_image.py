#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成80x60的8bit灰度图片并上传到服务器
"""

import numpy as np
import requests
from PIL import Image
import time
import os

def generate_gray_image(width=80, height=60):
    """生成80x60的8bit灰度图片"""
    # 创建一个渐变灰度图像
    # 从左到右：黑色到白色渐变
    # 从上到下：增加一些噪声
    
    # 创建基础渐变
    x = np.linspace(0, 255, width, dtype=np.uint8)
    y = np.linspace(0, 100, height, dtype=np.uint8)
    
    # 创建网格
    X, Y = np.meshgrid(x, y)
    
    # 添加一些随机噪声模拟真实图像
    noise = np.random.randint(-20, 20, (height, width), dtype=np.int16)
    
    # 组合渐变和噪声
    gray_array = np.clip(X + Y[:, np.newaxis] + noise, 0, 255).astype(np.uint8)
    
    # 在中间区域添加一个"亮斑"模拟人物
    center_h, center_w = height // 2, width // 2
    patch_size = 20
    
    # 创建亮斑
    for i in range(-patch_size//2, patch_size//2):
        for j in range(-patch_size//2, patch_size//2):
            if 0 <= center_h + i < height and 0 <= center_w + j < width:
                # 增加亮度
                new_value = gray_array[center_h + i, center_w + j] + 80
                gray_array[center_h + i, center_w + j] = min(new_value, 255)
    
    return gray_array

def save_image(image_array, filename="test_gray_image.png"):
    """保存图片到文件"""
    img = Image.fromarray(image_array, 'L')
    img.save(filename)
    print(f"图片已保存: {filename}")
    print(f"图片尺寸: {img.size}")
    print(f"图片模式: {img.mode}")
    
    # 显示图片信息
    print(f"像素数据大小: {len(image_array.tobytes())} 字节")
    print(f"期望大小: 80*60 = 4800 字节")
    
    return img

def upload_image(image_array, url="http://100.64.0.5:5000/video/upload"):
    """上传图片到服务器"""
    try:
        # 获取原始字节数据
        image_bytes = image_array.tobytes()
        
        print(f"\n准备上传图片...")
        print(f"目标URL: {url}")
        print(f"图片数据大小: {len(image_bytes)} 字节")
        print(f"图片尺寸: 80x60")
        
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
    print("STM32 图片生成与上传工具")
    print("=" * 60)
    
    # 1. 生成图片
    print("\n[1/2] 生成80x60 8bit灰度图片...")
    gray_image = generate_gray_image(80, 60)
    
    # 2. 保存图片（可选）
    save_option = input("\n是否保存图片到文件? (y/n): ").lower()
    if save_option == 'y':
        filename = input("请输入文件名 (默认: test_gray_image.png): ").strip()
        if not filename:
            filename = "test_gray_image.png"
        save_image(gray_image, filename)
    
    # 3. 上传图片
    print("\n[2/2] 上传图片到服务器...")
    url = input(f"请输入服务器URL (默认: http://100.64.0.5:5000/video/upload): ").strip()
    if not url:
        url = "http://100.64.0.5:5000/video/upload"
    
    upload_image(gray_image, url)
    
    print("\n" + "=" * 60)
    print("任务完成!")
    print("=" * 60)

if __name__ == "__main__":
    main()