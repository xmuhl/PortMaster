#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import sys
import os
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

# 设置控制台输出编码
if sys.platform == 'win32':
    import subprocess
    subprocess.run(['chcp', '65001'], shell=True)

"""
自动化系统状态检查脚本
检查PortMaster自动化系统的各个组件是否正常工作
"""

import os
import sys
import subprocess
from pathlib import Path

def check_python_environment():
    """检查Python环境"""
    print("=== Python环境检查 ===")
    print(f"Python版本: {sys.version}")

    required_modules = [
        'autonomous_master_controller',
        'run_automated_workflow',
        'build_integration',
        'autotest_integration',
        'static_analyzer'
    ]

    for module in required_modules:
        try:
            __import__(module)
            print(f"[OK] {module} - 可用")
        except ImportError as e:
            print(f"[FAIL] {module} - 不可用: {e}")

def check_project_structure():
    """检查项目结构"""
    print("\n=== 项目结构检查 ===")

    required_files = [
        'autonomous_master_controller.py',
        'run_automated_workflow.py',
        'build_integration.py',
        'autotest_integration.py',
        'static_analyzer.py',
        'AutoTest/AutoTest.exe',
        'AutoTest/TestFramework.h'
    ]

    for file_path in required_files:
        if os.path.exists(file_path):
            print(f"[OK] {file_path}")
        else:
            print(f"[MISSING] {file_path}")

def check_build_system():
    """检查构建系统"""
    print("\n=== 构建系统检查 ===")

    build_scripts = [
        'autobuild_x86_debug.bat',
        'autobuild.bat'
    ]

    for script in build_scripts:
        if os.path.exists(script):
            print(f"[OK] {script}")
        else:
            print(f"[MISSING] {script}")

def check_test_executable():
    """检查测试可执行文件"""
    print("\n=== 测试程序检查 ===")

    test_exe = "AutoTest/AutoTest.exe"
    if os.path.exists(test_exe):
        print(f"[OK] {test_exe} 存在")

        # 尝试运行简单测试
        try:
            result = subprocess.run(
                [test_exe, '--help'],
                capture_output=True,
                text=True,
                timeout=10
            )
            if result.returncode == 0:
                print("[OK] 测试程序可以正常运行")
            else:
                print(f"[WARNING] 测试程序运行异常: {result.stderr}")
        except subprocess.TimeoutExpired:
            print("[WARNING] 测试程序响应超时")
        except Exception as e:
            print(f"[FAIL] 测试程序无法运行: {e}")
    else:
        print(f"[MISSING] {test_exe} 不存在")

def check_automation_reports():
    """检查自动化报告"""
    print("\n=== 自动化报告检查 ===")

    reports_dir = "automation_reports"
    if os.path.exists(reports_dir):
        print(f"[OK] {reports_dir} 目录存在")

        reports = os.listdir(reports_dir)
        if reports:
            print(f"[INFO] 发现 {len(reports)} 个报告文件:")
            for report in reports[:5]:  # 只显示前5个
                print(f"   - {report}")
            if len(reports) > 5:
                print(f"   ... 还有 {len(reports) - 5} 个文件")
        else:
            print("[WARNING] 报告目录为空")
    else:
        print(f"[MISSING] {reports_dir} 目录不存在")

def check_ci_cd():
    """检查CI/CD配置"""
    print("\n=== CI/CD配置检查 ===")

    workflows_dir = ".github/workflows"
    if os.path.exists(workflows_dir):
        print(f"[OK] {workflows_dir} 目录存在")

        workflows = os.listdir(workflows_dir)
        for workflow in workflows:
            print(f"[FILE] {workflow}")
    else:
        print(f"[MISSING] {workflows_dir} 目录不存在")

def run_quick_test():
    """运行快速测试"""
    print("\n=== 快速测试 ===")

    test_exe = "AutoTest/AutoTest.exe"
    if os.path.exists(test_exe):
        print("尝试运行单元测试...")
        try:
            result = subprocess.run(
                [test_exe, '--unit-tests'],
                capture_output=True,
                text=True,
                timeout=30
            )

            if "PASS" in result.stdout:
                print("[OK] 单元测试有通过的结果")
            else:
                print("[WARNING] 单元测试可能存在问题")

            # 显示简要结果
            lines = result.stdout.split('\n')
            for line in lines:
                if 'PASS' in line or 'FAIL' in line:
                    print(f"   {line.strip()}")

        except subprocess.TimeoutExpired:
            print("[WARNING] 测试执行超时")
        except Exception as e:
            print(f"[FAIL] 测试执行失败: {e}")

def main():
    """主函数"""
    print("PortMaster 自动化系统状态检查")
    print("=" * 50)

    check_python_environment()
    check_project_structure()
    check_build_system()
    check_test_executable()
    check_automation_reports()
    check_ci_cd()
    run_quick_test()

    print("\n" + "=" * 50)
    print("检查完成")

if __name__ == "__main__":
    main()