#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
错误诊断器 - 分析测试结果并识别问题模式
"""

import json
import os
import re
import sys
from datetime import datetime
from typing import Dict, List, Optional, Tuple

class ErrorDiagnostic:
    def __init__(self):
        self.error_patterns = {
            'ASSERTION_FAILED': {
                'description': 'MFC调试断言失败',
                'common_causes': [
                    '控件ID不匹配',
                    'DDX绑定错误',
                    '初始化顺序问题',
                    '资源文件损坏',
                    '控件句柄无效'
                ],
                'fix_strategies': [
                    'check_resource_consistency',
                    'fix_initialization_order',
                    'verify_ddx_bindings',
                    'add_null_checks'
                ]
            },
            'FAILED_TO_START': {
                'description': '程序启动失败',
                'common_causes': [
                    '缺少依赖库',
                    '可执行文件损坏',
                    '权限问题',
                    '配置文件错误'
                ],
                'fix_strategies': [
                    'rebuild_executable',
                    'check_dependencies',
                    'verify_permissions'
                ]
            },
            'PROGRAM_CRASH': {
                'description': '程序运行时崩溃',
                'common_causes': [
                    '内存访问错误',
                    '空指针解引用',
                    '栈溢出',
                    '异常未处理'
                ],
                'fix_strategies': [
                    'add_exception_handling',
                    'check_null_pointers',
                    'verify_memory_management'
                ]
            },
            'DEBUG_WINDOW': {
                'description': '调试窗口出现',
                'common_causes': [
                    '断言触发',
                    '运行时检查失败',
                    '调试代码未移除'
                ],
                'fix_strategies': [
                    'resolve_assertion',
                    'remove_debug_code',
                    'fix_runtime_checks'
                ]
            },
            'ERROR_WINDOW': {
                'description': '错误对话框出现',
                'common_causes': [
                    '异常弹窗',
                    '用户输入错误',
                    '系统错误'
                ],
                'fix_strategies': [
                    'add_error_handling',
                    'improve_user_input_validation',
                    'handle_system_errors'
                ]
            }
        }

    def analyze_test_result(self, result_file: str) -> Dict:
        """分析测试结果文件"""
        try:
            with open(result_file, 'r', encoding='utf-8') as f:
                data = json.load(f)

            diagnosis = {
                'timestamp': data.get('timestamp', ''),
                'status': data.get('status', 'UNKNOWN'),
                'error_type': data.get('error_type', 'NONE'),
                'success': data.get('success', False),
                'diagnosis': self._diagnose_error(data.get('error_type', 'NONE')),
                'recommendations': [],
                'fix_priority': 'HIGH'
            }

            # 根据错误类型添加具体建议
            diagnosis['recommendations'] = self._get_recommendations(diagnosis)
            diagnosis['fix_priority'] = self._determine_priority(diagnosis)

            return diagnosis

        except Exception as e:
            return {
                'error': f'分析测试结果失败: {str(e)}',
                'status': 'ANALYSIS_ERROR'
            }

    def _diagnose_error(self, error_type: str) -> Dict:
        """诊断具体错误类型"""
        if error_type not in self.error_patterns:
            return {
                'type': 'UNKNOWN',
                'description': '未知错误类型',
                'possible_causes': ['需要进一步调查'],
                'confidence': 'LOW'
            }

        pattern = self.error_patterns[error_type]
        return {
            'type': error_type,
            'description': pattern['description'],
            'possible_causes': pattern['common_causes'],
            'confidence': self._calculate_confidence(error_type),
            'fix_strategies': pattern['fix_strategies']
        }

    def _calculate_confidence(self, error_type: str) -> str:
        """计算诊断置信度"""
        confidence_map = {
            'ASSERTION_WINDOW': 'HIGH',
            'FAILED_TO_START': 'HIGH',
            'PROGRAM_CRASH': 'MEDIUM',
            'DEBUG_WINDOW': 'MEDIUM',
            'ERROR_WINDOW': 'LOW'
        }
        return confidence_map.get(error_type, 'LOW')

    def _get_recommendations(self, diagnosis: Dict) -> List[str]:
        """获取修复建议"""
        recommendations = []
        error_type = diagnosis.get('error_type', 'NONE')

        if error_type == 'ASSERTION_WINDOW':
            recommendations.extend([
                '检查Resource.h和PortMaster.rc中的控件ID一致性',
                '验证DoDataExchange中的控件绑定',
                '确保管理器在控件创建后初始化',
                '添加控件有效性检查'
            ])
        elif error_type == 'FAILED_TO_START':
            recommendations.extend([
                '重新编译项目确保可执行文件完整',
                '检查所有依赖库是否存在',
                '验证程序运行权限',
                '检查配置文件完整性'
            ])
        elif error_type == 'PROGRAM_CRASH':
            recommendations.extend([
                '添加异常处理机制',
                '检查空指针访问',
                '验证内存管理',
                '增加调试日志输出'
            ])
        elif error_type == 'DEBUG_WINDOW':
            recommendations.extend([
                '定位并修复触发的断言',
                '检查调试代码是否需要移除',
                '验证运行时检查逻辑',
                '确保发布版本配置正确'
            ])

        return recommendations

    def _determine_priority(self, diagnosis: Dict) -> str:
        """确定修复优先级"""
        error_type = diagnosis.get('error_type', 'NONE')

        high_priority = ['ASSERTION_WINDOW', 'FAILED_TO_START']
        medium_priority = ['PROGRAM_CRASH', 'DEBUG_WINDOW']
        low_priority = ['ERROR_WINDOW']

        if error_type in high_priority:
            return 'HIGH'
        elif error_type in medium_priority:
            return 'MEDIUM'
        else:
            return 'LOW'

    def find_latest_test_result(self, test_dir: str = 'test_screenshots') -> Optional[str]:
        """查找最新的测试结果文件"""
        if not os.path.exists(test_dir):
            return None

        json_files = [f for f in os.listdir(test_dir) if f.startswith('test_result_') and f.endswith('.json')]
        if not json_files:
            return None

        # 按时间戳排序，获取最新的
        json_files.sort(reverse=True)
        return os.path.join(test_dir, json_files[0])

    def generate_diagnostic_report(self, diagnosis: Dict) -> str:
        """生成诊断报告"""
        report = f"""
