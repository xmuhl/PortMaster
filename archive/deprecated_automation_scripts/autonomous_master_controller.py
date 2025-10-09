#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PortMaster全自动化AI驱动开发系统 - 主控制器
整合所有测试、分析、修复模块，实现完全自动化的开发流程
"""

import os
import sys
import json
import subprocess
import logging
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# 导入已有模块
sys.path.append(os.path.dirname(__file__))

from build_integration import BuildIntegration, CompilationError
from autotest_integration import AutoTestIntegration, TestValidator
from fix_strategies import FixStrategyLibrary
from static_analyzer import StaticAnalyzer
from coverage_analyzer import CoverageAnalyzer
from error_pattern_db import ErrorPatternDatabase, diagnose_error


class AutomationWorkflow:
    """自动化工作流定义"""

    def __init__(self, name: str, steps: List[Dict]):
        """
        初始化工作流

        Args:
            name: 工作流名称
            steps: 工作流步骤列表
        """
        self.name = name
        self.steps = steps
        self.current_step = 0
        self.results = []

    def execute_step(self, step: Dict, context: Dict) -> Dict:
        """
        执行单个步骤

        Args:
            step: 步骤定义
            context: 执行上下文

        Returns:
            步骤执行结果
        """
        step_type = step.get('type')
        step_name = step.get('name', f'Step {self.current_step}')

        logging.info(f"执行工作流步骤: {step_name} (类型: {step_type})")

        result = {
            'step': step_name,
            'type': step_type,
            'success': False,
            'message': '',
            'data': {}
        }

        try:
            if step_type == 'build':
                result = self._execute_build_step(step, context)
            elif step_type == 'test':
                result = self._execute_test_step(step, context)
            elif step_type == 'analyze':
                result = self._execute_analyze_step(step, context)
            elif step_type == 'fix':
                result = self._execute_fix_step(step, context)
            elif step_type == 'regression':
                result = self._execute_regression_step(step, context)
            else:
                result['message'] = f"未知步骤类型: {step_type}"

        except Exception as e:
            result['success'] = False
            result['message'] = f"步骤执行异常: {str(e)}"
            logging.error(f"工作流步骤失败: {step_name} - {str(e)}")

        self.results.append(result)
        self.current_step += 1

        return result

    def _execute_build_step(self, step: Dict, context: Dict) -> Dict:
        """执行编译步骤"""
        builder = context.get('builder')
        if not builder:
            return {'success': False, 'message': '未找到编译器实例'}

        clean = step.get('clean', False)
        build_result = builder.build(clean=clean)

        return {
            'step': step.get('name', 'Build'),
            'type': 'build',
            'success': build_result.get('success', False),
            'message': f"编译完成: {build_result.get('error_count', 0)} errors, {build_result.get('warning_count', 0)} warnings",
            'data': build_result
        }

    def _execute_test_step(self, step: Dict, context: Dict) -> Dict:
        """执行测试步骤"""
        autotest = context.get('autotest')
        if not autotest:
            return {'success': False, 'message': '未找到测试集成实例'}

        # 支持两种配置方式：test_type（兼容性）和 mode（新标准）
        test_mode = step.get('mode', step.get('test_type', 'all'))

        # 从步骤配置中获取参数
        config = step.get('config', {})
        report_name = config.get('report_name')

        logging.info(f"执行测试模式: {test_mode}")

        # 使用新的通用run_tests方法
        if hasattr(autotest, 'run_tests'):
            test_result = autotest.run_tests(test_mode, report_name)
        else:
            # 兼容性处理：使用具体方法
            if test_mode == 'all':
                test_result = autotest.run_all_tests(report_name)
            elif test_mode == 'unit-tests':
                test_result = autotest.run_unit_tests(report_name)
            elif test_mode == 'integration':
                test_result = autotest.run_integration_tests(report_name)
            elif test_mode == 'error-recovery':
                test_result = autotest.run_error_recovery_tests(report_name)
            elif test_mode == 'performance':
                test_result = autotest.run_performance_tests(report_name)
            elif test_mode == 'stress':
                test_result = autotest.run_stress_tests(report_name)
            else:
                return {'success': False, 'message': f'未知测试类型: {test_mode}'}

        # 提取测试摘要信息
        analysis = test_result.get('analysis', {})
        test_summary = f"总计{analysis.get('total_tests', 0)}个, 通过{analysis.get('passed_tests', 0)}个"
        if analysis.get('failed_tests', 0) > 0:
            test_summary += f", 失败{analysis.get('failed_tests', 0)}个"

        return {
            'step': step.get('name', 'Test'),
            'type': 'test',
            'success': test_result.get('success', False),
            'message': f"测试完成: {test_summary}",
            'data': test_result
        }

    def _execute_analyze_step(self, step: Dict, context: Dict) -> Dict:
        """执行分析步骤"""
        analyzer = context.get('static_analyzer')
        if not analyzer:
            return {'success': False, 'message': '未找到静态分析器实例'}

        analyze_result = analyzer.analyze_project()

        return {
            'step': step.get('name', 'Analyze'),
            'type': 'analyze',
            'success': True,
            'message': f"分析完成: 扫描了 {analyze_result.get('files_analyzed', 0)} 个文件",
            'data': analyze_result
        }

    def _execute_fix_step(self, step: Dict, context: Dict) -> Dict:
        """执行修复步骤"""
        fix_library = context.get('fix_library')
        if not fix_library:
            return {'success': False, 'message': '未找到修复策略库实例'}

        # 从上下文获取错误信息
        build_result = context.get('last_build_result', {})
        errors = build_result.get('errors', [])

        if not errors:
            return {
                'step': step.get('name', 'Fix'),
                'type': 'fix',
                'success': True,
                'message': '无需修复',
                'data': {}
            }

        # 获取修复建议
        fix_actions = []
        for error in errors[:5]:  # 限制最多5个错误
            error_text = str(error)
            actions = fix_library.get_fix_actions(error_text, context)
            fix_actions.extend(actions)

        return {
            'step': step.get('name', 'Fix'),
            'type': 'fix',
            'success': len(fix_actions) > 0,
            'message': f"生成了 {len(fix_actions)} 个修复建议",
            'data': {'fix_actions': fix_actions}
        }

    def _execute_regression_step(self, step: Dict, context: Dict) -> Dict:
        """执行回归测试步骤"""
        # 调用AutoTest的回归测试功能
        version = step.get('version', 'current')
        baseline = step.get('baseline', None)

        # 构建回归测试命令
        cmd = ['AutoTest.exe']

        if baseline:
            cmd.extend(['--regression', baseline, version])
        else:
            cmd.extend(['--auto-regression', version])

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=600
            )

            return {
                'step': step.get('name', 'Regression'),
                'type': 'regression',
                'success': result.returncode == 0,
                'message': '回归测试完成' if result.returncode == 0 else '检测到回归问题',
                'data': {'returncode': result.returncode, 'output': result.stdout}
            }
        except Exception as e:
            return {
                'step': step.get('name', 'Regression'),
                'type': 'regression',
                'success': False,
                'message': f'回归测试失败: {str(e)}',
                'data': {}
            }


class AutonomousMasterController:
    """主自动化控制器"""

    def __init__(self, project_dir: str = "."):
        """
        初始化主控制器

        Args:
            project_dir: 项目根目录
        """
        self.project_dir = Path(project_dir).resolve()
        self.report_dir = self.project_dir / "automation_reports"
        self.report_dir.mkdir(exist_ok=True)

        # 初始化所有模块
        self.builder = BuildIntegration()
        self.autotest = AutoTestIntegration()
        self.test_validator = TestValidator()
        self.static_analyzer = StaticAnalyzer()
        self.coverage_analyzer = CoverageAnalyzer()
        self.fix_library = FixStrategyLibrary()
        self.error_db = ErrorPatternDatabase()

        # 工作流历史
        self.workflow_history = []

        # 配置日志
        logging.basicConfig(
            level=logging.INFO,
            format='%(asctime)s - %(levelname)s - %(message)s'
        )

    def create_standard_workflow(self) -> AutomationWorkflow:
        """
        创建标准自动化工作流

        Returns:
            标准工作流定义
        """
        workflow = AutomationWorkflow("Standard Development Workflow", [
            {
                'name': 'Static Analysis',
                'type': 'analyze',
                'description': '静态代码分析'
            },
            {
                'name': 'Clean Build',
                'type': 'build',
                'clean': True,
                'description': '清理编译'
            },
            {
                'name': 'Fix Build Errors',
                'type': 'fix',
                'description': '修复编译错误',
                'condition': 'build_failed'
            },
            {
                'name': 'Run Unit Tests',
                'type': 'test',
                'test_type': 'all',
                'description': '运行所有测试'
            },
            {
                'name': 'Regression Test',
                'type': 'regression',
                'version': 'current',
                'description': '回归测试'
            }
        ])

        return workflow

    def execute_workflow(self, workflow: AutomationWorkflow) -> Dict:
        """
        执行自动化工作流

        Args:
            workflow: 工作流定义

        Returns:
            工作流执行结果
        """
        logging.info(f"开始执行工作流: {workflow.name}")

        context = {
            'builder': self.builder,
            'autotest': self.autotest,
            'static_analyzer': self.static_analyzer,
            'coverage_analyzer': self.coverage_analyzer,
            'fix_library': self.fix_library,
            'error_db': self.error_db,
            'test_validator': self.test_validator
        }

        workflow_result = {
            'workflow_name': workflow.name,
            'start_time': datetime.now().isoformat(),
            'steps': [],
            'overall_success': True
        }

        for step in workflow.steps:
            # 检查条件
            condition = step.get('condition')
            if condition:
                if not self._check_condition(condition, context):
                    logging.info(f"跳过步骤 {step.get('name')} (条件不满足: {condition})")
                    continue

            # 执行步骤
            step_result = workflow.execute_step(step, context)

            workflow_result['steps'].append(step_result)

            # 更新上下文
            if step.get('type') == 'build':
                context['last_build_result'] = step_result.get('data', {})

            # 如果关键步骤失败，根据策略决定是否继续
            if not step_result.get('success') and step.get('critical', False):
                workflow_result['overall_success'] = False
                logging.error(f"关键步骤失败: {step.get('name')}")
                break

        workflow_result['end_time'] = datetime.now().isoformat()

        # 保存工作流历史
        self.workflow_history.append(workflow_result)

        # 生成报告
        self._generate_workflow_report(workflow_result)

        return workflow_result

    def _check_condition(self, condition: str, context: Dict) -> bool:
        """
        检查步骤执行条件

        Args:
            condition: 条件字符串
            context: 执行上下文

        Returns:
            条件是否满足
        """
        if condition == 'build_failed':
            build_result = context.get('last_build_result', {})
            return not build_result.get('success', True)

        if condition == 'tests_failed':
            test_result = context.get('last_test_result', {})
            return not test_result.get('success', True)

        return True

    def auto_fix_and_rebuild(self, max_iterations: int = 3) -> Dict:
        """
        自动修复和重新编译（迭代）

        Args:
            max_iterations: 最大迭代次数

        Returns:
            修复结果
        """
        logging.info(f"开始自动修复流程（最大迭代: {max_iterations}）")

        result = {
            'success': False,
            'iterations': 0,
            'final_errors': [],
            'fix_history': []
        }

        for iteration in range(max_iterations):
            logging.info(f"迭代 {iteration + 1}/{max_iterations}")

            # 编译
            build_result = self.builder.build(clean=(iteration == 0))

            if build_result.get('success'):
                logging.info("编译成功！")
                result['success'] = True
                result['iterations'] = iteration + 1
                break

            # 分析错误
            errors = build_result.get('errors', [])
            if not errors:
                break

            logging.info(f"发现 {len(errors)} 个错误")

            # 获取修复建议
            fix_suggestions = self.builder.get_fix_suggestions(errors)

            if not fix_suggestions:
                logging.warning("无法生成修复建议")
                result['final_errors'] = errors
                break

            # 记录修复历史
            result['fix_history'].append({
                'iteration': iteration + 1,
                'errors': [str(e) for e in errors],
                'suggestions': fix_suggestions
            })

            # 这里应该实际应用修复建议
            # 由于需要代码修改权限，这里仅记录建议
            logging.info(f"生成了 {len(fix_suggestions)} 个修复建议（需要手动应用）")

        return result

    def run_complete_test_cycle(self) -> Dict:
        """
        运行完整测试周期

        Returns:
            测试周期结果
        """
        logging.info("开始完整测试周期")

        cycle_result = {
            'start_time': datetime.now().isoformat(),
            'phases': []
        }

        # 阶段1: 单元测试
        logging.info("阶段1: 运行单元测试")
        unit_result = self.autotest.run_all_tests()
        cycle_result['phases'].append({
            'name': 'Unit Tests',
            'result': unit_result
        })

        # 阶段2: 性能测试
        logging.info("阶段2: 运行性能测试")
        perf_result = self.autotest.run_performance_tests()
        cycle_result['phases'].append({
            'name': 'Performance Tests',
            'result': perf_result
        })

        # 阶段3: 压力测试
        logging.info("阶段3: 运行压力测试")
        stress_result = self.autotest.run_stress_tests()
        cycle_result['phases'].append({
            'name': 'Stress Tests',
            'result': stress_result
        })

        # 综合评估
        all_passed = all(
            phase['result'].get('success', False)
            for phase in cycle_result['phases']
        )

        cycle_result['overall_success'] = all_passed
        cycle_result['end_time'] = datetime.now().isoformat()

        # 生成报告
        report_path = self._generate_test_cycle_report(cycle_result)
        cycle_result['report_path'] = str(report_path)

        return cycle_result

    def _generate_workflow_report(self, workflow_result: Dict):
        """生成工作流执行报告"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        report_path = self.report_dir / f"workflow_report_{timestamp}.md"

        with open(report_path, 'w', encoding='utf-8') as f:
            f.write(f"# 自动化工作流执行报告\n\n")
            f.write(f"**工作流**: {workflow_result['workflow_name']}\n")
            f.write(f"**开始时间**: {workflow_result['start_time']}\n")
            f.write(f"**结束时间**: {workflow_result['end_time']}\n")
            f.write(f"**整体状态**: {'✅ 成功' if workflow_result['overall_success'] else '❌ 失败'}\n\n")

            f.write("## 执行步骤\n\n")

            for i, step in enumerate(workflow_result['steps'], 1):
                status = '✅' if step['success'] else '❌'
                f.write(f"### {i}. {step['step']} {status}\n\n")
                f.write(f"**类型**: {step['type']}\n")
                f.write(f"**结果**: {step['message']}\n\n")

        logging.info(f"工作流报告已保存: {report_path}")

    def _generate_test_cycle_report(self, cycle_result: Dict) -> Path:
        """生成测试周期报告"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        report_path = self.report_dir / f"test_cycle_report_{timestamp}.md"

        with open(report_path, 'w', encoding='utf-8') as f:
            f.write(f"# 完整测试周期报告\n\n")
            f.write(f"**开始时间**: {cycle_result['start_time']}\n")
            f.write(f"**结束时间**: {cycle_result['end_time']}\n")
            f.write(f"**整体状态**: {'✅ 通过' if cycle_result['overall_success'] else '❌ 失败'}\n\n")

            f.write("## 测试阶段\n\n")

            for phase in cycle_result['phases']:
                phase_name = phase['name']
                phase_result = phase['result']

                status = '✅' if phase_result.get('success', False) else '❌'
                f.write(f"### {phase_name} {status}\n\n")
                f.write(f"**摘要**: {phase_result.get('test_summary', 'N/A')}\n\n")

        logging.info(f"测试周期报告已保存: {report_path}")
        return report_path


if __name__ == '__main__':
    print("=== PortMaster 主自动化控制器测试 ===\n")

    # 创建控制器实例
    controller = AutonomousMasterController()

    # 测试1: 创建并执行标准工作流
    print("测试1: 执行标准开发工作流")
    workflow = controller.create_standard_workflow()

    # 显示工作流步骤
    print(f"\n工作流: {workflow.name}")
    print("步骤:")
    for i, step in enumerate(workflow.steps, 1):
        print(f"  {i}. {step['name']} ({step['type']})")

    print("\n工作流定义完成。")
    print("使用 controller.execute_workflow(workflow) 执行工作流。")

    # 测试2: 自动修复功能
    print("\n测试2: 自动修复和重新编译")
    print("功能：自动检测编译错误并提供修复建议")

    # 测试3: 完整测试周期
    print("\n测试3: 完整测试周期")
    print("功能：运行单元测试→性能测试→压力测试")

    print("\n主自动化控制器已就绪。")
