#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
测试curl功能
"""

import requests
import json

def test_upload():
    """测试上传图片"""
    url = "http://100.64.0.5:5000/video/upload"
    
    # 读取图片文件
    with open("test_gray_image.raw", "rb") as f:
        image_data = f.read()
    
    print(f"图片大小: {len(image_data)} 字节")
    print(f"目标URL: {url}")
    
    try:
        response = requests.post(
            url,
            data=image_data,
            headers={
                'Content-Type': 'application/octet-stream',
                'Content-Length': str(len(image_data))
            },
            timeout=10
        )
        
        print(f"\nHTTP状态码: {response.status_code}")
        
        if response.status_code == 200:
            result = response.json()
            print(f"服务器响应: {json.dumps(result, indent=2, ensure_ascii=False)}")
        else:
            print(f"错误响应: {response.text}")
            
    except Exception as e:
        print(f"请求失败: {e}")

def test_health():
    """测试健康检查"""
    url = "http://100.64.0.5:5000/health"
    
    try:
        response = requests.get(url, timeout=5)
        print(f"健康检查状态码: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            print(f"健康状态: {json.dumps(result, indent=2, ensure_ascii=False)}")
    except Exception as e:
        print(f"健康检查失败: {e}")

def main():
    print("=" * 60)
    print("服务器测试工具")
    print("=" * 60)
    
    print("\n[1/3] 测试健康检查...")
    test_health()
    
    print("\n[2/3] 测试图片上传...")
    test_upload()
    
    print("\n[3/3] 查看图片信息...")
    with open("test_gray_image.raw", "rb") as f:
        data = f.read()
        print(f"文件大小: {len(data)} 字节")
        print(f"前20字节: {data[:20].hex()}")
        print(f"像素数: {len(data)} (应为80*60=4800)")
        
        # 检查是否为有效的80x60图像
        if len(data) == 4800:
            print("✓ 文件大小正确: 80x60 8bit灰度图")
        else:
            print(f"⚠ 文件大小异常: {len(data)} != 4800")

if __name__ == "__main__":
    main()