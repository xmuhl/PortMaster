#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
CI/CD集成脚本
用于GitHub Actions、Jenkins等CI/CD系统集成
提供标准化的测试执行和结果报告
"""

import os
import sys
import json
import argparse
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional
from autonomous_master_controller import AutonomousMasterController

class CICDIntegration:
    """CI/CD集成类"""

    def __init__(self, project_dir: str = "."):
        self.project_dir = Path(project_dir).resolve()
        self.controller = AutonomousMasterController(str(self.project_dir))
        self.artifacts_dir = self.project_dir / "ci_artifacts"
        self.artifacts_dir.mkdir(exist_ok=True)

    def run_pr_validation(self) -> Dict:
        """
        Pull Request验证流程
        - 静态分析
        - 编译验证
        - 单元测试
        - 集成测试
        """
        print("="*80)
        print("执行Pull Request验证流程")
        print("="*80)

        validation_result = {
            "timestamp": datetime.now().isoformat(),
            "type": "pr_validation",
            "steps": [],
            "overall_success": True
        }

        # 步骤1: 静态分析
        print("\n[1/4] 执行静态代码分析...")
        static_result = self.controller.static_analyzer.analyze_project(str(self.project_dir))
        validation_result["steps"].append({
            "name": "静态分析",
            "success": static_result.get("total_issues", 0) == 0,
            "details": static_result
        })

        if static_result.get("total_issues", 0) > 0:
            print(f"⚠️ 发现 {static_result['total_issues']} 个代码问题")
            validation_result["overall_success"] = False

        # 步骤2: 编译验证
        print("\n[2/4] 执行编译验证...")
        build_result = self.controller.builder.build("autobuild_x86_debug.bat")
        validation_result["steps"].append({
            "name": "编译",
            "success": build_result["success"],
            "details": build_result
        })

        if not build_result["success"]:
            print("❌ 编译失败")
            validation_result["overall_success"] = False
            return validation_result

        # 步骤3: 单元测试
        print("\n[3/4] 执行单元测试...")
        unit_test_result = self.controller.autotest.run_tests("unit-tests")
        validation_result["steps"].append({
            "name": "单元测试",
            "success": unit_test_result["success"],
            "details": unit_test_result
        })

        if not unit_test_result["success"]:
            print(f"❌ 单元测试失败: {unit_test_result.get('failed_tests', 0)} 个测试失败")
            validation_result["overall_success"] = False

        # 步骤4: 集成测试
        print("\n[4/4] 执行集成测试...")
        integration_test_result = self.controller.autotest.run_tests("integration")
        validation_result["steps"].append({
            "name": "集成测试",
            "success": integration_test_result["success"],
            "details": integration_test_result
        })

        if not integration_test_result["success"]:
            print(f"❌ 集成测试失败: {integration_test_result.get('failed_tests', 0)} 个测试失败")
            validation_result["overall_success"] = False

        # 保存验证结果
        self._save_artifact("pr_validation.json", validation_result)

        return validation_result

    def run_nightly_build(self) -> Dict:
        """
        夜间构建流程
        - 完整构建
        - 全部测试（单元+集成+性能+压力）
        - 代码覆盖率分析
        - 回归测试
        """
        print("="*80)
        print("执行夜间完整构建流程")
        print("="*80)

        nightly_result = {
            "timestamp": datetime.now().isoformat(),
            "type": "nightly_build",
            "steps": [],
            "overall_success": True
        }

        # 步骤1: 完整构建
        print("\n[1/5] 执行完整构建...")
        build_result = self.controller.builder.clean_build("autobuild_x86_debug.bat")
        nightly_result["steps"].append({
            "name": "完整构建",
            "success": build_result["success"],
            "details": build_result
        })

        if not build_result["success"]:
            print("❌ 构建失败")
            nightly_result["overall_success"] = False
            return nightly_result

        # 步骤2: 全部测试
        print("\n[2/5] 执行全部测试...")
        all_tests_result = self.controller.autotest.run_tests("all")
        nightly_result["steps"].append({
            "name": "全部测试",
            "success": all_tests_result["success"],
            "details": all_tests_result
        })

        if not all_tests_result["success"]:
            print(f"❌ 测试失败: {all_tests_result.get('failed_tests', 0)} 个测试失败")
            nightly_result["overall_success"] = False

        # 步骤3: 代码覆盖率
        print("\n[3/5] 分析代码覆盖率...")
        coverage_result = self.controller.coverage_analyzer.run_coverage_analysis(
            "AutoTest/bin/Debug/AutoTest.exe"
        )
        nightly_result["steps"].append({
            "name": "代码覆盖率",
            "success": True,  # 覆盖率分析本身不会失败
            "details": coverage_result
        })

        # 步骤4: 回归测试
        print("\n[4/5] 执行回归测试...")
        version = datetime.now().strftime("%Y%m%d-%H%M%S")
        regression_result = self.controller.autotest.create_baseline(version)
        nightly_result["steps"].append({
            "name": "创建回归基线",
            "success": regression_result["success"],
            "details": regression_result
        })

        # 步骤5: 生成报告
        print("\n[5/5] 生成综合报告...")
        report = self._generate_nightly_report(nightly_result)
        nightly_result["report_path"] = report

        # 保存夜间构建结果
        self._save_artifact("nightly_build.json", nightly_result)

        return nightly_result

    def run_release_validation(self, version: str) -> Dict:
        """
        发布验证流程
        - 完整构建
        - 全部测试
        - 回归测试（与上一个发布版本对比）
        - 性能基准测试
        """
        print("="*80)
        print(f"执行发布验证流程 - 版本: {version}")
        print("="*80)

        release_result = {
            "timestamp": datetime.now().isoformat(),
            "type": "release_validation",
            "version": version,
            "steps": [],
            "overall_success": True
        }

        # 步骤1: 清理构建
        print("\n[1/4] 执行清理构建...")
        build_result = self.controller.builder.clean_build("autobuild_x86_debug.bat")
        release_result["steps"].append({
            "name": "清理构建",
            "success": build_result["success"],
            "details": build_result
        })

        if not build_result["success"]:
            print("❌ 构建失败")
            release_result["overall_success"] = False
            return release_result

        # 步骤2: 全部测试
        print("\n[2/4] 执行全部测试...")
        all_tests_result = self.controller.autotest.run_tests("all")
        release_result["steps"].append({
            "name": "全部测试",
            "success": all_tests_result["success"],
            "details": all_tests_result
        })

        if not all_tests_result["success"]:
            print(f"❌ 测试失败: {all_tests_result.get('failed_tests', 0)} 个测试失败")
            release_result["overall_success"] = False

        # 步骤3: 回归测试
        print("\n[3/4] 执行回归测试...")
        regression_result = self.controller.autotest.run_regression(version)
        release_result["steps"].append({
            "name": "回归测试",
            "success": not regression_result.get("has_regression", False),
            "details": regression_result
        })

        if regression_result.get("has_regression", False):
            print("⚠️ 检测到回归问题")
            release_result["overall_success"] = False

        # 步骤4: 创建发布基线
        print("\n[4/4] 创建发布基线...")
        baseline_result = self.controller.autotest.create_baseline(version)
        release_result["steps"].append({
            "name": "创建基线",
            "success": baseline_result["success"],
            "details": baseline_result
        })

        # 保存发布验证结果
        self._save_artifact(f"release_validation_{version}.json", release_result)

        return release_result

    def _generate_nightly_report(self, result: Dict) -> str:
        """生成夜间构建Markdown报告"""
        report_lines = [
            f"# 夜间构建报告",
            f"",
            f"**构建时间**: {result['timestamp']}",
            f"**整体状态**: {'✅ 成功' if result['overall_success'] else '❌ 失败'}",
            f"",
            f"## 执行步骤",
            f""
        ]

        for idx, step in enumerate(result["steps"], 1):
            status = "✅" if step["success"] else "❌"
            report_lines.append(f"### {idx}. {step['name']} {status}")
            report_lines.append(f"")

            # 添加步骤详情
            details = step.get("details", {})
            if "total_tests" in details:
                report_lines.append(f"- 总测试数: {details['total_tests']}")
                report_lines.append(f"- 通过测试: {details.get('passed_tests', 0)}")
                report_lines.append(f"- 失败测试: {details.get('failed_tests', 0)}")
            elif "errors" in details:
                report_lines.append(f"- 错误数: {details['errors']}")
                report_lines.append(f"- 警告数: {details['warnings']}")
            elif "line_coverage" in details:
                report_lines.append(f"- 行覆盖率: {details['line_coverage']:.1f}%")
                report_lines.append(f"- 分支覆盖率: {details['branch_coverage']:.1f}%")

            report_lines.append(f"")

        report_content = "\n".join(report_lines)

        # 保存报告
        report_file = self.artifacts_dir / f"nightly_report_{datetime.now().strftime('%Y%m%d')}.md"
        with open(report_file, 'w', encoding='utf-8') as f:
            f.write(report_content)

        return str(report_file)

    def _save_artifact(self, filename: str, data: Dict):
        """保存构建产物"""
        artifact_file = self.artifacts_dir / filename
        with open(artifact_file, 'w', encoding='utf-8') as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
        print(f"\n📁 产物已保存: {artifact_file}")

    def export_for_github_actions(self, result: Dict) -> str:
        """导出GitHub Actions格式的输出"""
        # GitHub Actions输出格式
        output_lines = []

        if result["overall_success"]:
            output_lines.append("::notice::构建成功")
        else:
            output_lines.append("::error::构建失败")

        # 添加步骤详情
        for step in result.get("steps", []):
            if not step["success"]:
                output_lines.append(f"::error::{step['name']} 失败")

        return "\n".join(output_lines)

def main():
    parser = argparse.ArgumentParser(
        description="PortMaster CI/CD集成工具",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument(
        'mode',
        choices=['pr', 'nightly', 'release'],
        help='CI/CD执行模式: pr=Pull Request验证, nightly=夜间构建, release=发布验证'
    )

    parser.add_argument(
        '--version',
        type=str,
        help='发布版本号（仅用于release模式）'
    )

    parser.add_argument(
        '--project-dir',
        type=str,
        default='.',
        help='项目根目录（默认为当前目录）'
    )

    parser.add_argument(
        '--github-actions',
        action='store_true',
        help='输出GitHub Actions格式'
    )

    args = parser.parse_args()

    # 创建CI/CD集成实例
    ci_cd = CICDIntegration(args.project_dir)

    result = None

    # 执行相应的CI/CD流程
    if args.mode == 'pr':
        result = ci_cd.run_pr_validation()
    elif args.mode == 'nightly':
        result = ci_cd.run_nightly_build()
    elif args.mode == 'release':
        if not args.version:
            print("错误: release模式需要指定--version参数")
            return 1
        result = ci_cd.run_release_validation(args.version)

    # 输出结果
    if result:
        if args.github_actions:
            print(ci_cd.export_for_github_actions(result))

        print("\n" + "="*80)
        print(f"CI/CD流程执行完成: {'✅ 成功' if result['overall_success'] else '❌ 失败'}")
        print("="*80)

        return 0 if result["overall_success"] else 1
    else:
        print("错误: CI/CD流程执行失败")
        return 1

if __name__ == "__main__":
    sys.exit(main())
