#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
修复策略模板库
定义各种常见错误的自动修复策略
"""

from typing import Dict, List, Optional
import re


class FixStrategy:
    """修复策略基类"""

    def __init__(self, name: str, pattern: str, priority: int = 5):
        """
        初始化修复策略

        Args:
            name: 策略名称
            pattern: 错误匹配模式（正则表达式）
            priority: 优先级（1-10，10最高）
        """
        self.name = name
        self.pattern = re.compile(pattern, re.IGNORECASE | re.MULTILINE)
        self.priority = priority

    def matches(self, error_text: str) -> bool:
        """检查是否匹配此错误模式"""
        return bool(self.pattern.search(error_text))

    def get_fix_actions(self, error_text: str, context: Dict) -> List[Dict]:
        """
        获取修复操作列表

        Args:
            error_text: 错误文本
            context: 上下文信息（文件名、行号等）

        Returns:
            修复操作列表
        """
        raise NotImplementedError("子类必须实现get_fix_actions方法")


class CompilationErrorStrategy(FixStrategy):
    """编译错误修复策略"""

    def __init__(self):
        super().__init__(
            name="Compilation Error",
            pattern=r"(error\s+C\d+|error\s+LNK\d+|fatal error)",
            priority=9
        )

    def get_fix_actions(self, error_text: str, context: Dict) -> List[Dict]:
        actions = []

        # LNK1104: 文件被占用
        if 'LNK1104' in error_text or 'cannot open file' in error_text:
            actions.append({
                'type': 'kill_process',
                'target': 'PortMaster.exe',
                'description': '关闭占用文件的进程'
            })
            actions.append({
                'type': 'rebuild',
                'clean': True,
                'description': '清理后重新编译'
            })

        # C2039: 成员不存在
        elif 'C2039' in error_text:
            match = re.search(r'"(\w+)".*?不是.*?"(\w+)".*?的成员', error_text)
            if match:
                member_name, class_name = match.groups()
                actions.append({
                    'type': 'code_fix',
                    'file': context.get('file', ''),
                    'line': context.get('line', 0),
                    'fix_type': 'missing_member',
                    'class_name': class_name,
                    'member_name': member_name,
                    'description': f'修复类{class_name}中缺失的成员{member_name}'
                })

        # C2664: 参数类型不匹配
        elif 'C2664' in error_text:
            actions.append({
                'type': 'code_fix',
                'file': context.get('file', ''),
                'line': context.get('line', 0),
                'fix_type': 'type_mismatch',
                'description': '修复参数类型不匹配'
            })

        # C2065: 未声明的标识符
        elif 'C2065' in error_text:
            match = re.search(r'"(\w+)".*?未声明', error_text)
            if match:
                identifier = match.group(1)
                actions.append({
                    'type': 'code_fix',
                    'file': context.get('file', ''),
                    'line': context.get('line', 0),
                    'fix_type': 'undeclared_identifier',
                    'identifier': identifier,
                    'description': f'添加{identifier}的声明或包含相应头文件'
                })

        # C4819: 编码问题
        elif 'C4819' in error_text:
            actions.append({
                'type': 'encoding_fix',
                'file': context.get('file', ''),
                'target_encoding': 'UTF-8 with BOM',
                'description': '修复文件编码为UTF-8 with BOM'
            })

        return actions


class RuntimeErrorStrategy(FixStrategy):
    """运行时错误修复策略"""

    def __init__(self):
        super().__init__(
            name="Runtime Error",
            pattern=r"(access violation|null pointer|assertion failed|unhandled exception)",
            priority=10
        )

    def get_fix_actions(self, error_text: str, context: Dict) -> List[Dict]:
        actions = []

        # 空指针访问
        if 'null pointer' in error_text.lower() or 'access violation' in error_text.lower():
            actions.append({
                'type': 'code_analysis',
                'analysis_type': 'null_pointer_check',
                'file': context.get('file', ''),
                'line': context.get('line', 0),
                'description': '分析空指针访问并添加检查'
            })
            actions.append({
                'type': 'code_fix',
                'fix_type': 'add_null_check',
                'file': context.get('file', ''),
                'line': context.get('line', 0),
                'description': '添加空指针检查'
            })

        # 断言失败
        elif 'assertion failed' in error_text.lower():
            actions.append({
                'type': 'code_analysis',
                'analysis_type': 'assertion_failure',
                'description': '分析断言失败原因'
            })

        # 未处理异常
        elif 'unhandled exception' in error_text.lower():
            actions.append({
                'type': 'code_fix',
                'fix_type': 'add_exception_handler',
                'file': context.get('file', ''),
                'description': '添加异常处理'
            })

        return actions


class MemoryLeakStrategy(FixStrategy):
    """内存泄漏修复策略"""

    def __init__(self):
        super().__init__(
            name="Memory Leak",
            pattern=r"(memory leak|未释放|delete|资源泄漏)",
            priority=7
        )

    def get_fix_actions(self, error_text: str, context: Dict) -> List[Dict]:
        actions = []

        actions.append({
            'type': 'code_analysis',
            'analysis_type': 'memory_leak_detection',
            'file': context.get('file', ''),
            'description': '检测内存泄漏点'
        })

        actions.append({
            'type': 'code_fix',
            'fix_type': 'add_cleanup',
            'file': context.get('file', ''),
            'description': '添加资源清理代码'
        })

        return actions


class UIStateErrorStrategy(FixStrategy):
    """UI状态错误修复策略"""

    def __init__(self):
        super().__init__(
            name="UI State Error",
            pattern=r"(按钮.*禁用|状态.*不正确|界面.*异常|按钮.*灰色)",
            priority=8
        )

    def get_fix_actions(self, error_text: str, context: Dict) -> List[Dict]:
        actions = []

        # 按钮禁用问题
        if '按钮' in error_text and ('禁用' in error_text or '灰色' in error_text):
            actions.append({
                'type': 'code_analysis',
                'analysis_type': 'button_state_logic',
                'description': '分析按钮状态管理逻辑'
            })
            actions.append({
                'type': 'code_fix',
                'fix_type': 'fix_button_state',
                'file': context.get('file', ''),
                'description': '修复按钮状态管理逻辑'
            })

        # 状态同步问题
        if '状态' in error_text and '同步' in error_text:
            actions.append({
                'type': 'code_fix',
                'fix_type': 'add_state_sync',
                'file': context.get('file', ''),
                'description': '添加状态同步代码'
            })

        return actions


class ThreadSafetyStrategy(FixStrategy):
    """线程安全问题修复策略"""

    def __init__(self):
        super().__init__(
            name="Thread Safety",
            pattern=r"(线程|多线程|race condition|deadlock|互斥)",
            priority=9
        )

    def get_fix_actions(self, error_text: str, context: Dict) -> List[Dict]:
        actions = []

        # 竞态条件
        if 'race condition' in error_text.lower():
            actions.append({
                'type': 'code_fix',
                'fix_type': 'add_mutex',
                'file': context.get('file', ''),
                'description': '添加互斥锁保护'
            })

        # 死锁
        elif 'deadlock' in error_text.lower():
            actions.append({
                'type': 'code_analysis',
                'analysis_type': 'deadlock_detection',
                'description': '分析死锁原因'
            })

        return actions


class FixStrategyLibrary:
    """修复策略库"""

    def __init__(self):
        """初始化策略库"""
        self.strategies = [
            CompilationErrorStrategy(),
            RuntimeErrorStrategy(),
            MemoryLeakStrategy(),
            UIStateErrorStrategy(),
            ThreadSafetyStrategy(),
        ]

        # 按优先级排序
        self.strategies.sort(key=lambda s: s.priority, reverse=True)

    def find_strategies(self, error_text: str) -> List[FixStrategy]:
        """
        查找匹配的修复策略

        Args:
            error_text: 错误文本

        Returns:
            匹配的策略列表（按优先级排序）
        """
        matching_strategies = []

        for strategy in self.strategies:
            if strategy.matches(error_text):
                matching_strategies.append(strategy)

        return matching_strategies

    def get_fix_actions(self, error_text: str, context: Dict = None) -> List[Dict]:
        """
        获取所有匹配策略的修复操作

        Args:
            error_text: 错误文本
            context: 上下文信息

        Returns:
            修复操作列表
        """
        if context is None:
            context = {}

        all_actions = []
        strategies = self.find_strategies(error_text)

        for strategy in strategies:
            actions = strategy.get_fix_actions(error_text, context)
            all_actions.extend(actions)

        # 去重
        seen = set()
        unique_actions = []
        for action in all_actions:
            key = (action['type'], action.get('file', ''), action.get('description', ''))
            if key not in seen:
                seen.add(key)
                unique_actions.append(action)

        return unique_actions

    def add_strategy(self, strategy: FixStrategy):
        """添加新的修复策略"""
        self.strategies.append(strategy)
        self.strategies.sort(key=lambda s: s.priority, reverse=True)


# 全局策略库实例
fix_library = FixStrategyLibrary()


def get_fix_actions_for_error(error_text: str, context: Dict = None) -> List[Dict]:
    """
    便捷函数：获取错误的修复操作

    Args:
        error_text: 错误文本
        context: 上下文信息

    Returns:
        修复操作列表
    """
    return fix_library.get_fix_actions(error_text, context)


if __name__ == '__main__':
    # 测试用例
    test_errors = [
        "error LNK1104: cannot open file 'PortMaster.exe'",
        "error C2039: 'SendData': 不是 'ReliableChannel' 的成员",
        "Access violation reading location 0x00000000",
        "按钮在传输完成后仍然禁用",
    ]

    print("=== 修复策略库测试 ===\n")

    for error in test_errors:
        print(f"错误: {error}")
        actions = get_fix_actions_for_error(error, {'file': 'test.cpp', 'line': 100})
        print(f"建议修复操作数量: {len(actions)}")
        for i, action in enumerate(actions, 1):
            print(f"  {i}. [{action['type']}] {action['description']}")
        print()
