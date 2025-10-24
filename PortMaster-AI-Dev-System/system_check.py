#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI Development System - System Check Script

This script verifies the integrity and completeness of the AI Development System.
"""

import sys
import os
from pathlib import Path
import importlib.util

def check_file_exists(file_path: Path, description: str) -> bool:
    """Check if a file exists"""
    if file_path.exists():
        print(f"[OK] {description}: {file_path}")
        return True
    else:
        print(f"[FAIL] {description}: {file_path} - 文件不存在")
        return False

def check_python_module(module_path: Path, module_name: str) -> bool:
    """Check if a Python module can be imported"""
    try:
        spec = importlib.util.spec_from_file_location(module_name, module_path)
        if spec and spec.loader:
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            print(f"[OK] Python模块: {module_name}")
            return True
    except Exception as e:
        print(f"[FAIL] Python模块: {module_name} - 导入失败: {e}")
        return False
    return False

def main():
    """Main system check function"""
    print("AI Development System - 系统完整性检查")
    print("=" * 60)

    current_dir = Path(__file__).parent
    checks_passed = 0
    total_checks = 0

    # Check core files
    print("\n[INFO] 核心文件检查:")
    core_files = [
        (current_dir / "__init__.py", "主模块初始化文件"),
        (current_dir / "main.py", "主入口脚本"),
        (current_dir / "requirements.txt", "依赖文件"),
        (current_dir / "requirements-dev.txt", "开发依赖文件"),
    ]

    for file_path, description in core_files:
        total_checks += 1
        if check_file_exists(file_path, description):
            checks_passed += 1

    # Check documentation
    print("\n📚 文档文件检查:")
    doc_files = [
        (current_dir / "README.md", "项目说明文档"),
        (current_dir / "USER_MANUAL.md", "用户手册"),
        (current_dir / "API_REFERENCE.md", "API参考文档"),
        (current_dir / "DEPLOYMENT_GUIDE.md", "部署指南"),
        (current_dir / "EXAMPLES.md", "使用示例"),
        (current_dir / "PROJECT_SUMMARY.md", "项目总结"),
        (current_dir / "QUICK_START.md", "快速开始指南"),
    ]

    for file_path, description in doc_files:
        total_checks += 1
        if check_file_exists(file_path, description):
            checks_passed += 1

    # Check directory structure
    print("\n📂 目录结构检查:")
    directories = [
        (current_dir / "core", "核心控制器模块"),
        (current_dir / "analyzers", "分析器模块"),
        (current_dir / "integrators", "集成器模块"),
        (current_dir / "utils", "工具模块"),
        (current_dir / "config", "配置模块"),
        (current_dir / "reporters", "报告模块"),
    ]

    for dir_path, description in directories:
        total_checks += 1
        if dir_path.exists() and dir_path.is_dir():
            print(f"✅ {description}: {dir_path}")
            checks_passed += 1
        else:
            print(f"❌ {description}: {dir_path} - 目录不存在")

    # Check core module files
    print("\n🔧 核心模块文件检查:")
    core_module_files = [
        (current_dir / "core" / "__init__.py", "核心模块初始化"),
        (current_dir / "core" / "master_controller.py", "主控制器"),
        (current_dir / "core" / "fix_controller.py", "修复控制器"),
        (current_dir / "core" / "workflow_runner.py", "工作流运行器"),
    ]

    for file_path, description in core_module_files:
        total_checks += 1
        if check_file_exists(file_path, description):
            checks_passed += 1

    # Check analyzer module files
    print("\n🔍 分析器模块文件检查:")
    analyzer_files = [
        (current_dir / "analyzers" / "__init__.py", "分析器模块初始化"),
        (current_dir / "analyzers" / "static_analyzer.py", "静态分析器"),
        (current_dir / "analyzers" / "coverage_analyzer.py", "覆盖率分析器"),
        (current_dir / "analyzers" / "error_pattern_analyzer.py", "错误模式分析器"),
        (current_dir / "analyzers" / "performance_analyzer.py", "性能分析器"),
    ]

    for file_path, description in analyzer_files:
        total_checks += 1
        if check_file_exists(file_path, description):
            checks_passed += 1

    # Check integrator module files
    print("\n🔗 集成器模块文件检查:")
    integrator_files = [
        (current_dir / "integrators" / "__init__.py", "集成器模块初始化"),
        (current_dir / "integrators" / "build_integrator.py", "构建集成器"),
        (current_dir / "integrators" / "test_integrator.py", "测试集成器"),
        (current_dir / "integrators" / "cicd_integrator.py", "CI/CD集成器"),
    ]

    for file_path, description in integrator_files:
        total_checks += 1
        if check_file_exists(file_path, description):
            checks_passed += 1

    # Check utility module files
    print("\n🛠️ 工具模块文件检查:")
    util_files = [
        (current_dir / "utils" / "__init__.py", "工具模块初始化"),
        (current_dir / "utils" / "file_utils.py", "文件操作工具"),
        (current_dir / "utils" / "log_utils.py", "日志工具"),
        (current_dir / "utils" / "validation_utils.py", "验证工具"),
        (current_dir / "utils" / "git_utils.py", "Git工具"),
    ]

    for file_path, description in util_files:
        total_checks += 1
        if check_file_exists(file_path, description):
            checks_passed += 1

    # Check config module files
    print("\n⚙️ 配置模块文件检查:")
    config_files = [
        (current_dir / "config" / "__init__.py", "配置模块初始化"),
        (current_dir / "config" / "config_manager.py", "配置管理器"),
        (current_dir / "config" / "default_config.yaml", "默认配置文件"),
    ]

    for file_path, description in config_files:
        total_checks += 1
        if check_file_exists(file_path, description):
            checks_passed += 1

    # Check reporter module files
    print("\n📊 报告模块文件检查:")
    reporter_files = [
        (current_dir / "reporters" / "__init__.py", "报告模块初始化"),
        (current_dir / "reporters" / "report_generator.py", "报告生成器"),
    ]

    for file_path, description in reporter_files:
        total_checks += 1
        if check_file_exists(file_path, description):
            checks_passed += 1

    # Check Python syntax (basic)
    print("\n🐍 Python语法检查:")
    python_files = list(current_dir.rglob("*.py"))

    # Check a few key files for syntax
    key_files = [
        current_dir / "__init__.py",
        current_dir / "main.py",
        current_dir / "core" / "master_controller.py",
        current_dir / "analyzers" / "static_analyzer.py",
    ]

    for file_path in key_files:
        if file_path.exists():
            total_checks += 1
            try:
                with open(file_path, 'r', encoding='utf-8') as f:
                    compile(f.read(), file_path, 'exec')
                print(f"✅ Python语法: {file_path.name}")
                checks_passed += 1
            except SyntaxError as e:
                print(f"❌ Python语法: {file_path.name} - 语法错误: {e}")
            except Exception as e:
                print(f"⚠️ Python语法: {file_path.name} - 检查失败: {e}")

    # Summary
    print("\n" + "=" * 60)
    print(f"📊 检查结果: {checks_passed}/{total_checks} 项通过")

    if checks_passed == total_checks:
        print("🎉 所有检查都通过了！AI开发系统完整性验证成功！")
        return 0
    else:
        failed_count = total_checks - checks_passed
        print(f"⚠️ 有 {failed_count} 项检查失败，请检查上述问题。")
        return 1

if __name__ == "__main__":
    sys.exit(main())