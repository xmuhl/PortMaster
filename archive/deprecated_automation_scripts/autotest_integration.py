#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AutoTest集成模块
提供自动化测试执行和结果分析功能
"""

import os
import json
import subprocess
import logging
from typing import Dict, List, Optional
from pathlib import Path


class AutoTestIntegration:
    """AutoTest集成类"""

    def __init__(self, autotest_path: str = None):
        """
        初始化AutoTest集成

        Args:
            autotest_path: AutoTest可执行文件路径（可选，默认自动检测）
        """
        # 自动检测AutoTest.exe路径
        if autotest_path is None:
            possible_paths = [
                "./AutoTest/AutoTest.exe",
                "AutoTest/AutoTest.exe",
                os.path.join(os.getcwd(), "AutoTest", "AutoTest.exe")
            ]

            for path in possible_paths:
                if os.path.exists(path):
                    self.autotest_path = path
                    break
            else:
                # 如果都找不到，使用默认路径
                self.autotest_path = "./AutoTest/AutoTest.exe"
        else:
            self.autotest_path = autotest_path

        self.report_dir = "test_reports"
        os.makedirs(self.report_dir, exist_ok=True)

        logging.info(f"AutoTest路径: {self.autotest_path}")
        logging.info(f"AutoTest存在: {os.path.exists(self.autotest_path)}")

    def run_all_tests(self, report_name: Optional[str] = None) -> Dict:
        """
        运行所有测试套件

        Args:
            report_name: 报告文件名（可选）

        Returns:
            测试结果字典
        """
        return self._run_tests("--all", report_name or "full_test_report.json")

    def run_error_recovery_tests(self, report_name: Optional[str] = None) -> Dict:
        """
        运行错误恢复测试

        Args:
            report_name: 报告文件名（可选）

        Returns:
            测试结果字典
        """
        return self._run_tests("--error-recovery", report_name or "error_recovery_report.json")

    def run_performance_tests(self, report_name: Optional[str] = None) -> Dict:
        """
        运行性能测试

        Args:
            report_name: 报告文件名（可选）

        Returns:
            测试结果字典
        """
        return self._run_tests("--performance", report_name or "performance_report.json")

    def run_stress_tests(self, report_name: Optional[str] = None) -> Dict:
        """
        运行压力测试

        Args:
            report_name: 报告文件名（可选）

        Returns:
            测试结果字典
        """
        return self._run_tests("--stress", report_name or "stress_report.json")

    def run_unit_tests(self, report_name: Optional[str] = None) -> Dict:
        """
        运行单元测试

        Args:
            report_name: 报告文件名（可选）

        Returns:
            测试结果字典
        """
        # 当前版本的AutoTest可能不支持--unit-tests，使用--all作为备选
        try:
            return self._run_tests("--unit-tests", report_name or "unit_test_report.json")
        except Exception as e:
            logging.warning(f"--unit-tests参数不支持，使用--all: {e}")
            return self._run_tests("--all", report_name or "unit_test_report.json")

    def run_integration_tests(self, report_name: Optional[str] = None) -> Dict:
        """
        运行集成测试

        Args:
            report_name: 报告文件名（可选）

        Returns:
            测试结果字典
        """
        # 当前版本的AutoTest可能不支持--integration，使用--all作为备选
        try:
            return self._run_tests("--integration", report_name or "integration_test_report.json")
        except Exception as e:
            logging.warning(f"--integration参数不支持，使用--all: {e}")
            return self._run_tests("--all", report_name or "integration_test_report.json")

    def run_tests(self, mode: str, report_name: Optional[str] = None) -> Dict:
        """
        通用测试运行方法 - 支持任意测试模式

        Args:
            mode: 测试模式（unit-tests, integration, performance, stress, error-recovery, all等）
            report_name: 报告文件名（可选）

        Returns:
            测试结果字典
        """
        # 根据模式确定默认报告名
        default_names = {
            'unit-tests': 'unit_test_report.json',
            'integration': 'integration_test_report.json',
            'performance': 'performance_report.json',
            'stress': 'stress_report.json',
            'error-recovery': 'error_recovery_report.json',
            'all': 'full_test_report.json'
        }

        if report_name is None:
            report_name = default_names.get(mode, 'test_report.json')

        # 构建测试参数
        test_param = f"--{mode}" if not mode.startswith('--') else mode

        try:
            return self._run_tests(test_param, report_name)
        except Exception as e:
            logging.warning(f"测试参数 '{test_param}' 不支持，使用 --all: {e}")
            # 备选方案：使用--all参数
            return self._run_tests("--all", report_name)

    def _run_tests(self, test_mode: str, report_name: str) -> Dict:
        """
        执行测试

        Args:
            test_mode: 测试模式（--all, --error-recovery等）
            report_name: 报告文件名

        Returns:
            测试结果字典
        """
        if not os.path.exists(self.autotest_path):
            logging.error(f"AutoTest可执行文件不存在: {self.autotest_path}")
            return {
                'success': False,
                'error': 'AutoTest可执行文件不存在',
                'exit_code': -1
            }

        # 首先验证测试参数是否被支持
        if not self._verify_test_mode_supported(test_mode):
            logging.warning(f"测试参数 '{test_mode}' 不被支持，使用 '--all'")
            test_mode = "--all"

        report_path = os.path.join(self.report_dir, report_name)

        # 构建命令
        cmd = [self.autotest_path, test_mode, "--report", report_path]

        logging.info(f"执行测试: {' '.join(cmd)}")

        try:
            # 执行测试
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=600,  # 10分钟超时
                cwd=os.path.dirname(self.autotest_path) or '.'
            )

            # 解析输出
            stdout_lines = result.stdout.strip().split('\n') if result.stdout else []
            stderr_lines = result.stderr.strip().split('\n') if result.stderr else []

            # 读取JSON报告
            test_report = None
            if os.path.exists(report_path):
                try:
                    with open(report_path, 'r', encoding='utf-8') as f:
                        test_report = json.load(f)
                except Exception as e:
                    logging.warning(f"无法解析测试报告: {e}")

            # 分析结果
            analysis = self._analyze_test_results(test_report)

            return {
                'success': result.returncode == 0,
                'exit_code': result.returncode,
                'stdout': stdout_lines,
                'stderr': stderr_lines,
                'report_path': report_path,
                'test_report': test_report,
                'analysis': analysis
            }

        except subprocess.TimeoutExpired:
            logging.error("测试执行超时")
            return {
                'success': False,
                'error': '测试执行超时',
                'exit_code': -2
            }
        except Exception as e:
            logging.error(f"测试执行失败: {e}")
            return {
                'success': False,
                'error': str(e),
                'exit_code': -3
            }

    def _analyze_test_results(self, test_report: Optional[Dict]) -> Dict:
        """
        分析测试结果

        Args:
            test_report: 测试报告JSON

        Returns:
            分析结果字典
        """
        if not test_report:
            return {
                'has_report': False,
                'total_tests': 0,
                'passed_tests': 0,
                'failed_tests': 0,
                'success_rate': 0.0,
                'failures': []
            }

        # 提取测试运行信息
        test_run = test_report.get('test_run', {})
        summary = test_run.get('summary', {})
        results = test_run.get('results', [])

        total_tests = summary.get('total_tests', 0)
        passed_tests = summary.get('passed', 0)
        failed_tests = summary.get('failed', 0)
        success_rate = summary.get('success_rate', 0.0)

        # 提取失败的测试详情
        failures = []
        for result in results:
            if not result.get('passed', True):
                failures.append({
                    'suite': result.get('suite', ''),
                    'test': result.get('test', ''),
                    'error': result.get('error', ''),
                    'duration': result.get('duration', 0.0)
                })

        return {
            'has_report': True,
            'total_tests': total_tests,
            'passed_tests': passed_tests,
            'failed_tests': failed_tests,
            'success_rate': success_rate,
            'duration': test_run.get('duration_seconds', 0.0),
            'failures': failures,
            'all_passed': failed_tests == 0
        }

    def get_test_summary(self, report_path: str) -> Dict:
        """
        获取测试摘要

        Args:
            report_path: 报告文件路径

        Returns:
            测试摘要字典
        """
        if not os.path.exists(report_path):
            return {'error': '报告文件不存在'}

        try:
            with open(report_path, 'r', encoding='utf-8') as f:
                test_report = json.load(f)
                return self._analyze_test_results(test_report)
        except Exception as e:
            return {'error': f'无法读取报告: {e}'}

    def _verify_test_mode_supported(self, test_mode: str) -> bool:
        """
        验证测试模式是否被AutoTest支持

        Args:
            test_mode: 测试模式参数

        Returns:
            True如果参数被支持
        """
        # 从帮助信息中提取支持的参数
        try:
            result = subprocess.run(
                [self.autotest_path, "--help"],
                capture_output=True,
                text=True,
                timeout=10
            )

            if result.returncode == 0:
                help_text = result.stdout
                # 检查参数是否在帮助信息中
                if test_mode in help_text:
                    return True
            else:
                # 如果help命令失败，使用预定义的已知支持列表
                supported_modes = ["--all", "--error-recovery", "--performance", "--stress", "--help"]
                return test_mode in supported_modes

        except Exception as e:
            logging.warning(f"无法验证测试模式支持: {e}")
            # 默认支持的测试模式
            supported_modes = ["--all", "--error-recovery", "--performance", "--stress"]
            return test_mode in supported_modes

    def verify_autotest_ready(self) -> bool:
        """
        验证AutoTest是否就绪

        Returns:
            True如果AutoTest可用
        """
        if not os.path.exists(self.autotest_path):
            logging.error(f"AutoTest可执行文件不存在: {self.autotest_path}")
            return False

        # 尝试运行帮助命令
        try:
            result = subprocess.run(
                [self.autotest_path, "--help"],
                capture_output=True,
                text=True,
                timeout=10
            )
            return result.returncode == 0 or "AutoTest" in result.stdout
        except Exception as e:
            logging.error(f"AutoTest验证失败: {e}")
            return False


class TestValidator:
    """测试验证器 - 评估测试结果是否满足质量标准"""

    def __init__(self):
        """初始化验证器"""
        self.quality_thresholds = {
            'min_success_rate': 90.0,  # 最低成功率90%
            'max_failures': 2,          # 最多2个失败用例
            'critical_suites': [        # 关键测试套件必须全部通过
                'PacketLossTest',
                'TimeoutTest',
                'CRCFailureTest'
            ]
        }

    def validate_test_results(self, analysis: Dict) -> Dict:
        """
        验证测试结果是否满足质量标准

        Args:
            analysis: 测试分析结果

        Returns:
            验证结果字典
        """
        if not analysis.get('has_report', False):
            return {
                'passed': False,
                'reason': '缺少测试报告',
                'recommendations': ['重新运行测试']
            }

        issues = []
        recommendations = []

        # 检查成功率
        success_rate = analysis.get('success_rate', 0.0)
        if success_rate < self.quality_thresholds['min_success_rate']:
            issues.append(f"测试成功率过低: {success_rate:.1f}% < {self.quality_thresholds['min_success_rate']}%")
            recommendations.append('分析失败用例并修复')

        # 检查失败数量
        failed_tests = analysis.get('failed_tests', 0)
        if failed_tests > self.quality_thresholds['max_failures']:
            issues.append(f"失败用例过多: {failed_tests} > {self.quality_thresholds['max_failures']}")
            recommendations.append('优先修复高频失败用例')

        # 检查关键测试套件
        failures = analysis.get('failures', [])
        critical_failures = [
            f for f in failures
            if f.get('suite', '') in self.quality_thresholds['critical_suites']
        ]

        if critical_failures:
            issues.append(f"关键测试套件失败: {len(critical_failures)}个")
            for failure in critical_failures:
                recommendations.append(f"修复{failure['suite']}::{failure['test']}")

        # 综合判断
        passed = len(issues) == 0

        return {
            'passed': passed,
            'issues': issues,
            'recommendations': recommendations,
            'success_rate': success_rate,
            'failed_tests': failed_tests,
            'critical_failures': len(critical_failures)
        }


if __name__ == '__main__':
    # 测试用例
    logging.basicConfig(level=logging.INFO)

    print("=== AutoTest集成模块测试 ===\n")

    integration = AutoTestIntegration()

    # 验证AutoTest是否就绪
    if integration.verify_autotest_ready():
        print("✓ AutoTest就绪")
    else:
        print("✗ AutoTest未就绪")
        print(f"  AutoTest路径: {integration.autotest_path}")
        print(f"  文件存在: {os.path.exists(integration.autotest_path)}")

    # 测试报告分析
    validator = TestValidator()

    # 模拟测试报告
    mock_analysis = {
        'has_report': True,
        'total_tests': 15,
        'passed_tests': 14,
        'failed_tests': 1,
        'success_rate': 93.3,
        'failures': [
            {
                'suite': 'StressTest',
                'test': 'Large data stress (100MB)',
                'error': 'Timeout after 60s'
            }
        ]
    }

    validation = validator.validate_test_results(mock_analysis)
    print(f"\n验证结果: {'通过' if validation['passed'] else '失败'}")
    if validation['issues']:
        print("问题:")
        for issue in validation['issues']:
            print(f"  - {issue}")
    if validation['recommendations']:
        print("建议:")
        for rec in validation['recommendations']:
            print(f"  - {rec}")
