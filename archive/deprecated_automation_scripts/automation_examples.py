#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
自动化系统使用示例
演示如何使用各种自动化工具和工作流
"""

import os
import sys
from pathlib import Path
from autonomous_master_controller import AutonomousMasterController, AutomationWorkflow

def example_1_quick_build_and_test():
    """示例1: 快速构建和单元测试"""
    print("="*80)
    print("示例1: 快速构建和单元测试")
    print("="*80)

    controller = AutonomousMasterController(".")

    # 方法1: 直接使用控制器方法
    print("\n方法1: 使用控制器的快速测试方法")
    result = controller.run_complete_test_cycle()

    if result['overall_success']:
        print("✅ 快速测试成功")
    else:
        print("❌ 快速测试失败")

    # 方法2: 自定义工作流
    print("\n方法2: 使用自定义工作流")
    steps = [
        {
            "name": "构建",
            "type": "build",
            "config": {"build_script": "autobuild_x86_debug.bat"},
            "enabled": True
        },
        {
            "name": "单元测试",
            "type": "test",
            "config": {"mode": "unit-tests"},
            "enabled": True
        }
    ]

    workflow = AutomationWorkflow("快速测试", steps)
    result = controller.execute_workflow(workflow)

    print(f"工作流执行完成: {result['workflow_name']}")
    print(f"状态: {'✅ 成功' if result['overall_success'] else '❌ 失败'}")

def example_2_auto_fix_cycle():
    """示例2: 自动修复循环"""
    print("\n" + "="*80)
    print("示例2: 自动修复循环")
    print("="*80)

    controller = AutonomousMasterController(".")

    # 执行自动修复，最多尝试3次
    result = controller.auto_fix_and_rebuild(max_iterations=3)

    print(f"\n自动修复结果:")
    print(f"- 总迭代次数: {result.get('total_iterations', 0)}")
    print(f"- 最终状态: {'✅ 成功' if result.get('final_build_success', False) else '❌ 失败'}")

    if result.get('fixes_applied'):
        print(f"- 应用的修复:")
        for fix in result['fixes_applied']:
            print(f"  - {fix}")

def example_3_comprehensive_workflow():
    """示例3: 综合测试工作流"""
    print("\n" + "="*80)
    print("示例3: 综合测试工作流")
    print("="*80)

    controller = AutonomousMasterController(".")

    # 创建综合工作流
    steps = [
        {
            "name": "静态分析",
            "type": "analyze",
            "description": "执行静态代码分析",
            "enabled": True
        },
        {
            "name": "清理构建",
            "type": "build",
            "config": {"clean": True, "build_script": "autobuild_x86_debug.bat"},
            "enabled": True
        },
        {
            "name": "单元测试",
            "type": "test",
            "config": {"mode": "unit-tests"},
            "enabled": True
        },
        {
            "name": "集成测试",
            "type": "test",
            "config": {"mode": "integration"},
            "enabled": True
        },
        {
            "name": "性能测试",
            "type": "test",
            "config": {"mode": "performance"},
            "enabled": True
        }
    ]

    workflow = AutomationWorkflow("综合测试", steps)
    result = controller.execute_workflow(workflow)

    # 打印详细结果
    print(f"\n工作流: {result['workflow_name']}")
    print(f"总步骤: {result['total_steps']}")
    print(f"成功: {result['successful_steps']}")
    print(f"失败: {result['failed_steps']}")
    print(f"耗时: {result['total_duration']:.2f}秒")

def example_4_conditional_workflow():
    """示例4: 条件执行工作流"""
    print("\n" + "="*80)
    print("示例4: 条件执行工作流（构建失败时自动修复）")
    print("="*80)

    controller = AutonomousMasterController(".")

    # 创建带条件的工作流
    steps = [
        {
            "name": "构建",
            "type": "build",
            "config": {"build_script": "autobuild_x86_debug.bat"},
            "enabled": True
        },
        {
            "name": "自动修复",
            "type": "fix",
            "description": "如果构建失败，自动修复",
            "condition": lambda ctx: not ctx.get('build', {}).get('success', False),
            "enabled": True
        },
        {
            "name": "重新构建",
            "type": "build",
            "description": "修复后重新构建",
            "condition": lambda ctx: not ctx.get('build', {}).get('success', False),
            "config": {"build_script": "autobuild_x86_debug.bat"},
            "enabled": True
        },
        {
            "name": "测试",
            "type": "test",
            "description": "构建成功后执行测试",
            "condition": lambda ctx: ctx.get('build', {}).get('success', False),
            "config": {"mode": "all"},
            "enabled": True
        }
    ]

    workflow = AutomationWorkflow("智能修复工作流", steps)
    result = controller.execute_workflow(workflow)

    print(f"\n工作流执行完成")
    print(f"跳过的步骤: {result.get('skipped_steps', 0)}")
    print(f"最终状态: {'✅ 成功' if result['overall_success'] else '❌ 失败'}")

def example_5_regression_testing():
    """示例5: 回归测试"""
    print("\n" + "="*80)
    print("示例5: 回归测试")
    print("="*80)

    controller = AutonomousMasterController(".")

    # 创建回归测试工作流
    steps = [
        {
            "name": "构建",
            "type": "build",
            "config": {"build_script": "autobuild_x86_debug.bat"},
            "enabled": True
        },
        {
            "name": "全部测试",
            "type": "test",
            "config": {"mode": "all"},
            "enabled": True
        },
        {
            "name": "回归测试",
            "type": "regression",
            "config": {
                "version": "current",
                "auto_regression": True
            },
            "enabled": True
        }
    ]

    workflow = AutomationWorkflow("回归测试流程", steps)
    result = controller.execute_workflow(workflow)

    # 检查回归结果
    regression_step = next(
        (step for step in result.get('step_results', [])
         if step['step_name'] == '回归测试'),
        None
    )

    if regression_step:
        has_regression = regression_step.get('result', {}).get('has_regression', False)
        if has_regression:
            print("⚠️ 检测到回归问题")
        else:
            print("✅ 无回归问题")

def example_6_custom_fix_strategy():
    """示例6: 自定义修复策略"""
    print("\n" + "="*80)
    print("示例6: 自定义修复策略")
    print("="*80)

    controller = AutonomousMasterController(".")

    # 添加自定义修复策略
    def custom_fix_strategy(error_info: dict) -> dict:
        """自定义修复策略示例"""
        if "LNK1104" in error_info.get('message', ''):
            return {
                "strategy": "custom_lnk1104_fix",
                "description": "关闭占用进程并重新构建",
                "actions": [
                    "taskkill /F /IM PortMaster.exe",
                    "timeout /t 2",
                    "autobuild_x86_debug.bat"
                ]
            }
        return {"strategy": "unknown"}

    # 注册自定义策略
    controller.fix_library.register_custom_strategy("LNK1104", custom_fix_strategy)

    # 执行自动修复
    result = controller.auto_fix_and_rebuild(max_iterations=2)

    print(f"修复结果: {'✅ 成功' if result.get('final_build_success') else '❌ 失败'}")

def main():
    """运行所有示例"""
    examples = [
        ("快速构建和测试", example_1_quick_build_and_test),
        ("自动修复循环", example_2_auto_fix_cycle),
        ("综合测试工作流", example_3_comprehensive_workflow),
        ("条件执行工作流", example_4_conditional_workflow),
        ("回归测试", example_5_regression_testing),
        ("自定义修复策略", example_6_custom_fix_strategy)
    ]

    print("PortMaster自动化系统使用示例")
    print("="*80)
    print("\n可用示例:")
    for idx, (name, _) in enumerate(examples, 1):
        print(f"  {idx}. {name}")
    print(f"  0. 运行所有示例")

    try:
        choice = input("\n请选择示例编号 (0-6): ").strip()

        if choice == "0":
            # 运行所有示例
            for name, func in examples:
                try:
                    func()
                except Exception as e:
                    print(f"❌ 示例 '{name}' 执行失败: {e}")
        elif choice.isdigit() and 1 <= int(choice) <= len(examples):
            # 运行指定示例
            name, func = examples[int(choice) - 1]
            func()
        else:
            print("无效的选择")
            return 1

    except KeyboardInterrupt:
        print("\n\n用户中断")
        return 1

    print("\n" + "="*80)
    print("示例演示完成")
    print("="*80)
    return 0

if __name__ == "__main__":
    sys.exit(main())
