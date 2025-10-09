#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
自动化工作流执行脚本
整合所有测试模块，实现完整的自动化开发-测试-修复循环
"""

import os
import sys
import argparse
import json
import logging
from datetime import datetime
from pathlib import Path

# Windows编码设置
if sys.platform == 'win32':
    import subprocess
    # 设置控制台编码为UTF-8
    subprocess.run(['chcp', '65001'], shell=True, capture_output=True)

from autonomous_master_controller import AutonomousMasterController, AutomationWorkflow

def create_comprehensive_workflow(controller: AutonomousMasterController) -> AutomationWorkflow:
    """创建综合测试工作流"""
    steps = [
        {
            "name": "静态代码分析",
            "type": "analyze",
            "description": "执行Cppcheck静态分析",
            "enabled": True
        },
        {
            "name": "清理构建",
            "type": "build",
            "description": "执行完整的项目构建",
            "config": {
                "clean": True,
                "build_script": "autobuild_x86_debug.bat"
            },
            "enabled": True
        },
        {
            "name": "自动修复",
            "type": "fix",
            "description": "如果构建失败，尝试自动修复",
            "condition": lambda ctx: not ctx.get('build', {}).get('success', False),
            "enabled": True
        },
        {
            "name": "单元测试",
            "type": "test",
            "description": "运行Transport和Protocol单元测试",
            "config": {
                "mode": "unit-tests"
            },
            "enabled": True
        },
        {
            "name": "集成测试",
            "type": "test",
            "description": "运行多层协议栈和文件传输集成测试",
            "config": {
                "mode": "integration"
            },
            "enabled": True
        },
        {
            "name": "性能测试",
            "type": "test",
            "description": "运行性能基准测试",
            "config": {
                "mode": "performance"
            },
            "enabled": True
        },
        {
            "name": "压力测试",
            "type": "test",
            "description": "运行压力和稳定性测试",
            "config": {
                "mode": "stress"
            },
            "enabled": True
        },
        {
            "name": "回归测试",
            "type": "regression",
            "description": "执行回归测试并生成对比报告",
            "config": {
                "version": datetime.now().strftime("%Y%m%d-%H%M%S"),
                "auto_regression": True
            },
            "enabled": True
        }
    ]

    return AutomationWorkflow("综合自动化测试流程", steps)

def create_quick_test_workflow(controller: AutonomousMasterController) -> AutomationWorkflow:
    """创建快速测试工作流（仅单元测试）"""
    steps = [
        {
            "name": "快速构建",
            "type": "build",
            "description": "执行快速构建验证",
            "config": {
                "clean": False,
                "build_script": "autobuild_x86_debug.bat"
            },
            "enabled": True
        },
        {
            "name": "单元测试",
            "type": "test",
            "description": "仅运行单元测试",
            "config": {
                "mode": "unit-tests"
            },
            "enabled": True
        }
    ]

    return AutomationWorkflow("快速测试流程", steps)

def create_regression_only_workflow(controller: AutonomousMasterController, baseline_version: str) -> AutomationWorkflow:
    """创建仅回归测试工作流"""
    steps = [
        {
            "name": "构建验证",
            "type": "build",
            "description": "验证当前代码可构建",
            "config": {
                "clean": False,
                "build_script": "autobuild_x86_debug.bat"
            },
            "enabled": True
        },
        {
            "name": "全面测试",
            "type": "test",
            "description": "运行所有测试套件",
            "config": {
                "mode": "all"
            },
            "enabled": True
        },
        {
            "name": "回归对比",
            "type": "regression",
            "description": f"与基线版本 {baseline_version} 对比",
            "config": {
                "baseline_version": baseline_version,
                "current_version": datetime.now().strftime("%Y%m%d-%H%M%S")
            },
            "enabled": True
        }
    ]

    return AutomationWorkflow("回归测试流程", steps)

def print_workflow_summary(result: dict):
    """打印工作流执行摘要"""
    print("\n" + "="*80)
    print(f"工作流执行摘要: {result.get('workflow_name', 'Unknown')}")
    print("="*80)

    print(f"\n总步骤数: {result.get('total_steps', 0)}")
    print(f"成功步骤: {result.get('successful_steps', 0)}")
    print(f"失败步骤: {result.get('failed_steps', 0)}")
    print(f"跳过步骤: {result.get('skipped_steps', 0)}")
    print(f"总耗时: {result.get('total_duration', 0):.2f} 秒")

    # 打印每个步骤的结果
    print("\n步骤详情:")
    for idx, step_result in enumerate(result.get('step_results', []), 1):
        status = "✅" if step_result['success'] else "❌"
        print(f"  {status} 步骤 {idx}: {step_result['step_name']}")
        print(f"     耗时: {step_result['duration']:.2f}秒")
        if not step_result['success'] and step_result.get('error'):
            print(f"     错误: {step_result['error']}")

    # 最终状态
    overall_success = result.get('overall_success', False)
    print(f"\n整体状态: {'✅ 成功' if overall_success else '❌ 失败'}")
    print("="*80)

def main():
    parser = argparse.ArgumentParser(
        description="PortMaster自动化测试工作流执行工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
工作流模式:
  comprehensive  - 综合测试流程（静态分析+构建+全部测试+回归）
  quick         - 快速测试流程（构建+单元测试）
  regression    - 回归测试流程（构建+全部测试+回归对比）
  auto-fix      - 自动修复流程（迭代构建-修复-重建）

示例:
  python run_automated_workflow.py comprehensive
  python run_automated_workflow.py quick
  python run_automated_workflow.py regression --baseline v1.0.0
  python run_automated_workflow.py auto-fix --max-iterations 5
        """
    )

    parser.add_argument(
        'mode',
        choices=['comprehensive', 'quick', 'regression', 'auto-fix'],
        help='工作流执行模式'
    )

    parser.add_argument(
        '--baseline',
        type=str,
        help='回归测试基线版本（仅用于regression模式）'
    )

    parser.add_argument(
        '--max-iterations',
        type=int,
        default=3,
        help='自动修复最大迭代次数（仅用于auto-fix模式，默认3次）'
    )

    parser.add_argument(
        '--project-dir',
        type=str,
        default='.',
        help='项目根目录（默认为当前目录）'
    )

    parser.add_argument(
        '--report-dir',
        type=str,
        default='automation_reports',
        help='报告输出目录（默认为automation_reports）'
    )

    args = parser.parse_args()

    # 创建主控制器
    print(f"初始化自动化控制器...")
    print(f"项目目录: {os.path.abspath(args.project_dir)}")
    controller = AutonomousMasterController(args.project_dir)

    # 根据模式创建工作流
    workflow = None
    result = None

    if args.mode == 'comprehensive':
        print("\n执行综合自动化测试流程...")
        workflow = create_comprehensive_workflow(controller)
        result = controller.execute_workflow(workflow)

    elif args.mode == 'quick':
        print("\n执行快速测试流程...")
        workflow = create_quick_test_workflow(controller)
        result = controller.execute_workflow(workflow)

    elif args.mode == 'regression':
        if not args.baseline:
            print("错误: regression模式需要指定--baseline参数")
            return 1
        print(f"\n执行回归测试流程（基线: {args.baseline}）...")
        workflow = create_regression_only_workflow(controller, args.baseline)
        result = controller.execute_workflow(workflow)

    elif args.mode == 'auto-fix':
        print(f"\n执行自动修复流程（最大迭代: {args.max_iterations}次）...")
        result = controller.auto_fix_and_rebuild(max_iterations=args.max_iterations)

    # 打印执行摘要
    if result:
        print_workflow_summary(result)

        # 保存报告
        report_dir = Path(args.report_dir)
        report_dir.mkdir(exist_ok=True)

        timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        report_file = report_dir / f"workflow_{args.mode}_{timestamp}.json"

        with open(report_file, 'w', encoding='utf-8') as f:
            json.dump(result, f, ensure_ascii=False, indent=2)

        print(f"\n详细报告已保存到: {report_file}")

        # 返回状态码
        return 0 if result.get('overall_success', False) else 1
    else:
        print("错误: 工作流执行失败")
        return 1

if __name__ == "__main__":
    sys.exit(main())