=== 错误诊断报告 ===
时间戳: {diagnosis.get('timestamp', 'N/A')}
程序状态: {diagnosis.get('status', 'N/A')}
错误类型: {diagnosis.get('error_type', 'N/A')}
测试成功: {'是' if diagnosis.get('success', False) else '否'}

诊断结果:
- 错误类型: {diagnosis.get('diagnosis', {}).get('description', 'N/A')}
- 置信度: {diagnosis.get('diagnosis', {}).get('confidence', 'N/A')}
- 可能原因:
"""

        for cause in diagnosis.get('diagnosis', {}).get('possible_causes', []):
            report += f"  * {cause}\n"

        report += "\n修复建议:\n"
        for rec in diagnosis.get('recommendations', []):
            report += f"  * {rec}\n"

        report += f"\n修复优先级: {diagnosis.get('fix_priority', 'N/A')}\n"
        report += f"\n建议修复策略: {', '.join(diagnosis.get('diagnosis', {}).get('fix_strategies', []))}\n"

        return report

def main():
    if len(sys.argv) > 1:
        result_file = sys.argv[1]
    else:
        diagnostic = ErrorDiagnostic()
        result_file = diagnostic.find_latest_test_result()
        if not result_file:
            print("错误: 未找到测试结果文件")
            return 1

    diagnostic = ErrorDiagnostic()
    diagnosis = diagnostic.analyze_test_result(result_file)

    print("=== 错误诊断结果 ===")
    print(f"状态: {diagnosis.get('status', 'N/A')}")
    print(f"错误类型: {diagnosis.get('error_type', 'N/A')}")
    print(f"优先级: {diagnosis.get('fix_priority', 'N/A')}")

    # 保存诊断报告
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    report_file = f"test_screenshots/diagnostic_report_{timestamp}.txt"

    try:
        with open(report_file, 'w', encoding='utf-8') as f:
            f.write(diagnostic.generate_diagnostic_report(diagnosis))
        print(f"诊断报告已保存: {report_file}")

        # 同时保存JSON格式的诊断结果
        json_file = f"test_screenshots/diagnostic_result_{timestamp}.json"
        with open(json_file, 'w', encoding='utf-8') as f:
            json.dump(diagnosis, f, indent=2, ensure_ascii=False)
        print(f"诊断结果已保存: {json_file}")

    except Exception as e:
        print(f"保存诊断报告失败: {e}")

    return 0 if diagnosis.get('success', False) else 1

if __name__ == '__main__':
    sys.exit(main())