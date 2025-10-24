#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
代码覆盖率分析工具
集成OpenCppCoverage和自定义覆盖率分析
"""

import os
import re
import json
import subprocess
import logging
from typing import Dict, List, Optional, Set
from pathlib import Path
from datetime import datetime


class OpenCppCoverageIntegration:
    """OpenCppCoverage集成"""

    def __init__(self, opencppcoverage_path: str = 'OpenCppCoverage.exe'):
        """
        初始化OpenCppCoverage集成

        Args:
            opencppcoverage_path: OpenCppCoverage可执行文件路径
        """
        self.opencppcoverage_path = opencppcoverage_path

    def check_availability(self) -> bool:
        """检查OpenCppCoverage是否可用"""
        try:
            result = subprocess.run(
                [self.opencppcoverage_path, '--help'],
                capture_output=True,
                text=True,
                timeout=5
            )
            return result.returncode == 0 or 'OpenCppCoverage' in result.stdout
        except Exception:
            return False

    def measure_coverage(self, executable: str, args: Optional[List[str]] = None,
                        sources: Optional[List[str]] = None,
                        modules: Optional[List[str]] = None) -> Dict:
        """
        测量代码覆盖率

        Args:
            executable: 可执行文件路径
            args: 程序参数
            sources: 源代码目录列表
            modules: 模块列表

        Returns:
            覆盖率结果字典
        """
        if not self.check_availability():
            return {
                'success': False,
                'error': 'OpenCppCoverage不可用',
                'coverage': 0.0
            }

        if not os.path.exists(executable):
            return {
                'success': False,
                'error': f'可执行文件不存在: {executable}',
                'coverage': 0.0
            }

        # 构建命令
        cmd = [self.opencppcoverage_path]

        # 添加源代码目录
        if sources:
            for source_dir in sources:
                cmd.extend(['--sources', source_dir])
        else:
            cmd.extend(['--sources', '.'])

        # 添加模块
        if modules:
            for module in modules:
                cmd.extend(['--modules', module])

        # 输出格式
        output_dir = 'coverage_results'
        os.makedirs(output_dir, exist_ok=True)

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        html_output = os.path.join(output_dir, f'coverage_{timestamp}.html')
        json_output = os.path.join(output_dir, f'coverage_{timestamp}.json')

        cmd.extend(['--export_type', 'html:' + html_output])
        cmd.extend(['--export_type', 'json:' + json_output])

        # 添加可执行文件和参数
        cmd.append('--')
        cmd.append(executable)
        if args:
            cmd.extend(args)

        logging.info(f"运行OpenCppCoverage: {' '.join(cmd)}")

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=600  # 10分钟超时
            )

            # 解析输出
            coverage_info = self._parse_coverage_output(result.stdout)

            # 读取JSON报告
            coverage_data = None
            if os.path.exists(json_output):
                try:
                    with open(json_output, 'r', encoding='utf-8') as f:
                        coverage_data = json.load(f)
                except Exception as e:
                    logging.warning(f"无法解析覆盖率JSON: {e}")

            return {
                'success': True,
                'coverage_percentage': coverage_info.get('coverage', 0.0),
                'lines_covered': coverage_info.get('lines_covered', 0),
                'total_lines': coverage_info.get('total_lines', 0),
                'html_report': html_output,
                'json_report': json_output,
                'coverage_data': coverage_data,
                'stdout': result.stdout
            }

        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': '覆盖率测量超时',
                'coverage': 0.0
            }
        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'coverage': 0.0
            }

    def _parse_coverage_output(self, output: str) -> Dict:
        """解析OpenCppCoverage输出"""
        coverage_info = {
            'coverage': 0.0,
            'lines_covered': 0,
            'total_lines': 0
        }

        # 查找覆盖率百分比
        # 格式: "Covered lines: 123 / 456 (27.0%)"
        coverage_match = re.search(r'Covered lines:\s*(\d+)\s*/\s*(\d+)\s*\((\d+\.?\d*)%\)', output)
        if coverage_match:
            coverage_info['lines_covered'] = int(coverage_match.group(1))
            coverage_info['total_lines'] = int(coverage_match.group(2))
            coverage_info['coverage'] = float(coverage_match.group(3))

        return coverage_info


class ManualCoverageAnalyzer:
    """手动覆盖率分析器（基于测试执行日志）"""

    def __init__(self):
        """初始化手动覆盖率分析器"""
        self.source_files = {}
        self.executed_lines = {}

    def analyze_test_execution(self, test_log: str, source_dir: str) -> Dict:
        """
        基于测试执行日志分析覆盖率

        Args:
            test_log: 测试日志文件路径
            source_dir: 源代码目录

        Returns:
            覆盖率估算结果
        """
        # 收集源文件
        self._collect_source_files(source_dir)

        # 分析日志提取执行信息
        if os.path.exists(test_log):
            self._extract_execution_info(test_log)

        # 计算覆盖率
        coverage = self._calculate_coverage()

        return coverage

    def _collect_source_files(self, source_dir: str):
        """收集源文件"""
        source_path = Path(source_dir)

        for pattern in ['**/*.cpp', '**/*.c']:
            for filepath in source_path.glob(pattern):
                # 排除测试文件和第三方库
                if 'test' in str(filepath).lower() or 'third_party' in str(filepath).lower():
                    continue

                try:
                    with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
                        lines = f.readlines()

                    # 统计可执行代码行
                    executable_lines = set()
                    for i, line in enumerate(lines, 1):
                        stripped = line.strip()
                        # 跳过空行、注释、大括号等
                        if stripped and not stripped.startswith('//') and stripped not in ['{', '}', '};']:
                            executable_lines.add(i)

                    self.source_files[str(filepath)] = executable_lines
                except Exception as e:
                    logging.warning(f"无法读取文件 {filepath}: {e}")

    def _extract_execution_info(self, test_log: str):
        """从测试日志提取执行信息"""
        # 这是一个简化的实现
        # 实际应该通过调试符号、日志、或插桩来追踪执行
        try:
            with open(test_log, 'r', encoding='utf-8', errors='replace') as f:
                content = f.read()

            # 提取测试执行的文件和行号
            # 格式: file.cpp(line): message
            execution_pattern = re.compile(r'([^(]+)\((\d+)\)')

            for match in execution_pattern.finditer(content):
                filepath = match.group(1).strip()
                line = int(match.group(2))

                if filepath not in self.executed_lines:
                    self.executed_lines[filepath] = set()
                self.executed_lines[filepath].add(line)

        except Exception as e:
            logging.warning(f"无法解析测试日志: {e}")

    def _calculate_coverage(self) -> Dict:
        """计算覆盖率"""
        total_lines = sum(len(lines) for lines in self.source_files.values())
        covered_lines = 0

        file_coverage = {}

        for filepath, executable_lines in self.source_files.items():
            executed = self.executed_lines.get(filepath, set())
            covered = len(executed & executable_lines)
            total = len(executable_lines)

            file_coverage[filepath] = {
                'covered_lines': covered,
                'total_lines': total,
                'coverage_percentage': (covered / total * 100.0) if total > 0 else 0.0
            }

            covered_lines += covered

        overall_coverage = (covered_lines / total_lines * 100.0) if total_lines > 0 else 0.0

        return {
            'overall_coverage': overall_coverage,
            'total_executable_lines': total_lines,
            'covered_lines': covered_lines,
            'uncovered_lines': total_lines - covered_lines,
            'file_coverage': file_coverage,
            'files_analyzed': len(self.source_files)
        }


class CoverageAnalyzer:
    """代码覆盖率分析主类"""

    def __init__(self):
        """初始化覆盖率分析器"""
        self.opencppcoverage = OpenCppCoverageIntegration()
        self.manual_analyzer = ManualCoverageAnalyzer()
        self.report_dir = Path('coverage_reports')
        self.report_dir.mkdir(exist_ok=True)

        # 覆盖率目标
        self.coverage_targets = {
            'minimum': 70.0,   # 最低覆盖率
            'target': 90.0,    # 目标覆盖率
            'excellent': 95.0  # 优秀覆盖率
        }

    def measure_coverage(self, test_executable: str, test_args: Optional[List[str]] = None,
                        source_dirs: Optional[List[str]] = None) -> Dict:
        """
        测量代码覆盖率

        Args:
            test_executable: 测试可执行文件
            test_args: 测试参数
            source_dirs: 源代码目录

        Returns:
            覆盖率结果字典
        """
        logging.info("开始代码覆盖率分析...")

        result = {}

        # 尝试使用OpenCppCoverage
        if self.opencppcoverage.check_availability():
            logging.info("使用OpenCppCoverage测量覆盖率")
            result = self.opencppcoverage.measure_coverage(
                test_executable,
                args=test_args,
                sources=source_dirs
            )
        else:
            logging.warning("OpenCppCoverage不可用，使用手动分析")
            # 回退到手动分析
            result = {
                'success': False,
                'method': 'manual',
                'coverage_percentage': 0.0,
                'message': 'OpenCppCoverage不可用，无法精确测量覆盖率'
            }

        # 评估覆盖率
        if result.get('success'):
            coverage = result.get('coverage_percentage', 0.0)
            result['evaluation'] = self._evaluate_coverage(coverage)

        # 生成报告
        report_path = self._generate_report(result)
        result['report_path'] = str(report_path)

        return result

    def _evaluate_coverage(self, coverage: float) -> Dict:
        """评估覆盖率"""
        if coverage >= self.coverage_targets['excellent']:
            level = 'excellent'
            message = f"覆盖率优秀: {coverage:.1f}%"
            recommendations = ["继续保持高覆盖率"]
        elif coverage >= self.coverage_targets['target']:
            level = 'good'
            message = f"覆盖率良好: {coverage:.1f}%"
            recommendations = [
                f"继续提升，目标{self.coverage_targets['excellent']}%"
            ]
        elif coverage >= self.coverage_targets['minimum']:
            level = 'acceptable'
            message = f"覆盖率可接受: {coverage:.1f}%"
            recommendations = [
                f"需要提升，目标{self.coverage_targets['target']}%",
                "增加边界条件测试",
                "增加异常处理测试"
            ]
        else:
            level = 'insufficient'
            message = f"覆盖率不足: {coverage:.1f}%"
            recommendations = [
                f"严重不足，最低要求{self.coverage_targets['minimum']}%",
                "增加单元测试用例",
                "补充集成测试",
                "检查未覆盖的关键路径"
            ]

        return {
            'level': level,
            'message': message,
            'recommendations': recommendations,
            'meets_minimum': coverage >= self.coverage_targets['minimum'],
            'meets_target': coverage >= self.coverage_targets['target']
        }

    def _generate_report(self, coverage_result: Dict) -> Path:
        """生成覆盖率报告"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        report_path = self.report_dir / f"coverage_report_{timestamp}.json"

        report = {
            'timestamp': datetime.now().isoformat(),
            'coverage_result': coverage_result,
            'targets': self.coverage_targets
        }

        with open(report_path, 'w', encoding='utf-8') as f:
            json.dump(report, f, indent=2, ensure_ascii=False)

        logging.info(f"覆盖率报告已保存: {report_path}")

        return report_path

    def verify_coverage_target(self, coverage_result: Dict) -> bool:
        """
        验证是否满足覆盖率目标

        Args:
            coverage_result: 覆盖率结果

        Returns:
            True如果满足目标
        """
        if not coverage_result.get('success'):
            return False

        coverage = coverage_result.get('coverage_percentage', 0.0)
        return coverage >= self.coverage_targets['target']


