#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
100.64.0.5:5000 后端服务器
功能：
1. 接收47.83.221.161转发的图像（Nginx代理）
2. AI人物识别（模拟）
3. 识别到人后，向47.83.221.161发送door=1命令
4. 保存图像历史

部署在: 100.64.0.5:5000
Nginx代理: 47.83.221.161:6637 -> 100.64.0.5:5000
"""

from flask import Flask, request, jsonify
from datetime import datetime
import os
import struct
import requests
from PIL import Image
import numpy as np

app = Flask(__name__)

# ==================== 配置 ====================
UPLOAD_FOLDER = 'received_images'
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

# 47.83.221.161 Nginx服务器配置
NGINX_SERVER = "https://weather.haungzixuan.xin"
BEEP_API = f"{NGINX_SERVER}/api/mqtt/beep/"

# AI识别配置
AI_ENABLED = True  # 是否启用AI识别
PERSON_DETECT_THRESHOLD = 0.6  # 人物检测阈值（模拟）

# 统计信息
stats = {
    "total_images": 0,
    "person_detected": 0,
    "door_triggered": 0,
    "last_detection": None
}

# ==================== RGB565转换 ====================
def rgb565_to_rgb888(data, width, height):
    """将RGB565二进制数据转换为RGB888"""
    pixels = []
    for i in range(0, len(data), 2):
        if i + 1 >= len(data):
            break

        # 读取16位RGB565
        pixel = struct.unpack('<H', data[i:i+2])[0]

        # 提取RGB分量（5-6-5）
        r5 = (pixel >> 11) & 0x1F
        g6 = (pixel >> 5) & 0x3F
        b5 = pixel & 0x1F

        # 转换为8位RGB
        r8 = (r5 * 255) // 31
        g8 = (g6 * 255) // 63
        b8 = (b5 * 255) // 31

        pixels.append([r8, g8, b8])

    # 重塑为图像数组
    expected_pixels = width * height
    current_pixels = len(pixels)

    if current_pixels == expected_pixels:
        img_array = np.array(pixels, dtype=np.uint8).reshape(height, width, 3)
        return img_array
    elif current_pixels < expected_pixels:
        print(f"⚠ 像素不足: {current_pixels} < {expected_pixels}，自动填充黑色")
        # 填充黑色像素
        padding = [[0, 0, 0]] * (expected_pixels - current_pixels)
        pixels.extend(padding)
        img_array = np.array(pixels, dtype=np.uint8).reshape(height, width, 3)
        return img_array
    else:
        print(f"⚠ 像素过多: {current_pixels} > {expected_pixels}，自动裁剪")
        # 裁剪多余像素
        pixels = pixels[:expected_pixels]
        img_array = np.array(pixels, dtype=np.uint8).reshape(height, width, 3)
        return img_array

# ==================== AI人物识别（模拟）====================
def detect_person(img_array):
    """
    模拟AI人物识别
    实际项目中可使用 YOLOv8、OpenCV等
    这里简单模拟：计算图像平均亮度，大于阈值认为有人
    """
    if img_array is None:
        return False, 0.0

    # 模拟检测：计算图像中间区域的平均亮度
    h, w = img_array.shape[:2]
    center_region = img_array[h//4:3*h//4, w//4:3*w//4]
    avg_brightness = np.mean(center_region)

    # 简单规则：亮度>100认为可能有人（实际应使用深度学习模型）
    if avg_brightness > 100:
        confidence = min(avg_brightness / 200, 1.0)  # 归一化到0-1
        has_person = confidence > PERSON_DETECT_THRESHOLD
        return has_person, confidence
    else:
        return False, 0.0

# ==================== 触发蜂鸣器 ====================
def trigger_beep():
    """
    向服务器发送蜂鸣器命令（响一次）
    调用: POST https://weather.haungzixuan.xin/api/mqtt/beep/
    """
    try:
        beep_url = f"{NGINX_SERVER}/api/mqtt/beep/"
        print(f"\n{'='*60}")
        print(f"[BEEP TRIGGER] 向服务器发送蜂鸣器命令")
        print(f"目标URL: {beep_url}")
        print(f"{'='*60}")

        response = requests.post(
            beep_url,
            timeout=5,
            verify=False  # 如果是自签名证书，跳过验证
        )

        if response.status_code == 200:
            result = response.json()
            print(f"✓ 蜂鸣器命令发送成功!")
            print(f"  响应: {result}")
            stats["door_triggered"] += 1  # 重用计数器，虽然现在是蜂鸣器
            return True
        else:
            print(f"✗ 蜂鸣器命令发送失败: HTTP {response.status_code}")
            print(f"  响应: {response.text}")
            return False

    except requests.exceptions.RequestException as e:
        print(f"✗ 网络请求失败: {e}")
        return False
    except Exception as e:
        print(f"✗ 触发蜂鸣器时出错: {e}")
        return False



# ==================== Flask路由 ====================

@app.route('/video/upload', methods=['POST'])
def upload_video():
    """
    接收STM32上传的图像数据
    支持80x60 8bit灰度图
    工作流程：
    1. 接收灰度图像数据
    2. 转换并保存图像
    3. AI人物识别
    4. 如果检测到人，触发beep命令（蜂鸣器响两次，间隔1秒）
    """
    try:
        # 获取二进制数据
        image_data = request.data
        data_len = len(image_data)

        timestamp = datetime.now().strftime('%Y-%m-%d_%H-%M-%S')
        print(f"\n{'='*60}")
        print(f"[{timestamp}] 收到图像上传")
        print(f"  原始大小: {data_len} 字节")

        stats["total_images"] += 1

        # 80x60 灰度图配置
        width, height = 80, 60
        expected_pixels = width * height  # 4800
        expected_bytes = expected_pixels  # 1 byte per pixel for grayscale

        print(f"  识别尺寸: {width}x{height} (8bit灰度)")
        print(f"  期望字节: {expected_bytes} (实际: {data_len})")

        # 处理灰度数据
        if data_len < expected_bytes:
            print(f"⚠ 数据不足，填充黑色")
            image_data += b'\x00' * (expected_bytes - data_len)
        elif data_len > expected_bytes:
            print(f"⚠ 数据过多，裁剪")
            image_data = image_data[:expected_bytes]

        try:
            # 创建灰度图像
            img = Image.frombytes('L', (width, height), image_data)
            img = img.convert('RGB')  # 转为RGB以便后续处理
            img_array = np.array(img)

            # 保存为PNG
            filename = f'camera_{timestamp}.png'
            filepath = os.path.join(UPLOAD_FOLDER, filename)
            img.save(filepath)
            print(f"  ✓ 图像已保存: {filename}")

            # AI人物识别
            if AI_ENABLED:
                has_person, confidence = detect_person(img_array)

                print(f"\n  [AI识别结果]")
                print(f"    检测到人物: {'是' if has_person else '否'}")
                print(f"    置信度: {confidence:.2%}")

                if has_person:
                    stats["person_detected"] += 1
                    stats["last_detection"] = timestamp

                    print(f"\n  🚨 检测到人物！触发蜂鸣器...")

                    # 触发蜂鸣器 - 发送两次请求，间隔1秒
                    beep_success = trigger_beep()

                    return jsonify({
                        "status": "success",
                        "message": "Image received and person detected",
                        "filename": filename,
                        "size": data_len,
                        "resolution": f"{width}x{height}",
                        "ai_result": {
                            "person_detected": True,
                            "confidence": f"{confidence:.2%}",
                            "beep_triggered": beep_success
                        }
                    }), 200
                else:
                    return jsonify({
                        "status": "success",
                        "message": "Image received, no person detected",
                        "filename": filename,
                        "size": data_len,
                        "resolution": f"{width}x{height}",
                        "ai_result": {
                            "person_detected": False,
                            "confidence": f"{confidence:.2%}"
                        }
                    }), 200
            else:
                # AI未启用
                return jsonify({
                    "status": "success",
                    "message": "Image received (AI disabled)",
                    "filename": filename,
                    "size": data_len,
                    "resolution": f"{width}x{height}"
                }), 200

        except Exception as e:
            print(f"  ✗ 图像处理失败: {e}")
            return jsonify({
                "status": "error",
                "message": f"Image processing failed: {str(e)}"
            }), 400

    except Exception as e:
        print(f"✗ 上传处理错误: {e}")
        import traceback
        traceback.print_exc()
        return jsonify({
            "status": "error",
            "message": str(e)
        }), 500

@app.route('/api/stats', methods=['GET'])
def get_stats():
    """获取统计信息"""
    return jsonify({
        "status": "success",
        "stats": stats
    })

@app.route('/api/config', methods=['GET', 'POST'])
def config():
    """配置AI识别参数"""
    global AI_ENABLED, PERSON_DETECT_THRESHOLD

    if request.method == 'POST':
        data = request.get_json()
        if 'ai_enabled' in data:
            AI_ENABLED = data['ai_enabled']
        if 'threshold' in data:
            PERSON_DETECT_THRESHOLD = float(data['threshold'])

        return jsonify({
            "status": "success",
            "message": "Config updated",
            "config": {
                "ai_enabled": AI_ENABLED,
                "threshold": PERSON_DETECT_THRESHOLD
            }
        })
    else:
        return jsonify({
            "status": "success",
            "config": {
                "ai_enabled": AI_ENABLED,
                "threshold": PERSON_DETECT_THRESHOLD,
                "nginx_server": NGINX_SERVER,
                "beep_api": BEEP_API
            }
        })

@app.route('/api/test_beep', methods=['POST'])
def test_beep():
    """测试蜂鸣器（手动触发）"""
    success = trigger_beep()
    return jsonify({
        "status": "success" if success else "error",
        "message": "Beep trigger test completed"
    })

@app.route('/health', methods=['GET'])
def health():
    """健康检查"""
    return jsonify({
        "status": "healthy",
        "service": "STM32 Backend Server",
        "timestamp": datetime.now().isoformat(),
        "stats": stats
    })

@app.route('/', methods=['GET'])
def index():
    """首页 - 基本信息"""
    return jsonify({
        "service": "STM32 Backend Server",
        "version": "1.0",
        "status": "running",
        "stats": stats,
        "config": {
            "ai_enabled": AI_ENABLED,
            "threshold": PERSON_DETECT_THRESHOLD,
            "nginx_server": NGINX_SERVER,
            "beep_api": BEEP_API
        },
        "endpoints": [
            "POST /video/upload",
            "GET /api/stats",
            "GET/POST /api/config",
            "POST /api/test_beep",
            "GET /health"
        ]
    })


if __name__ == '__main__':
    print("🚀 启动STM32后端服务器...")
    print(f"   服务地址: 0.0.0.0:5000")
    print(f"   上传接口: POST /video/upload")
    print(f"   健康检查: GET /health")
    print("-" * 50)
    app.run(host='0.0.0.0', port=5000, debug=False)
