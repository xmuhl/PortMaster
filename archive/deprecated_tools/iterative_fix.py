#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
迭代修复控制器 - 管理修复→测试→验证的循环
"""

import json
import os
import sys
import time
import subprocess
from datetime import datetime
from typing import Dict, List, Optional

class IterativeFixController:
    def __init__(self):
        self.max_iterations = 10
        self.test_script = 'simple_test_runner.bat'
        self.diagnostic_script = 'error_diagnostic.py'
        self.fix_script = 'auto_fix.py'
        self.test_dir = 'test_screenshots'
        self.log_file = 'iterative_fix_log.txt'

        # 成功标准
        self.success_criteria = {
            'min_stable_runtime': 15,  # 稳定运行时间（秒）
            'max_error_count': 0,       # 错误窗口数量
            'required_files': ['build/Debug/PortMaster.exe']
        }

    def run_iterative_fix(self) -> Dict:
        """运行迭代修复流程"""
        start_time = datetime.now()

        session_result = {
            'session_id': start_time.strftime("%Y%m%d-%H%M%S"),
            'start_time': start_time.isoformat(),
            'iterations': [],
            'success': False,
            'total_time': 0,
            'final_error': None,
            'summary': {}
        }

        print(f"=== 开始迭代修复流程 ===")
        print(f"会话ID: {session_result['session_id']}")
        print(f"最大迭代次数: {self.max_iterations}")
        print(f"开始时间: {start_time.strftime('%Y-%m-%d %H:%M:%S')}")

        try:
            # 预检查
            pre_check_result = self._pre_check()
            if not pre_check_result['success']:
                session_result['final_error'] = '预检查失败'
                session_result['summary'] = pre_check_result
                return session_result

            for iteration in range(1, self.max_iterations + 1):
                print(f"\n--- 第 {iteration} 次迭代 ---")

                iteration_result = {
                    'iteration': iteration,
                    'timestamp': datetime.now().isoformat(),
                    'test_result': None,
                    'diagnostic_result': None,
                    'fix_result': None,
                    'success': False
                }

                # 步骤1: 运行测试
                print("步骤1: 运行测试...")
                test_result = self._run_test()
                iteration_result['test_result'] = test_result

                # 步骤2: 诊断问题
                print("步骤2: 诊断问题...")
                diagnostic_result = self._run_diagnostic()
                iteration_result['diagnostic_result'] = diagnostic_result

                # 步骤3: 检查是否成功
                if self._check_success(test_result, diagnostic_result):
                    print("✅ 测试通过！")
                    iteration_result['success'] = True
                    session_result['iterations'].append(iteration_result)
                    session_result['success'] = True
                    break
                else:
                    print("❌ 测试未通过，需要修复...")

                # 步骤4: 应用修复
                print("步骤4: 应用修复...")
                fix_result = self._run_fix()
                iteration_result['fix_result'] = fix_result

                session_result['iterations'].append(iteration_result)

                # 步骤5: 检查修复是否成功
                if fix_result.get('success', False):
                    print("✅ 修复应用成功")
                else:
                    print("❌ 修复应用失败")

                # 短暂等待
                time.sleep(2)

            else:
                print(f"\n❌ 达到最大迭代次数 ({self.max_iterations})，修复失败")
                session_result['final_error'] = '达到最大迭代次数'

        except Exception as e:
            print(f"\n💥 迭代修复过程发生异常: {e}")
            session_result['final_error'] = f'异常: {str(e)}'

        finally:
            end_time = datetime.now()
            session_result['end_time'] = end_time.isoformat()
            session_result['total_time'] = (end_time - start_time).total_seconds()

            # 生成总结
            session_result['summary'] = self._generate_summary(session_result)

            # 保存会话结果
            self._save_session_result(session_result)

            # 显示最终结果
            self._display_final_result(session_result)

        return session_result

    def _pre_check(self) -> Dict:
        """预检查"""
        print("执行预检查...")

        checks = {
            'required_files_exist': self._check_required_files(),
            'python_available': self._check_python(),
            'scripts_executable': self._check_scripts(),
            'build_system_ready': self._check_build_system()
        }

        all_passed = all(checks.values())

        if all_passed:
            print("✅ 预检查通过")
            return {'success': True, 'checks': checks}
        else:
            print("❌ 预检查失败")
            for check_name, result in checks.items():
                status = "✅" if result else "❌"
                print(f"  {status} {check_name}")

            return {'success': False, 'checks': checks}

    def _check_required_files(self) -> bool:
        """检查必需文件是否存在"""
        required_files = [
            'simple_test_runner.bat',
            'error_diagnostic.py',
            'auto_fix.py',
            'src/PortMasterDlg.cpp',
            'src/PortMasterDlg.h',
            'resources/Resource.h'
        ]

        return all(os.path.exists(f) for f in required_files)

    def _check_python(self) -> bool:
        """检查Python是否可用"""
        try:
            subprocess.run([sys.executable, '--version'], capture_output=True, check=True)
            return True
        except:
            return False

    def _check_scripts(self) -> bool:
        """检查脚本是否可执行"""
        try:
            # 测试Python脚本的语法
            for script in [self.diagnostic_script, self.fix_script]:
                subprocess.run([sys.executable, '-m', 'py_compile', script],
                             capture_output=True, check=True)
            return True
        except:
            return False

    def _check_build_system(self) -> bool:
        """检查构建系统是否就绪"""
        return os.path.exists('autobuild_x86_debug.bat')

    def _run_test(self) -> Dict:
        """运行测试"""
        try:
            result = subprocess.run(
                [self.test_script],
                capture_output=True,
                text=True,
                timeout=120,  # 2分钟超时
                cwd=os.getcwd()
            )

            # 查找最新的测试结果文件
            test_result_file = self._find_latest_file('test_result_*.json', self.test_dir)

            if test_result_file:
                with open(test_result_file, 'r', encoding='utf-8') as f:
                    test_data = json.load(f)

                return {
                    'success': True,
                    'test_data': test_data,
                    'stdout': result.stdout,
                    'stderr': result.stderr,
                    'returncode': result.returncode
                }
            else:
                return {
                    'success': False,
                    'error': '未找到测试结果文件',
                    'stdout': result.stdout,
                    'stderr': result.stderr,
                    'returncode': result.returncode
                }

        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': '测试超时'
            }
        except Exception as e:
            return {
                'success': False,
                'error': f'运行测试失败: {str(e)}'
            }

    def _run_diagnostic(self) -> Dict:
        """运行诊断"""
        try:
            # 查找最新的测试结果文件
            test_result_file = self._find_latest_file('test_result_*.json', self.test_dir)

            if not test_result_file:
                return {
                    'success': False,
                    'error': '未找到测试结果文件用于诊断'
                }

            result = subprocess.run(
                [sys.executable, self.diagnostic_script, test_result_file],
                capture_output=True,
                text=True,
                timeout=60
            )

            # 查找最新的诊断结果文件
            diagnostic_result_file = self._find_latest_file('diagnostic_result_*.json', self.test_dir)

            if diagnostic_result_file:
                with open(diagnostic_result_file, 'r', encoding='utf-8') as f:
                    diagnostic_data = json.load(f)

                return {
                    'success': True,
                    'diagnostic_data': diagnostic_data,
                    'stdout': result.stdout,
                    'stderr': result.stderr,
                    'returncode': result.returncode
                }
            else:
                return {
                    'success': False,
                    'error': '未找到诊断结果文件',
                    'stdout': result.stdout,
                    'stderr': result.stderr
                }

        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': '诊断超时'
            }
        except Exception as e:
            return {
                'success': False,
                'error': f'运行诊断失败: {str(e)}'
            }

    def _run_fix(self) -> Dict:
        """运行修复"""
        try:
            # 查找最新的诊断结果文件
            diagnostic_result_file = self._find_latest_file('diagnostic_result_*.json', self.test_dir)

            if not diagnostic_result_file:
                return {
                    'success': False,
                    'error': '未找到诊断结果文件用于修复'
                }

            result = subprocess.run(
                [sys.executable, self.fix_script, diagnostic_result_file],
                capture_output=True,
                text=True,
                timeout=180  # 3分钟超时
            )

            # 查找最新的修复结果文件
            fix_result_file = self._find_latest_file('fix_result_*.json', self.test_dir)

            if fix_result_file:
                with open(fix_result_file, 'r', encoding='utf-8') as f:
                    fix_data = json.load(f)

                return {
                    'success': True,
                    'fix_data': fix_data,
                    'stdout': result.stdout,
                    'stderr': result.stderr,
                    'returncode': result.returncode
                }
            else:
                return {
                    'success': False,
                    'error': '未找到修复结果文件',
                    'stdout': result.stdout,
                    'stderr': result.stderr
                }

        except subprocess.TimeoutExpired:
            return {
                'success': False,
                'error': '修复超时'
            }
        except Exception as e:
            return {
                'success': False,
                'error': f'运行修复失败: {str(e)}'
            }

    def _check_success(self, test_result: Dict, diagnostic_result: Dict) -> bool:
        """检查是否成功"""
        if not test_result.get('success'):
            return False

        test_data = test_result.get('test_data', {})
        status = test_data.get('status', '')
        error_type = test_data.get('error_type', '')

        # 成功标准：
        # 1. 程序状态为RUNNING
        # 2. 没有错误窗口
        # 3. 测试标记为成功
        return (
            status == 'RUNNING' and
            error_type == 'NONE' and
            test_data.get('success', False)
        )

    def _find_latest_file(self, pattern: str, directory: str) -> Optional[str]:
        """查找最新的文件"""
        if not os.path.exists(directory):
            return None

        files = [f for f in os.listdir(directory) if f.startswith(pattern.split('*')[0])]
        if not files:
            return None

        files.sort(reverse=True)
        return os.path.join(directory, files[0])

    def _generate_summary(self, session_result: Dict) -> Dict:
        """生成会话总结"""
        iterations = session_result.get('iterations', [])

        summary = {
            'total_iterations': len(iterations),
            'successful_iterations': len([i for i in iterations if i.get('success', False)]),
            'final_status': iterations[-1]['test_result']['test_data'].get('status', 'UNKNOWN') if iterations else 'NO_DATA',
            'final_error_type': iterations[-1]['test_result']['test_data'].get('error_type', 'NONE') if iterations else 'NO_DATA',
            'fixes_applied': [],
            'backup_created': False,
            'compilation_attempts': 0
        }

        # 收集所有应用的修复
        for iteration in iterations:
            fix_result = iteration.get('fix_result')
            if fix_result and fix_result.get('success'):
                fix_data = fix_result.get('fix_data', {})
                summary['fixes_applied'].extend(fix_data.get('fixes_applied', []))
                if fix_data.get('backup_created'):
                    summary['backup_created'] = True
                if fix_data.get('compilation_success'):
                    summary['compilation_attempts'] += 1

        return summary

    def _save_session_result(self, session_result: Dict):
        """保存会话结果"""
        try:
            timestamp = session_result.get('session_id', datetime.now().strftime("%Y%m%d-%H%M%S"))
            result_file = f"test_screenshots/iterative_fix_session_{timestamp}.json"

            with open(result_file, 'w', encoding='utf-8') as f:
                json.dump(session_result, f, indent=2, ensure_ascii=False)

            print(f"会话结果已保存: {result_file}")

            # 同时保存到日志文件
            with open(self.log_file, 'a', encoding='utf-8') as f:
                f.write(f"\n=== 会话 {session_result['session_id']} ===\n")
                f.write(f"开始时间: {session_result['start_time']}\n")
                f.write(f"结束时间: {session_result['end_time']}\n")
                f.write(f"总时间: {session_result['total_time']:.2f} 秒\n")
                f.write(f"迭代次数: {session_result['summary']['total_iterations']}\n")
                f.write(f"成功状态: {session_result['success']}\n")
                if session_result.get('final_error'):
                    f.write(f"最终错误: {session_result['final_error']}\n")
                f.write("=" * 50 + "\n")

        except Exception as e:
            print(f"保存会话结果失败: {e}")

    def _display_final_result(self, session_result: Dict):
        """显示最终结果"""
        print(f"\n{'='*60}")
        print(f"{'迭代修复流程完成':^60}")
        print(f"{'='*60}")

        print(f"会话ID: {session_result['session_id']}")
        print(f"总时间: {session_result['total_time']:.2f} 秒")
        print(f"迭代次数: {session_result['summary']['total_iterations']}")
        print(f"成功迭代: {session_result['summary']['successful_iterations']}")

        if session_result['success']:
            print(f"✅ 修复成功！")
            print(f"最终状态: {session_result['summary']['final_status']}")
        else:
            print(f"❌ 修复失败")
            if session_result.get('final_error'):
                print(f"最终错误: {session_result['final_error']}")

        if session_result['summary']['fixes_applied']:
            print(f"\n应用的修复:")
            for fix in set(session_result['summary']['fixes_applied']):  # 去重
                print(f"  * {fix}")

        if session_result['summary']['backup_created']:
            print(f"\n备份已创建到: code_backups/")

        print(f"\n详细结果文件: test_screenshots/iterative_fix_session_{session_result['session_id']}.json")
        print(f"{'='*60}")

def main():
    controller = IterativeFixController()
    result = controller.run_iterative_fix()
    return 0 if result['success'] else 1

if __name__ == '__main__':
    sys.exit(main())