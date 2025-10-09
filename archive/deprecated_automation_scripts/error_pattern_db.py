#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
增强错误模式识别数据库
包含丰富的错误模式和智能修复建议
"""

import re
import json
from typing import Dict, List, Optional
from datetime import datetime


class ErrorPattern:
    """错误模式类"""

    def __init__(self, pattern_id: str, name: str, pattern: str,
                 category: str, severity: str, description: str):
        """
        初始化错误模式

        Args:
            pattern_id: 模式ID
            name: 模式名称
            pattern: 正则表达式模式
            category: 类别
            severity: 严重程度
            description: 描述
        """
        self.pattern_id = pattern_id
        self.name = name
        self.pattern = re.compile(pattern, re.IGNORECASE | re.MULTILINE)
        self.category = category
        self.severity = severity
        self.description = description
        self.fix_actions = []

    def matches(self, error_text: str) -> bool:
        """检查是否匹配"""
        return bool(self.pattern.search(error_text))

    def add_fix_action(self, action: Dict):
        """添加修复操作"""
        self.fix_actions.append(action)

    def get_fix_actions(self, error_text: str, context: Dict) -> List[Dict]:
        """获取修复操作"""
        return self.fix_actions.copy()


class ErrorPatternDatabase:
    """错误模式数据库"""

    def __init__(self):
        """初始化错误模式数据库"""
        self.patterns = []
        self._initialize_patterns()

    def _initialize_patterns(self):
        """初始化错误模式"""

        # === 编译器错误 ===

        # LNK1104: 无法打开文件
        lnk1104 = ErrorPattern(
            pattern_id='LNK1104',
            name='文件被占用或不存在',
            pattern=r'error\s+LNK1104.*cannot\s+open\s+file',
            category='linker',
            severity='high',
            description='链接器无法打开文件，通常是文件被占用或不存在'
        )
        lnk1104.add_fix_action({
            'type': 'kill_process',
            'target': '*.exe',
            'description': '关闭占用文件的进程'
        })
        lnk1104.add_fix_action({
            'type': 'clean_build',
            'description': '清理后重新编译'
        })
        self.patterns.append(lnk1104)

        # LNK2001/LNK2019: 无法解析的外部符号
        lnk2001 = ErrorPattern(
            pattern_id='LNK2001_2019',
            name='无法解析的外部符号',
            pattern=r'error\s+LNK200[19].*unresolved\s+external\s+symbol',
            category='linker',
            severity='high',
            description='链接器无法找到函数或变量的定义'
        )
        lnk2001.add_fix_action({
            'type': 'check_library',
            'description': '检查是否缺少链接库'
        })
        lnk2001.add_fix_action({
            'type': 'check_definition',
            'description': '检查函数/变量是否有定义'
        })
        lnk2001.add_fix_action({
            'type': 'check_name_mangling',
            'description': '检查C/C++名称改编问题（extern "C"）'
        })
        self.patterns.append(lnk2001)

        # C2039: 成员不存在
        c2039 = ErrorPattern(
            pattern_id='C2039',
            name='类成员不存在',
            pattern=r'error\s+C2039.*is\s+not\s+a\s+member\s+of',
            category='compiler',
            severity='high',
            description='类或命名空间中不存在该成员'
        )
        c2039.add_fix_action({
            'type': 'check_api_version',
            'description': '检查API版本是否匹配'
        })
        c2039.add_fix_action({
            'type': 'suggest_alternative_api',
            'description': '建议使用替代API'
        })
        self.patterns.append(c2039)

        # C2065: 未声明的标识符
        c2065 = ErrorPattern(
            pattern_id='C2065',
            name='未声明的标识符',
            pattern=r'error\s+C2065.*undeclared\s+identifier',
            category='compiler',
            severity='medium',
            description='使用了未声明的标识符'
        )
        c2065.add_fix_action({
            'type': 'add_include',
            'description': '添加缺失的头文件'
        })
        c2065.add_fix_action({
            'type': 'add_declaration',
            'description': '添加变量或函数声明'
        })
        self.patterns.append(c2065)

        # C1083: 无法打开包含文件
        c1083 = ErrorPattern(
            pattern_id='C1083',
            name='无法打开包含文件',
            pattern=r'fatal\s+error\s+C1083.*Cannot\s+open\s+include\s+file',
            category='compiler',
            severity='high',
            description='编译器无法找到包含的头文件'
        )
        c1083.add_fix_action({
            'type': 'check_include_path',
            'description': '检查包含目录设置'
        })
        c1083.add_fix_action({
            'type': 'check_file_exists',
            'description': '检查文件是否存在'
        })
        self.patterns.append(c1083)

        # C4819: 编码问题
        c4819 = ErrorPattern(
            pattern_id='C4819',
            name='文件编码问题',
            pattern=r'warning\s+C4819',
            category='compiler',
            severity='low',
            description='文件编码与当前代码页不匹配'
        )
        c4819.add_fix_action({
            'type': 'fix_encoding',
            'target_encoding': 'UTF-8 with BOM',
            'description': '转换文件编码为UTF-8 with BOM'
        })
        self.patterns.append(c4819)

        # C2664: 参数类型不匹配
        c2664 = ErrorPattern(
            pattern_id='C2664',
            name='参数类型不匹配',
            pattern=r'error\s+C2664.*cannot\s+convert.*argument',
            category='compiler',
            severity='medium',
            description='函数参数类型不匹配'
        )
        c2664.add_fix_action({
            'type': 'add_type_cast',
            'description': '添加类型转换'
        })
        c2664.add_fix_action({
            'type': 'fix_parameter_type',
            'description': '修正参数类型'
        })
        self.patterns.append(c2664)

        # === 运行时错误 ===

        # 访问违例
        access_violation = ErrorPattern(
            pattern_id='ACCESS_VIOLATION',
            name='访问违例',
            pattern=r'(access\s+violation|0xC0000005)',
            category='runtime',
            severity='critical',
            description='非法内存访问'
        )
        access_violation.add_fix_action({
            'type': 'add_null_check',
            'description': '添加空指针检查'
        })
        access_violation.add_fix_action({
            'type': 'check_buffer_overflow',
            'description': '检查缓冲区溢出'
        })
        access_violation.add_fix_action({
            'type': 'check_use_after_free',
            'description': '检查释放后使用'
        })
        self.patterns.append(access_violation)

        # 空指针解引用
        null_pointer = ErrorPattern(
            pattern_id='NULL_POINTER',
            name='空指针解引用',
            pattern=r'null\s+pointer|nullptr|0x00000000',
            category='runtime',
            severity='critical',
            description='尝试解引用空指针'
        )
        null_pointer.add_fix_action({
            'type': 'add_null_check',
            'description': '在使用前检查指针是否为空'
        })
        null_pointer.add_fix_action({
            'type': 'initialize_pointer',
            'description': '确保指针正确初始化'
        })
        self.patterns.append(null_pointer)

        # 断言失败
        assertion = ErrorPattern(
            pattern_id='ASSERTION_FAILED',
            name='断言失败',
            pattern=r'assertion\s+failed|ASSERT',
            category='runtime',
            severity='high',
            description='运行时断言失败'
        )
        assertion.add_fix_action({
            'type': 'analyze_assertion',
            'description': '分析断言条件为何失败'
        })
        assertion.add_fix_action({
            'type': 'fix_logic_error',
            'description': '修复导致断言失败的逻辑错误'
        })
        self.patterns.append(assertion)

        # === MFC特定错误 ===

        # 无效的窗口句柄
        invalid_hwnd = ErrorPattern(
            pattern_id='INVALID_HWND',
            name='无效的窗口句柄',
            pattern=r'invalid\s+window\s+handle|HWND',
            category='mfc',
            severity='high',
            description='使用了无效的窗口句柄'
        )
        invalid_hwnd.add_fix_action({
            'type': 'check_window_creation',
            'description': '检查窗口是否成功创建'
        })
        invalid_hwnd.add_fix_action({
            'type': 'check_window_destroyed',
            'description': '检查窗口是否已被销毁'
        })
        self.patterns.append(invalid_hwnd)

        # 资源泄漏
        resource_leak = ErrorPattern(
            pattern_id='RESOURCE_LEAK',
            name='资源泄漏',
            pattern=r'(memory\s+leak|resource\s+leak|handle\s+leak)',
            category='resource',
            severity='medium',
            description='资源未正确释放'
        )
        resource_leak.add_fix_action({
            'type': 'add_cleanup_code',
            'description': '添加资源释放代码'
        })
        resource_leak.add_fix_action({
            'type': 'use_smart_pointer',
            'description': '使用智能指针管理资源'
        })
        self.patterns.append(resource_leak)

        # === 线程同步错误 ===

        # 死锁
        deadlock = ErrorPattern(
            pattern_id='DEADLOCK',
            name='死锁',
            pattern=r'deadlock',
            category='threading',
            severity='critical',
            description='多线程死锁'
        )
        deadlock.add_fix_action({
            'type': 'analyze_lock_order',
            'description': '分析锁的获取顺序'
        })
        deadlock.add_fix_action({
            'type': 'implement_lock_timeout',
            'description': '实现锁超时机制'
        })
        self.patterns.append(deadlock)

        # 竞态条件
        race_condition = ErrorPattern(
            pattern_id='RACE_CONDITION',
            name='竞态条件',
            pattern=r'race\s+condition',
            category='threading',
            severity='high',
            description='多线程竞态条件'
        )
        race_condition.add_fix_action({
            'type': 'add_synchronization',
            'description': '添加同步机制（互斥锁、临界区）'
        })
        race_condition.add_fix_action({
            'type': 'use_atomic_operations',
            'description': '使用原子操作'
        })
        self.patterns.append(race_condition)

        # === UI状态错误 ===

        # 按钮状态错误
        button_state = ErrorPattern(
            pattern_id='BUTTON_STATE_ERROR',
            name='按钮状态错误',
            pattern=r'(按钮.*禁用|按钮.*灰色|button.*disabled)',
            category='ui',
            severity='medium',
            description='按钮状态不正确'
        )
        button_state.add_fix_action({
            'type': 'fix_button_enable_logic',
            'description': '修复按钮启用/禁用逻辑'
        })
        button_state.add_fix_action({
            'type': 'add_state_update',
            'description': '在状态变化时更新按钮状态'
        })
        self.patterns.append(button_state)

        # 控件状态同步
        control_sync = ErrorPattern(
            pattern_id='CONTROL_SYNC_ERROR',
            name='控件状态同步错误',
            pattern=r'(状态.*不同步|状态.*不一致|state.*inconsistent)',
            category='ui',
            severity='medium',
            description='UI控件状态与数据不同步'
        )
        control_sync.add_fix_action({
            'type': 'add_data_binding',
            'description': '实现数据绑定机制'
        })
        control_sync.add_fix_action({
            'type': 'add_update_event',
            'description': '在数据变化时触发UI更新'
        })
        self.patterns.append(control_sync)

    def find_patterns(self, error_text: str) -> List[ErrorPattern]:
        """
        查找匹配的错误模式

        Args:
            error_text: 错误文本

        Returns:
            匹配的模式列表（按严重程度排序）
        """
        matching_patterns = []

        for pattern in self.patterns:
            if pattern.matches(error_text):
                matching_patterns.append(pattern)

        # 按严重程度排序
        severity_order = {'critical': 0, 'high': 1, 'medium': 2, 'low': 3}
        matching_patterns.sort(key=lambda p: severity_order.get(p.severity, 99))

        return matching_patterns

    def get_fix_actions(self, error_text: str, context: Dict = None) -> List[Dict]:
        """获取修复操作"""
        if context is None:
            context = {}

        patterns = self.find_patterns(error_text)

        all_actions = []
        for pattern in patterns:
            actions = pattern.get_fix_actions(error_text, context)
            for action in actions:
                action['pattern_id'] = pattern.pattern_id
                action['pattern_name'] = pattern.name
                action['severity'] = pattern.severity
            all_actions.extend(actions)

        return all_actions

    def export_patterns(self, filepath: str):
        """导出错误模式数据库"""
        data = {
            'export_time': datetime.now().isoformat(),
            'total_patterns': len(self.patterns),
            'patterns': [
                {
                    'pattern_id': p.pattern_id,
                    'name': p.name,
                    'category': p.category,
                    'severity': p.severity,
                    'description': p.description,
                    'fix_actions_count': len(p.fix_actions)
                }
                for p in self.patterns
            ]
        }

        with open(filepath, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, ensure_ascii=False)

    def get_statistics(self) -> Dict:
        """获取数据库统计信息"""
        categories = {}
        severities = {}

        for pattern in self.patterns:
            categories[pattern.category] = categories.get(pattern.category, 0) + 1
            severities[pattern.severity] = severities.get(pattern.severity, 0) + 1

        return {
            'total_patterns': len(self.patterns),
            'by_category': categories,
            'by_severity': severities
        }


# 全局错误模式数据库实例
error_db = ErrorPatternDatabase()


def diagnose_error(error_text: str, context: Dict = None) -> Dict:
    """
    诊断错误并提供修复建议

    Args:
        error_text: 错误文本
        context: 上下文信息

    Returns:
        诊断结果字典
    """
    patterns = error_db.find_patterns(error_text)

    if not patterns:
        return {
            'matched': False,
            'message': '未识别到已知错误模式',
            'suggestions': ['请手动分析错误信息']
        }

    primary_pattern = patterns[0]
    fix_actions = error_db.get_fix_actions(error_text, context)

    return {
        'matched': True,
        'pattern_id': primary_pattern.pattern_id,
        'pattern_name': primary_pattern.name,
        'category': primary_pattern.category,
        'severity': primary_pattern.severity,
        'description': primary_pattern.description,
        'fix_actions': fix_actions,
        'total_matches': len(patterns),
        'all_patterns': [
            {
                'id': p.pattern_id,
                'name': p.name,
                'severity': p.severity
            }
            for p in patterns
        ]
    }


if __name__ == '__main__':
    # 测试用例
    print("=== 错误模式识别数据库测试 ===\n")

    # 显示统计信息
    stats = error_db.get_statistics()
    print(f"错误模式总数: {stats['total_patterns']}")
    print("\n按类别统计:")
    for category, count in stats['by_category'].items():
        print(f"  {category}: {count}")
    print("\n按严重程度统计:")
    for severity, count in stats['by_severity'].items():
        print(f"  {severity}: {count}")

    # 测试错误诊断
    test_errors = [
        "error LNK1104: cannot open file 'PortMaster.exe'",
        "error C2039: 'SendData' is not a member of 'ReliableChannel'",
        "Access violation reading location 0x00000000",
        "按钮在传输完成后仍然禁用",
        "Deadlock detected in worker thread"
    ]

    print("\n\n=== 错误诊断测试 ===\n")

    for error in test_errors:
        print(f"错误: {error}")
        diagnosis = diagnose_error(error)

        if diagnosis['matched']:
            print(f"  匹配模式: [{diagnosis['pattern_id']}] {diagnosis['pattern_name']}")
            print(f"  严重程度: {diagnosis['severity']}")
            print(f"  描述: {diagnosis['description']}")
            print(f"  修复建议:")
            for i, action in enumerate(diagnosis['fix_actions'], 1):
                print(f"    {i}. [{action['type']}] {action['description']}")
        else:
            print(f"  {diagnosis['message']}")

        print()

    # 导出模式数据库
    error_db.export_patterns('error_patterns_export.json')
    print("错误模式数据库已导出: error_patterns_export.json")
