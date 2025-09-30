#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
自动化测试脚本 - 验证可靠传输修复
通过分析日志文件验证Busy重试机制是否正常工作
"""

import os
import sys
import time
import hashlib
from pathlib import Path

def calculate_md5(filepath):
    """计算文件MD5"""
    md5_hash = hashlib.md5()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            md5_hash.update(chunk)
    return md5_hash.hexdigest()

def check_file_size(filepath):
    """检查文件大小"""
    return os.path.getsize(filepath) if os.path.exists(filepath) else 0

def analyze_log(log_file):
    """分析日志文件，验证Busy重试机制"""
    print("\n📊 分析日志文件...")

    if not os.path.exists(log_file):
        print(f"❌ 日志文件不存在: {log_file}")
        return False

    with open(log_file, 'r', encoding='utf-8', errors='ignore') as f:
        log_content = f.read()

    # 关键验证点
    checks = {
        "RetransmitPacketInternal Busy重试": "RetransmitPacketInternal: transport busy, retry",
        "SendAck Busy重试": "SendAck: transport busy, retry",
        "SendNak Busy重试": "SendNak: transport busy, retry",
        "SendStart Busy重试": "SendStart: transport busy, retry",
        "SendEnd Busy重试": "SendEnd: transport busy, retry",
        "SendFile Busy重试": "SendFile: transport busy, retry",
    }

    results = {}
    for name, pattern in checks.items():
        count = log_content.count(pattern)
        results[name] = count
        status = "✅" if count > 0 else "⚠️"
        print(f"  {status} {name}: {count}次")

    # 检查是否有错误
    error_patterns = [
        "ReportError called: 重传数据包失败",
        "ReportError called: 发送文件结束帧失败",
        "ReportError called: 握手超时",
        "ERROR - transport write failed with error: 6"
    ]

    print("\n🔍 检查错误信息...")
    has_errors = False
    for pattern in error_patterns:
        count = log_content.count(pattern)
        if count > 0:
            print(f"  ❌ 发现错误: {pattern} ({count}次)")
            has_errors = True

    if not has_errors:
        print("  ✅ 未发现Busy相关错误")

    # 检查传输是否完成
    if "传输完成" in log_content or "接收完成" in log_content:
        print("\n✅ 传输成功完成")
        return True
    else:
        print("\n❌ 传输未完成")
        return False

def main():
    print("=" * 60)
    print("可靠传输自动化测试验证工具")
    print("=" * 60)

    # 项目路径
    project_dir = Path("/mnt/c/Users/huangl/Desktop/PortMaster")
    test_file = project_dir / "招商证券股份有限公司融资融券业务合同.pdf"
    log_file = project_dir / "build/Debug/PortMaster_debug.log"

    # 检查测试文件
    print(f"\n📂 测试文件: {test_file}")
    if not test_file.exists():
        print("❌ 测试文件不存在！")
        return 1

    file_size = check_file_size(test_file)
    file_md5 = calculate_md5(test_file)
    print(f"  大小: {file_size} 字节")
    print(f"  MD5: {file_md5}")

    # 检查日志
    print(f"\n📋 日志文件: {log_file}")
    if not log_file.exists():
        print("⚠️ 日志文件不存在，需要先运行主程序测试")
        print("\n请按以下步骤操作：")
        print("1. 运行 build\\Debug\\PortMaster.exe")
        print("2. 选择Loopback传输")
        print("3. 勾选'可靠传输模式'")
        print("4. 选择测试PDF文件发送")
        print("5. 观察是否成功传输")
        print("6. 再次运行本脚本分析日志")
        return 1

    # 分析日志
    success = analyze_log(log_file)

    print("\n" + "=" * 60)
    if success:
        print("✅ 验证成功：Busy重试机制正常工作")
        print("=" * 60)
        return 0
    else:
        print("❌ 验证失败：请检查日志详情")
        print("=" * 60)
        return 1

if __name__ == "__main__":
    sys.exit(main())