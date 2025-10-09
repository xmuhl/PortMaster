#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
静态代码分析工具
集成Cppcheck、代码复杂度分析、风格检查
"""

import os
import re
import json
import subprocess
import logging
from typing import Dict, List, Optional, Set
from pathlib import Path
from datetime import datetime


class CodeComplexityAnalyzer:
    """代码复杂度分析器"""

    def __init__(self):
        """初始化复杂度分析器"""
        self.complexity_thresholds = {
            'low': 10,      # 低复杂度
            'medium': 20,   # 中等复杂度
            'high': 30,     # 高复杂度
            'very_high': 40 # 极高复杂度
        }

    def analyze_file(self, filepath: str) -> Dict:
        """
        分析单个文件的复杂度

        Args:
            filepath: 文件路径

        Returns:
            分析结果字典
        """
        if not os.path.exists(filepath):
            return {'error': '文件不存在'}

        try:
            with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
                content = f.read()
        except Exception as e:
            return {'error': f'读取文件失败: {e}'}

        # 分析指标
        lines_of_code = self._count_lines_of_code(content)
        functions = self._extract_functions(content)
        complexity_metrics = self._calculate_complexity(content, functions)

        return {
            'file': filepath,
            'lines_of_code': lines_of_code,
            'function_count': len(functions),
            'functions': functions,
            'complexity': complexity_metrics,
            'issues': self._identify_complexity_issues(functions, complexity_metrics)
        }

    def _count_lines_of_code(self, content: str) -> int:
        """计算代码行数（排除空行和注释）"""
        lines = content.split('\n')
        code_lines = 0

        in_multiline_comment = False

        for line in lines:
            stripped = line.strip()

            # 跳过空行
            if not stripped:
                continue

            # 处理多行注释
            if '/*' in stripped:
                in_multiline_comment = True
            if '*/' in stripped:
                in_multiline_comment = False
                continue
            if in_multiline_comment:
                continue

            # 跳过单行注释
            if stripped.startswith('//'):
                continue

            code_lines += 1

        return code_lines

    def _extract_functions(self, content: str) -> List[Dict]:
        """提取函数定义"""
        functions = []

        # 匹配C++函数定义（简化版）
        # 格式: 返回类型 函数名(参数) { ... }
        function_pattern = re.compile(
            r'(?:^|\n)\s*(?:virtual\s+)?(?:static\s+)?'
            r'(\w+(?:\s*\*)?(?:\s*&)?)\s+'  # 返回类型
            r'(\w+)\s*'  # 函数名
            r'\((.*?)\)\s*(?:const\s*)?(?:override\s*)?'  # 参数
            r'(?:\{|;)',  # 函数体开始或声明结束
            re.MULTILINE
        )

        matches = function_pattern.finditer(content)

        for match in matches:
            return_type = match.group(1).strip()
            function_name = match.group(2).strip()
            parameters = match.group(3).strip()

            # 跳过常见的C++关键字
            if return_type in ['if', 'while', 'for', 'switch', 'catch']:
                continue

            # 提取函数体
            start_pos = match.end()
            function_body = self._extract_function_body(content, start_pos)

            if function_body:
                cyclomatic_complexity = self._calculate_cyclomatic_complexity(function_body)

                functions.append({
                    'name': function_name,
                    'return_type': return_type,
                    'parameters': parameters,
                    'lines': len(function_body.split('\n')),
                    'cyclomatic_complexity': cyclomatic_complexity
                })

        return functions

    def _extract_function_body(self, content: str, start_pos: int) -> Optional[str]:
        """提取函数体"""
        # 从起始位置找到第一个{
        brace_pos = content.find('{', start_pos)
        if brace_pos == -1:
            return None

        # 计数大括号以找到匹配的}
        brace_count = 1
        pos = brace_pos + 1
        body_end = -1

        while pos < len(content) and brace_count > 0:
            if content[pos] == '{':
                brace_count += 1
            elif content[pos] == '}':
                brace_count -= 1
                if brace_count == 0:
                    body_end = pos
                    break
            pos += 1

        if body_end == -1:
            return None

        return content[brace_pos:body_end + 1]

    def _calculate_cyclomatic_complexity(self, function_body: str) -> int:
        """
        计算圈复杂度
        基本公式: CC = 决策点数 + 1

        决策点包括:
        - if, else if
        - while, for, do
        - case
        - &&, ||
        - ? (三元运算符)
        - catch
        """
        complexity = 1  # 基础复杂度

        # 统计决策关键字
        decision_keywords = ['if', 'while', 'for', 'case', 'catch']
        for keyword in decision_keywords:
            # 使用单词边界匹配，避免误匹配
            pattern = r'\b' + keyword + r'\b'
            complexity += len(re.findall(pattern, function_body))

        # 统计逻辑运算符
        complexity += len(re.findall(r'&&|\|\|', function_body))

        # 统计三元运算符
        complexity += len(re.findall(r'\?', function_body))

        return complexity

    def _calculate_complexity(self, content: str, functions: List[Dict]) -> Dict:
        """计算整体复杂度指标"""
        total_complexity = sum(f['cyclomatic_complexity'] for f in functions)
        avg_complexity = total_complexity / len(functions) if functions else 0

        return {
            'total_cyclomatic_complexity': total_complexity,
            'average_cyclomatic_complexity': avg_complexity,
            'max_cyclomatic_complexity': max((f['cyclomatic_complexity'] for f in functions), default=0),
            'high_complexity_functions': [
                f['name'] for f in functions
                if f['cyclomatic_complexity'] > self.complexity_thresholds['high']
            ]
        }

    def _identify_complexity_issues(self, functions: List[Dict], complexity: Dict) -> List[Dict]:
        """识别复杂度问题"""
        issues = []

        # 检查高复杂度函数
        for func in functions:
            cc = func['cyclomatic_complexity']

            if cc > self.complexity_thresholds['very_high']:
                issues.append({
                    'severity': 'critical',
                    'type': 'very_high_complexity',
                    'function': func['name'],
                    'complexity': cc,
                    'message': f"函数{func['name']}圈复杂度过高: {cc} (建议<{self.complexity_thresholds['high']})",
                    'suggestion': '考虑拆分函数或简化逻辑'
                })
            elif cc > self.complexity_thresholds['high']:
                issues.append({
                    'severity': 'high',
                    'type': 'high_complexity',
                    'function': func['name'],
                    'complexity': cc,
                    'message': f"函数{func['name']}圈复杂度较高: {cc}",
                    'suggestion': '建议简化函数逻辑'
                })

        # 检查函数行数
        for func in functions:
            if func['lines'] > 100:
                issues.append({
                    'severity': 'medium',
                    'type': 'long_function',
                    'function': func['name'],
                    'lines': func['lines'],
                    'message': f"函数{func['name']}行数过多: {func['lines']}行",
                    'suggestion': '考虑拆分为多个小函数'
                })

        return issues


class CppcheckIntegration:
    """Cppcheck集成"""

    def __init__(self, cppcheck_path: str = 'cppcheck'):
        """
        初始化Cppcheck集成

        Args:
            cppcheck_path: Cppcheck可执行文件路径
        """
        self.cppcheck_path = cppcheck_path

    def check_availability(self) -> bool:
        """检查Cppcheck是否可用"""
        try:
            result = subprocess.run(
                [self.cppcheck_path, '--version'],
                capture_output=True,
                text=True,
                timeout=5
            )
            return result.returncode == 0
        except Exception:
            return False

    def analyze(self, target_path: str, enable_checks: Optional[List[str]] = None) -> Dict:
        """
        运行Cppcheck分析

        Args:
            target_path: 目标路径（文件或目录）
            enable_checks: 启用的检查类型列表

        Returns:
            分析结果字典
        """
        if not self.check_availability():
            return {
                'success': False,
                'error': 'Cppcheck不可用',
                'issues': []
            }

        # 构建命令
        cmd = [
            self.cppcheck_path,
            '--enable=all',
            '--xml',
            '--xml-version=2',
            '--inline-suppr',
            target_path
        ]

        if enable_checks:
            cmd[1] = f"--enable={','.join(enable_checks)}"

        logging.info(f"运行Cppcheck: {' '.join(cmd)}")

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=300  # 5分钟超时
            )

            # Cppcheck输出在stderr
            xml_output = result.stderr

            # 解析XML结果
            issues = self._parse_cppcheck_xml(xml_output)

            return {
                'success': True,
                'issues': issues,
                'total_issues': len(issues)
            }

        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': 'Cppcheck超时',
                'issues': []
            }
        except Exception as e:
            return {
                'success': False,
                'error': str(e),
                'issues': []
            }

    def _parse_cppcheck_xml(self, xml_output: str) -> List[Dict]:
        """解析Cppcheck XML输出"""
        issues = []

        # 简单的正则解析（完整实现应使用XML解析库）
        error_pattern = re.compile(
            r'<error\s+id="([^"]+)".*?'
            r'severity="([^"]+)".*?'
            r'msg="([^"]+)".*?'
            r'file="([^"]+)".*?'
            r'line="([^"]+)"',
            re.DOTALL
        )

        for match in error_pattern.finditer(xml_output):
            issues.append({
                'id': match.group(1),
                'severity': match.group(2),
                'message': match.group(3),
                'file': match.group(4),
                'line': int(match.group(5)) if match.group(5).isdigit() else 0
            })

        return issues


class StaticAnalyzer:
    """静态代码分析主类"""

    def __init__(self, project_root: str = '.'):
        """
        初始化静态分析器

        Args:
            project_root: 项目根目录
        """
        self.project_root = Path(project_root)
        self.complexity_analyzer = CodeComplexityAnalyzer()
        self.cppcheck = CppcheckIntegration()
        self.report_dir = Path('static_analysis_reports')
        self.report_dir.mkdir(exist_ok=True)

    def analyze_project(self, include_patterns: Optional[List[str]] = None,
                       exclude_patterns: Optional[List[str]] = None) -> Dict:
        """
        分析整个项目

        Args:
            include_patterns: 包含的文件模式（如['*.cpp', '*.h']）
            exclude_patterns: 排除的文件模式

        Returns:
            分析结果字典
        """
        if include_patterns is None:
            include_patterns = ['*.cpp', '*.h', '*.c', '*.hpp']

        if exclude_patterns is None:
            exclude_patterns = [
                'build/*', '.vs/*', 'Debug/*', 'Release/*',
                'x64/*', 'Win32/*', 'obj/*', 'bin/*'
            ]

        logging.info("开始静态代码分析...")

        # 收集源文件
        source_files = self._collect_source_files(include_patterns, exclude_patterns)

        logging.info(f"找到 {len(source_files)} 个源文件")

        # 复杂度分析
        complexity_results = []
        for filepath in source_files:
            result = self.complexity_analyzer.analyze_file(str(filepath))
            if 'error' not in result:
                complexity_results.append(result)

        # Cppcheck分析
        cppcheck_result = None
        if self.cppcheck.check_availability():
            logging.info("运行Cppcheck分析...")
            cppcheck_result = self.cppcheck.analyze(str(self.project_root))
        else:
            logging.warning("Cppcheck不可用，跳过Cppcheck分析")

        # 汇总结果
        analysis_result = {
            'timestamp': datetime.now().isoformat(),
            'project_root': str(self.project_root),
            'source_files_analyzed': len(source_files),
            'complexity_analysis': {
                'files': complexity_results,
                'summary': self._summarize_complexity(complexity_results)
            },
            'cppcheck_analysis': cppcheck_result,
            'recommendations': self._generate_recommendations(complexity_results, cppcheck_result)
        }

        # 生成报告
        report_path = self._generate_report(analysis_result)
        analysis_result['report_path'] = str(report_path)

        return analysis_result

    def _collect_source_files(self, include_patterns: List[str],
                              exclude_patterns: List[str]) -> List[Path]:
        """收集源文件"""
        source_files = set()

        for pattern in include_patterns:
            files = self.project_root.rglob(pattern)
            source_files.update(files)

        # 排除文件
        filtered_files = []
        for filepath in source_files:
            should_exclude = False
            for exclude_pattern in exclude_patterns:
                if filepath.match(exclude_pattern):
                    should_exclude = True
                    break
            if not should_exclude:
                filtered_files.append(filepath)

        return filtered_files

    def _summarize_complexity(self, complexity_results: List[Dict]) -> Dict:
        """汇总复杂度分析结果"""
        if not complexity_results:
            return {}

        total_loc = sum(r['lines_of_code'] for r in complexity_results)
        total_functions = sum(r['function_count'] for r in complexity_results)

        all_issues = []
        for result in complexity_results:
            all_issues.extend(result.get('issues', []))

        critical_issues = [i for i in all_issues if i['severity'] == 'critical']
        high_issues = [i for i in all_issues if i['severity'] == 'high']

        return {
            'total_files': len(complexity_results),
            'total_lines_of_code': total_loc,
            'total_functions': total_functions,
            'total_issues': len(all_issues),
            'critical_issues': len(critical_issues),
            'high_issues': len(high_issues),
            'medium_issues': len(all_issues) - len(critical_issues) - len(high_issues)
        }

    def _generate_recommendations(self, complexity_results: List[Dict],
                                 cppcheck_result: Optional[Dict]) -> List[str]:
        """生成改进建议"""
        recommendations = []

        # 基于复杂度的建议
        all_issues = []
        for result in complexity_results:
            all_issues.extend(result.get('issues', []))

        critical_issues = [i for i in all_issues if i['severity'] == 'critical']
        if critical_issues:
            recommendations.append(f"发现{len(critical_issues)}个极高复杂度函数，强烈建议重构")

        high_issues = [i for i in all_issues if i['severity'] == 'high']
        if high_issues:
            recommendations.append(f"发现{len(high_issues)}个高复杂度函数，建议简化逻辑")

        # 基于Cppcheck的建议
        if cppcheck_result and cppcheck_result.get('success'):
            issues = cppcheck_result.get('issues', [])
            if issues:
                error_count = len([i for i in issues if i['severity'] == 'error'])
                warning_count = len([i for i in issues if i['severity'] == 'warning'])

                if error_count > 0:
                    recommendations.append(f"Cppcheck发现{error_count}个错误，需要立即修复")
                if warning_count > 0:
                    recommendations.append(f"Cppcheck发现{warning_count}个警告，建议检查")

        if not recommendations:
            recommendations.append("代码质量良好，未发现严重问题")

        return recommendations

    def _generate_report(self, analysis_result: Dict) -> Path:
        """生成分析报告"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        report_path = self.report_dir / f"static_analysis_{timestamp}.json"

        with open(report_path, 'w', encoding='utf-8') as f:
            json.dump(analysis_result, f, indent=2, ensure_ascii=False)

        logging.info(f"分析报告已保存: {report_path}")

        return report_path


