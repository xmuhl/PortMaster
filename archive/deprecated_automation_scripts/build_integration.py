#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
智能编译集成模块
提供自动化编译、错误分析和修复建议功能
"""

import os
import re
import subprocess
import logging
from typing import Dict, List, Optional, Tuple
from datetime import datetime


class CompilationError:
    """编译错误类"""

    def __init__(self, error_type: str, file: str, line: int, code: str, message: str):
        """
        初始化编译错误

        Args:
            error_type: 错误类型（error/warning）
            file: 文件路径
            line: 行号
            code: 错误代码（如C2039, LNK1104）
            message: 错误消息
        """
        self.error_type = error_type
        self.file = file
        self.line = line
        self.code = code
        self.message = message

    def __repr__(self):
        return f"{self.file}({self.line}): {self.error_type} {self.code}: {self.message}"


class BuildIntegration:
    """智能编译集成类"""

    def __init__(self, build_script: str = "autobuild_x86_debug.bat"):
        """
        初始化编译集成

        Args:
            build_script: 编译脚本路径
        """
        self.build_script = build_script
        self.log_dir = "build_logs"
        os.makedirs(self.log_dir, exist_ok=True)

        # 错误模式匹配
        self.error_patterns = [
            # Visual Studio错误格式: file(line): error CODE: message
            re.compile(
                r'(?P<file>[^(]+)\((?P<line>\d+)\):\s*(?P<type>error|warning)\s+(?P<code>[A-Z]+\d+):\s*(?P<message>.*)',
                re.IGNORECASE
            ),
            # 链接器错误: LINK : error CODE: message
            re.compile(
                r'LINK\s*:\s*(?P<type>error|warning)\s+(?P<code>LNK\d+):\s*(?P<message>.*)',
                re.IGNORECASE
            ),
            # fatal error格式
            re.compile(
                r'(?P<file>[^(]+)\((?P<line>\d+)\):\s*(?P<type>fatal error)\s+(?P<code>[A-Z]+\d+):\s*(?P<message>.*)',
                re.IGNORECASE
            ),
        ]

    def build(self, clean: bool = False, timeout: int = 600) -> Dict:
        """
        执行编译

        Args:
            clean: 是否清理后重新编译
            timeout: 超时时间（秒）

        Returns:
            编译结果字典
        """
        if not os.path.exists(self.build_script):
            logging.error(f"编译脚本不存在: {self.build_script}")
            return {
                'success': False,
                'error': '编译脚本不存在',
                'exit_code': -1
            }

        # 生成日志文件名
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        log_file = os.path.join(self.log_dir, f"build_{timestamp}.log")

        logging.info(f"开始编译（clean={clean}）...")

        try:
            # 如果需要清理，先执行清理
            if clean:
                self._clean_build()

            # 执行编译
            result = subprocess.run(
                [self.build_script],
                capture_output=True,
                text=True,
                timeout=timeout,
                shell=True,
                encoding='utf-8',
                errors='replace'  # 处理编码错误
            )

            # 保存完整日志
            full_output = result.stdout + result.stderr
            with open(log_file, 'w', encoding='utf-8') as f:
                f.write(full_output)

            # 解析错误和警告
            errors, warnings = self._parse_build_output(full_output)

            # 分析结果
            analysis = self._analyze_build_result(errors, warnings)

            success = result.returncode == 0 and len(errors) == 0

            return {
                'success': success,
                'exit_code': result.returncode,
                'errors': errors,
                'warnings': warnings,
                'error_count': len(errors),
                'warning_count': len(warnings),
                'log_file': log_file,
                'analysis': analysis,
                'clean_build': clean
            }

        except subprocess.TimeoutExpired:
            logging.error("编译超时")
            return {
                'success': False,
                'error': '编译超时',
                'exit_code': -2,
                'timeout': timeout
            }
        except Exception as e:
            logging.error(f"编译执行失败: {e}")
            return {
                'success': False,
                'error': str(e),
                'exit_code': -3
            }

    def _clean_build(self):
        """清理编译产物"""
        logging.info("清理编译产物...")

        # 删除常见的编译产物目录
        dirs_to_clean = ['build', '.vs', 'Debug', 'Release', 'x64', 'Win32']

        for dir_name in dirs_to_clean:
            if os.path.exists(dir_name):
                try:
                    import shutil
                    shutil.rmtree(dir_name)
                    logging.info(f"已删除目录: {dir_name}")
                except Exception as e:
                    logging.warning(f"无法删除目录 {dir_name}: {e}")

    def _parse_build_output(self, output: str) -> Tuple[List[CompilationError], List[CompilationError]]:
        """
        解析编译输出，提取错误和警告

        Args:
            output: 编译输出文本

        Returns:
            (错误列表, 警告列表)
        """
        errors = []
        warnings = []

        for line in output.split('\n'):
            for pattern in self.error_patterns:
                match = pattern.search(line)
                if match:
                    groups = match.groupdict()

                    error = CompilationError(
                        error_type=groups.get('type', 'error').lower(),
                        file=groups.get('file', '').strip(),
                        line=int(groups.get('line', 0)) if groups.get('line') else 0,
                        code=groups.get('code', '').strip(),
                        message=groups.get('message', '').strip()
                    )

                    if 'error' in error.error_type or 'fatal' in error.error_type:
                        errors.append(error)
                    else:
                        warnings.append(error)

                    break

        return errors, warnings

    def _analyze_build_result(self, errors: List[CompilationError], warnings: List[CompilationError]) -> Dict:
        """
        分析编译结果

        Args:
            errors: 错误列表
            warnings: 警告列表

        Returns:
            分析结果字典
        """
        analysis = {
            'total_errors': len(errors),
            'total_warnings': len(warnings),
            'error_types': {},
            'warning_types': {},
            'most_common_errors': [],
            'affected_files': set(),
            'critical_errors': []
        }

        # 统计错误类型
        for error in errors:
            analysis['error_types'][error.code] = analysis['error_types'].get(error.code, 0) + 1
            analysis['affected_files'].add(error.file)

            # 识别关键错误
            if error.code in ['LNK1104', 'LNK2001', 'LNK2019', 'C1083']:
                analysis['critical_errors'].append(error)

        # 统计警告类型
        for warning in warnings:
            analysis['warning_types'][warning.code] = analysis['warning_types'].get(warning.code, 0) + 1

        # 找出最常见的错误
        if analysis['error_types']:
            sorted_errors = sorted(
                analysis['error_types'].items(),
                key=lambda x: x[1],
                reverse=True
            )
            analysis['most_common_errors'] = sorted_errors[:5]

        analysis['affected_files'] = list(analysis['affected_files'])

        return analysis

    def get_fix_suggestions(self, errors: List[CompilationError]) -> List[Dict]:
        """
        获取修复建议

        Args:
            errors: 错误列表

        Returns:
            修复建议列表
        """
        suggestions = []

        for error in errors:
            suggestion = self._get_error_fix_suggestion(error)
            if suggestion:
                suggestions.append(suggestion)

        return suggestions

    def _get_error_fix_suggestion(self, error: CompilationError) -> Optional[Dict]:
        """
        获取单个错误的修复建议

        Args:
            error: 编译错误

        Returns:
            修复建议字典
        """
        code = error.code

        # LNK1104: 无法打开文件
        if code == 'LNK1104':
            return {
                'error': str(error),
                'priority': 'HIGH',
                'suggestions': [
                    '关闭所有正在运行的程序实例',
                    '执行清理编译（clean build）',
                    '检查文件是否被其他进程占用'
                ],
                'auto_fix': [
                    {'action': 'kill_process', 'target': 'PortMaster.exe'},
                    {'action': 'clean_build'}
                ]
            }

        # C2039: 成员不存在
        elif code == 'C2039':
            match = re.search(r'"(\w+)".*?"(\w+)"', error.message)
            if match:
                member, class_name = match.groups()
                return {
                    'error': str(error),
                    'priority': 'HIGH',
                    'suggestions': [
                        f'检查类{class_name}的定义',
                        f'确认成员{member}是否存在',
                        '检查头文件包含是否正确',
                        '检查API版本是否匹配'
                    ],
                    'auto_fix': [
                        {'action': 'code_search', 'pattern': f'class {class_name}'},
                        {'action': 'suggest_api_alternative', 'member': member}
                    ]
                }

        # C2065: 未声明的标识符
        elif code == 'C2065':
            match = re.search(r'"(\w+)"', error.message)
            if match:
                identifier = match.group(1)
                return {
                    'error': str(error),
                    'priority': 'MEDIUM',
                    'suggestions': [
                        f'添加{identifier}的声明',
                        '包含相应的头文件',
                        '检查命名空间'
                    ],
                    'auto_fix': [
                        {'action': 'search_header', 'identifier': identifier},
                        {'action': 'suggest_include'}
                    ]
                }

        # C4819: 编码问题
        elif code == 'C4819':
            return {
                'error': str(error),
                'priority': 'LOW',
                'suggestions': [
                    '将文件编码改为UTF-8 with BOM',
                    '使用Visual Studio的"高级保存选项"'
                ],
                'auto_fix': [
                    {'action': 'fix_encoding', 'file': error.file, 'encoding': 'utf-8-sig'}
                ]
            }

        return None

    def verify_build_success(self, build_result: Dict) -> bool:
        """
        验证编译是否成功（0 error 0 warning标准）

        Args:
            build_result: 编译结果

        Returns:
            True如果满足质量标准
        """
        return (
            build_result.get('success', False) and
            build_result.get('error_count', 1) == 0 and
            build_result.get('warning_count', 1) == 0
        )


if __name__ == '__main__':
    # 测试用例
    logging.basicConfig(level=logging.INFO)

    print("=== 智能编译集成模块测试 ===\n")

    builder = BuildIntegration()

    # 测试错误解析
    test_output = """
C:\\Projects\\PortMaster\\Main.cpp(42): error C2039: 'SendData': 不是 'ReliableChannel' 的成员
C:\\Projects\\PortMaster\\Transport.cpp(100): warning C4244: 转换类型不匹配
LINK : error LNK1104: cannot open file 'PortMaster.exe'
C:\\Projects\\PortMaster\\UI.cpp(200): fatal error C1083: 无法打开包含文件
"""

    errors, warnings = builder._parse_build_output(test_output)

    print(f"解析到 {len(errors)} 个错误, {len(warnings)} 个警告\n")

    print("错误列表:")
    for error in errors:
        print(f"  {error}")

    print("\n警告列表:")
    for warning in warnings:
        print(f"  {warning}")

    # 测试修复建议
    print("\n修复建议:")
    suggestions = builder.get_fix_suggestions(errors)
    for i, suggestion in enumerate(suggestions, 1):
        print(f"\n{i}. {suggestion['error']}")
        print(f"   优先级: {suggestion['priority']}")
        print(f"   建议:")
        for sug in suggestion['suggestions']:
            print(f"     - {sug}")