if __name__ == '__main__':
    # 测试用例
    logging.basicConfig(level=logging.INFO)

    print("=== 代码覆盖率分析工具测试 ===\n")

    # 检查OpenCppCoverage可用性
    opencppcoverage = OpenCppCoverageIntegration()
    if opencppcoverage.check_availability():
        print("✓ OpenCppCoverage可用")
    else:
        print("✗ OpenCppCoverage不可用")
        print("  建议: 安装OpenCppCoverage以启用精确的覆盖率测量")
        print("  下载: https://github.com/OpenCppCoverage/OpenCppCoverage/releases")

    # 测试覆盖率评估
    analyzer = CoverageAnalyzer()

    test_coverages = [95.0, 85.0, 75.0, 60.0]

    print("\n覆盖率评估测试:")
    for coverage in test_coverages:
        evaluation = analyzer._evaluate_coverage(coverage)
        print(f"\n覆盖率 {coverage}%:")
        print(f"  级别: {evaluation['level']}")
        print(f"  消息: {evaluation['message']}")
        print(f"  满足最低要求: {'是' if evaluation['meets_minimum'] else '否'}")
        print(f"  满足目标: {'是' if evaluation['meets_target'] else '否'}")
        if evaluation['recommendations']:
            print("  建议:")
            for rec in evaluation['recommendations']:
                print(f"    - {rec}")
