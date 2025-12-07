#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
诊断STM32图像上传问题
快速定位是Nginx还是后端的问题
"""

import requests
import time

def test_backend_direct():
    """测试1: 直接访问后端（跳过Nginx）"""
    print("=" * 60)
    print("测试1: 直接访问后端 (100.64.0.5:5000)")
    print("=" * 60)
    
    # 生成4800字节测试数据（80x60灰度图）
    test_data = b'\x80' * 4800
    
    try:
        print("发送4800字节测试数据...")
        response = requests.post(
            "http://100.64.0.5:5000/video/upload",
            data=test_data,
            headers={'Content-Type': 'application/octet-stream'},
            timeout=10
        )
        
        print(f"✓ 后端响应: HTTP {response.status_code}")
        print(f"  响应内容: {response.text[:200]}")
        
        if response.status_code == 200:
            print("\n✓✓✓ 后端服务器工作正常！")
            return True
        else:
            print(f"\n✗ 后端返回错误")
            return False
            
    except requests.exceptions.ConnectionError as e:
        print(f"\n✗ 无法连接到后端服务器")
        print(f"  错误: {e}")
        print("\n  解决方案:")
        print("  1. SSH到100.64.0.5")
        print("  2. cd ~/STM32project/back")
        print("  3. python3 server_backend.py")
        return False
    except Exception as e:
        print(f"\n✗ 错误: {e}")
        return False

def test_nginx_proxy():
    """测试2: 通过Nginx代理访问"""
    print("\n" + "=" * 60)
    print("测试2: Nginx代理 (47.83.221.161:6637)")
    print("=" * 60)
    
    test_data = b'\x80' * 4800
    
    try:
        print("通过Nginx发送数据...")
        response = requests.post(
            "http://47.83.221.161:6637/video/upload",
            data=test_data,
            headers={'Content-Type': 'application/octet-stream'},
            timeout=30
        )
        
        print(f"✓ Nginx响应: HTTP {response.status_code}")
        print(f"  响应内容: {response.text[:200]}")
        
        if response.status_code == 200:
            print("\n✓✓✓ Nginx代理工作正常！")
            return True
        elif response.status_code == 502:
            print("\n✗ 502 Bad Gateway - 后端服务器无法连接")
            print("  后端服务器未启动！")
            return False
        elif response.status_code == 504:
            print("\n✗ 504 Gateway Timeout - 后端响应超时")
            print("  可能原因:")
            print("  1. 后端服务器处理太慢")
            print("  2. Nginx超时设置太短")
            return False
        else:
            print(f"\n✗ 意外的状态码: {response.status_code}")
            return False
            
    except requests.exceptions.Timeout:
        print(f"\n✗ 请求超时（超过30秒）")
        print("  可能原因:")
        print("  1. Nginx没有正确配置6637端口")
        print("  2. 防火墙阻止")
        print("  3. Nginx未转发到后端")
        return False
    except requests.exceptions.ConnectionError as e:
        print(f"\n✗ 连接失败: {e}")
        print("  无法连接到47.83.221.161:6637")
        print("\n  检查:")
        print("  1. sudo netstat -tlnp | grep 6637")
        print("  2. sudo nginx -t")
        print("  3. sudo systemctl status nginx")
        return False
    except Exception as e:
        print(f"\n✗ 错误: {e}")
        return False

def test_nginx_config():
    """测试3: 检查Nginx配置"""
    print("\n" + "=" * 60)
    print("测试3: Nginx配置检查")
    print("=" * 60)
    
    print("""
在47.83.221.161服务器上执行以下命令：

1. 检查6637端口监听:
   sudo netstat -tlnp | grep 6637
   
2. 检查Nginx配置:
   sudo nginx -T | grep -A 20 "6637"
   
3. 查看Nginx日志（实时）:
   sudo tail -f /var/log/nginx/error.log
   
4. 测试Nginx配置:
   sudo nginx -t
   
5. 重载配置:
   sudo systemctl reload nginx

推荐配置 (/etc/nginx/sites-available/stm32):
---
server {
    listen 6637;
    server_name _;
    
    proxy_connect_timeout 60s;
    proxy_send_timeout 60s;
    proxy_read_timeout 60s;
    client_max_body_size 50M;
    
    location /video/upload {
        proxy_pass http://100.64.0.5:5000/video/upload;
        proxy_set_header Host $host;
        proxy_buffering off;
    }
}
---
""")

def test_backend_health():
    """测试4: 后端健康检查"""
    print("\n" + "=" * 60)
    print("测试4: 后端健康检查")
    print("=" * 60)
    
    try:
        # 直接测试后端
        response = requests.get(
            "http://100.64.0.5:5000/health",
            timeout=5
        )
        print(f"✓ 后端健康检查: {response.status_code}")
        print(f"  {response.json()}")
        return True
    except:
        print("✗ 后端健康检查失败")
        return False
    
    try:
        # 通过Nginx测试
        response = requests.get(
            "http://47.83.221.161:6637/health",
            timeout=5
        )
        print(f"✓ Nginx代理健康检查: {response.status_code}")
        return True
    except:
        print("✗ Nginx代理健康检查失败（可能Nginx未配置/health路由）")
        return False

if __name__ == '__main__':
    print("🔍 STM32图像上传诊断工具")
    print("=" * 60)
    
    results = {}
    
    # 测试后端
    results['backend'] = test_backend_direct()
    
    # 测试Nginx
    results['nginx'] = test_nginx_proxy()
    
    # 健康检查
    results['health'] = test_backend_health()
    
    # 配置建议
    test_nginx_config()
    
    # 总结
    print("\n" + "=" * 60)
    print("诊断总结")
    print("=" * 60)
    
    if results['backend'] and results['nginx']:
        print("✓✓✓ 完整链路正常！STM32应该可以成功上传")
        print("\n如果STM32还是失败，检查:")
        print("1. ESP8266是否正确连接WiFi")
        print("2. STM32代码中的IP地址是否正确")
        print("3. HTTP请求格式是否正确")
    elif results['backend'] and not results['nginx']:
        print("✗ 后端正常，但Nginx代理有问题")
        print("\n问题出在47.83.221.161的Nginx配置")
        print("请检查上面的Nginx配置建议")
    elif not results['backend']:
        print("✗ 后端服务器有问题")
        print("\n解决方案:")
        print("1. SSH到100.64.0.5")
        print("2. cd ~/STM32project/back")
        print("3. python3 server_backend.py")
    else:
        print("⚠ 部分测试失败，请查看上面的详细日志")
    
    print("=" * 60)