if __name__ == '__main__':
    # 测试用例
    logging.basicConfig(level=logging.INFO)

    print("=== 静态代码分析工具测试 ===\n")

    # 测试复杂度分析器
    analyzer = CodeComplexityAnalyzer()

    test_code = """
    int calculate(int x, int y) {
        if (x > 0) {
            if (y > 0) {
                return x + y;
            } else if (y < 0) {
                return x - y;
            } else {
                return x;
            }
        } else if (x < 0) {
            return -x;
        } else {
            return 0;
        }
    }
    """

    # 创建临时测试文件
    test_file = 'test_complexity.cpp'
    with open(test_file, 'w', encoding='utf-8') as f:
        f.write(test_code)

    result = analyzer.analyze_file(test_file)

    print(f"文件: {result['file']}")
    print(f"代码行数: {result['lines_of_code']}")
    print(f"函数数量: {result['function_count']}")
    print("\n函数列表:")
    for func in result['functions']:
        print(f"  - {func['name']}: 圈复杂度={func['cyclomatic_complexity']}, 行数={func['lines']}")

    if result['issues']:
        print("\n问题:")
        for issue in result['issues']:
            print(f"  [{issue['severity']}] {issue['message']}")

    # 清理测试文件
    os.remove(test_file)

    # 测试Cppcheck
    print("\nCppcheck可用性:", "✓" if CppcheckIntegration().check_availability() else "✗")
