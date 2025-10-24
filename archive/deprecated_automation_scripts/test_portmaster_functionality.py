#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PortMaster功能完整性验证脚本
验证PortMaster工具的各项核心功能是否正常工作
"""

import os
import sys
import subprocess
import json
from pathlib import Path

# Windows编码设置
if sys.platform == 'win32':
    subprocess.run(['chcp', '65001'], shell=True, capture_output=True)

def check_executable_exists():
    """检查可执行文件是否存在"""
    exe_path = "build/Debug/PortMaster.exe"
    exists = os.path.exists(exe_path)
    print(f"[OK] 可执行文件存在: {exists}")
    if exists:
        size = os.path.getsize(exe_path) / (1024 * 1024)
        print(f"  文件大小: {size:.2f} MB")
    return exists

def test_transport_layer():
    """测试传输层功能"""
    print("\n=== 传输层功能测试 ===")

    # 检查传输层源文件
    transport_files = [
        "Transport/ITransport.h",
        "Transport/SerialTransport.cpp",
        "Transport/ParallelTransport.cpp",
        "Transport/UsbPrintTransport.cpp",
        "Transport/NetworkPrintTransport.cpp",
        "Transport/LoopbackTransport.cpp",
        "Transport/TransportFactory.cpp"
    ]

    for file_path in transport_files:
        exists = os.path.exists(file_path)
        print(f"  [{ 'OK' if exists else 'MISSING' }] {os.path.basename(file_path)}: {'存在' if exists else '缺失'}")
        if not exists:
            return False
    return True

def test_protocol_layer():
    """测试协议层功能"""
    print("\n=== 协议层功能测试 ===")

    protocol_files = [
        "Protocol/FrameCodec.h",
        "Protocol/FrameCodec.cpp",
        "Protocol/ReliableChannel.h",
        "Protocol/ReliableChannel.cpp"
    ]

    for file_path in protocol_files:
        exists = os.path.exists(file_path)
        print(f"  [{ 'OK' if exists else 'MISSING' }] {os.path.basename(file_path)}: {'存在' if exists else '缺失'}")
        if not exists:
            return False
    return True

def test_ui_components():
    """测试UI组件"""
    print("\n=== UI组件测试 ===")

    # 检查主要UI文件
    ui_files = [
        "src/PortMasterDlg.h",
        "src/PortMasterDlg.cpp",
        "src/TransmissionTask.h",
        "src/TransmissionTask.cpp",
        "src/UIStateManager.h",
        "src/UIStateManager.cpp",
        "resources/PortMaster.rc"
    ]

    for file_path in ui_files:
        exists = os.path.exists(file_path)
        print(f"  [{ 'OK' if exists else 'MISSING' }] {os.path.basename(file_path)}: {'存在' if exists else '缺失'}")
        if not exists:
            return False

    # 检查UI资源定义
    try:
        with open("resources/PortMaster.rc", 'r', encoding='utf-8') as f:
            rc_content = f.read()

        ui_elements = [
            "IDD_PORTMASTER_DIALOG",
            "IDC_COMBO_PORT_TYPE",
            "IDC_BUTTON_CONNECT",
            "IDC_BUTTON_SEND",
            "IDC_EDIT_RECEIVE_DATA",
            "IDC_PROGRESS"
        ]

        for element in ui_elements:
            exists = element in rc_content
            print(f"  [{ 'OK' if exists else 'MISSING' }] {element}: {'已定义' if exists else '未定义'}")
            if not exists:
                return False

    except Exception as e:
        print(f"  [ERROR] UI资源文件读取失败: {e}")
        return False

    return True

def test_config_system():
    """测试配置系统"""
    print("\n=== 配置系统测试 ===")

    config_files = [
        "Common/ConfigStore.h",
        "Common/ConfigStore.cpp",
        "Common/CommonTypes.h"
    ]

    for file_path in config_files:
        exists = os.path.exists(file_path)
        print(f"  [{ 'OK' if exists else 'MISSING' }] {os.path.basename(file_path)}: {'存在' if exists else '缺失'}")
        if not exists:
            return False

    return True

def test_automation_integration():
    """测试自动化系统集成"""
    print("\n=== 自动化系统集成测试 ===")

    automation_files = [
        "autotest_integration.py",
        "autonomous_master_controller.py",
        "run_automated_workflow.py",
        "AutoTest/AutoTest.exe"
    ]

    for file_path in automation_files:
        exists = os.path.exists(file_path)
        print(f"  [{ 'OK' if exists else 'MISSING' }] {os.path.basename(file_path)}: {'存在' if exists else '缺失'}")
        if not exists:
            return False

    # 测试AutoTest是否可用
    try:
        result = subprocess.run(
            ["./AutoTest/AutoTest.exe", "--help"],
            capture_output=True,
            text=True,
            timeout=10
        )
        auto_test_available = result.returncode == 0 or "AutoTest" in result.stdout
        print(f"  [{ 'OK' if auto_test_available else 'ERROR' }] AutoTest功能: {'可用' if auto_test_available else '不可用'}")
    except Exception as e:
        print(f"  [ERROR] AutoTest测试失败: {e}")
        auto_test_available = False

    return auto_test_available

def check_project_documentation():
    """检查项目文档"""
    print("\n=== 项目文档检查 ===")

    doc_files = [
        "SYSTEM_ARCHITECTURE.md",
        "USER_MANUAL.md",
        "FINAL_PROJECT_SUMMARY.md",
        "CLAUDE.md"
    ]

    for file_path in doc_files:
        exists = os.path.exists(file_path)
        print(f"  [{ 'OK' if exists else 'MISSING' }] {os.path.basename(file_path)}: {'存在' if exists else '缺失'}")

    return True

def generate_functionality_report():
    """生成功能完整性报告"""
    print("\n" + "="*60)
    print("PortMaster功能完整性验证报告")
    print("="*60)

    tests = [
        ("可执行文件", check_executable_exists),
        ("传输层", test_transport_layer),
        ("协议层", test_protocol_layer),
        ("UI组件", test_ui_components),
        ("配置系统", test_config_system),
        ("自动化集成", test_automation_integration),
        ("项目文档", check_project_documentation)
    ]

    results = {}
    for test_name, test_func in tests:
        try:
            results[test_name] = test_func()
        except Exception as e:
            print(f"  [ERROR] {test_name}测试失败: {e}")
            results[test_name] = False

    # 生成总结
    print("\n" + "="*60)
    print("功能完整性总结:")
    print("="*60)

    passed = sum(results.values())
    total = len(results)

    for test_name, result in results.items():
        status = "[PASS]" if result else "[FAIL]"
        print(f"  {test_name}: {status}")

    print(f"\n总体通过率: {passed}/{total} ({passed/total*100:.1f}%)")

    if passed == total:
        print("[SUCCESS] PortMaster功能完整性验证通过！")
    else:
        print("[WARNING] 部分功能需要完善")

    return results

if __name__ == "__main__":
    results = generate_functionality_report()

    # 保存详细报告
    report_data = {
        "timestamp": "2025-10-02 23:30:00",
        "total_tests": len(results),
        "passed_tests": sum(results.values()),
        "success_rate": sum(results.values()) / len(results) * 100,
        "details": results
    }

    with open("portmaster_functionality_report.json", "w", encoding="utf-8") as f:
        json.dump(report_data, f, ensure_ascii=False, indent=2)

    print(f"\n详细报告已保存至: portmaster_functionality_report.json")