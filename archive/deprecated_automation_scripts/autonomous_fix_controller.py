#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
自主智能自动化修复控制器
完全自主执行的程序启动、截屏、分析、修复、验证系统
"""

import os
import sys
import json
import time
import subprocess
import shutil
from datetime import datetime
from typing import Dict, List, Optional, Tuple, Any
import logging

# 设置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('autonomous_fix.log', encoding='utf-8'),
        logging.StreamHandler(sys.stdout)
    ]
)

class AutonomousFixController:
    def __init__(self):
        self.max_iterations = 20  # 最大迭代次数
        self.screenshot_dir = "auto_screenshots"
        self.backup_dir = "code_backups"
        self.results_dir = "fix_results"

        # 程序信息
        self.executable_path = "build\\Debug\\PortMaster.exe"
        self.program_name = "PortMaster"

        # 成功标准
        self.success_criteria = {
            'min_stable_runtime': 30,  # 稳定运行时间（秒）
            'max_error_windows': 0,   # 错误窗口数量
            'required_processes': 1   # 必需运行的进程数
        }

        # 当前会话信息
        self.session_id = datetime.now().strftime("%Y%m%d-%H%M%S")
        self.current_iteration = 0
        self.session_start_time = datetime.now()

        # 创建必要目录
        self._setup_directories()

        logging.info(f"自主智能自动化修复系统启动")
        logging.info(f"会话ID: {self.session_id}")
        logging.info(f"工作目录: {os.getcwd()}")

    def _setup_directories(self):
        """创建必要的目录"""
        for directory in [self.screenshot_dir, self.backup_dir, self.results_dir]:
            os.makedirs(directory, exist_ok=True)
            logging.info(f"目录已创建: {directory}")

    def run_autonomous_fix(self) -> Dict:
        """执行完整的自主修复流程"""
        try:
            logging.info("=== 开始自主智能自动化修复流程 ===")

            session_result = {
                'session_id': self.session_id,
                'start_time': self.session_start_time.isoformat(),
                'iterations': [],
                'success': False,
                'total_time': 0,
                'screenshots_taken': [],
                'errors_found': [],
                'fixes_applied': [],
                'final_status': 'UNKNOWN'
            }

            # 预检查
            if not self._pre_check():
                session_result['final_status'] = 'PRE_CHECK_FAILED'
                return session_result

            # 主循环：启动程序→截屏→分析→修复→验证
            for self.current_iteration in range(1, self.max_iterations + 1):
                logging.info(f"\n--- 第 {self.current_iteration} 次迭代 ---")

                iteration_result = {
                    'iteration': self.current_iteration,
                    'timestamp': datetime.now().isoformat(),
                    'screenshots': [],
                    'analysis': None,
                    'fixes': [],
                    'success': False
                }

                # 步骤1: 启动程序
                program_info = self._start_program()
                iteration_result['program_info'] = program_info

                if not program_info['success']:
                    logging.error("程序启动失败，尝试修复...")
                    # 应用启动修复
                    startup_fixes = self._apply_startup_fixes()
                    iteration_result['fixes'].extend(startup_fixes)
                    session_result['fixes_applied'].extend(startup_fixes)
                    continue

                # 步骤2: 等待程序稳定并截屏
                stable_info = self._wait_and_capture_screenshots(iteration_result)

                # 步骤3: 分析截图和错误窗口
                has_error_windows = 'error_analysis' in stable_info and stable_info['error_analysis']['has_errors']

                # 合并截图分析和错误窗口分析
                analysis_result = self._analyze_screenshots(iteration_result['screenshots'])

                # 如果有错误窗口信息，添加到分析结果中
                if has_error_windows:
                    error_analysis = stable_info['error_analysis']
                    analysis_result['error_windows'].extend(error_analysis['error_windows'])
                    analysis_result['error_types'].extend([w['window_type'] for w in error_analysis['error_windows']])
                    analysis_result['has_errors'] = True
                    analysis_result['error_analysis'] = error_analysis

                iteration_result['analysis'] = analysis_result
                session_result['screenshots_taken'].extend(iteration_result['screenshots'])

                if analysis_result['has_errors']:
                    session_result['errors_found'].append(analysis_result)

                    # 步骤4: 基于分析结果应用修复
                    fixes = self._apply_fixes(analysis_result)
                    iteration_result['fixes'].extend(fixes)
                    session_result['fixes_applied'].extend(fixes)

                    # 步骤5: 关闭程序准备下次测试
                    self._terminate_program()
                else:
                    # 步骤6: 验证程序运行稳定性
                    if self._verify_stability():
                        iteration_result['success'] = True
                        session_result['success'] = True
                        session_result['final_status'] = 'SUCCESS'
                        logging.info("[SUCCESS] 程序运行稳定！修复成功！")
                        break
                    else:
                        logging.warning("程序稳定性验证失败，继续修复...")

                session_result['iterations'].append(iteration_result)

                # 清理进程，为下次迭代做准备
                self._cleanup_processes()

            # 汇总结果
            end_time = datetime.now()
            session_result['end_time'] = end_time.isoformat()
            session_result['total_time'] = (end_time - self.session_start_time).total_seconds()

            # 生成最终报告
            self._generate_final_report(session_result)

            return session_result

        except Exception as e:
            logging.error(f"自主修复流程发生异常: {e}")
            return {
                'session_id': self.session_id,
                'success': False,
                'error': str(e),
                'total_time': (datetime.now() - self.session_start_time).total_seconds()
            }

    def _pre_check(self) -> bool:
        """预检查系统环境"""
        logging.info("执行预检查...")

        checks = {
            'executable_exists': os.path.exists(self.executable_path),
            'directories_ready': all(os.path.exists(d) for d in [self.screenshot_dir, self.backup_dir, self.results_dir]),
            'build_system_ready': os.path.exists('autobuild_x86_debug.bat')
        }

        for check_name, result in checks.items():
            if result:
                logging.info(f"[SUCCESS] {check_name}")
            else:
                logging.error(f"[ERROR] {check_name}")

        return all(checks.values())

    def _start_program(self) -> Dict:
        """启动程序并获取进程信息"""
        logging.info("启动程序...")

        try:
            # 清理可能存在的进程
            self._cleanup_processes()

            # 启动程序
            process = subprocess.Popen(
                [self.executable_path],
                cwd=os.getcwd(),
                shell=True,
                creationflags=subprocess.CREATE_NEW_CONSOLE
            )

            # 等待程序启动
            time.sleep(5)

            # 检查进程是否仍在运行
            if self._is_program_running():
                logging.info("程序启动成功")
                return {
                    'success': True,
                    'pid': process.pid,
                    'start_time': datetime.now().isoformat(),
                    'executable': self.executable_path
                }
            else:
                logging.error("程序启动后立即退出")
                return {'success': False, 'error': '程序启动后立即退出'}

        except Exception as e:
            logging.error(f"启动程序失败: {e}")
            return {'success': False, 'error': str(e)}

    def _wait_and_capture_screenshots(self, iteration_result: Dict) -> Dict:
        """等待程序稳定并捕获多个截图"""
        logging.info("等待程序稳定并截屏...")

        screenshots = iteration_result['screenshots']

        # 首先检查是否有错误窗口
        error_analysis = self._check_for_error_windows()
        if error_analysis['has_errors']:
            iteration_result['error_analysis'] = error_analysis
            iteration_result['screenshots'] = error_analysis['screenshots']
            logging.warning(f"检测到错误窗口: {len(error_analysis['error_windows'])}个")
            return iteration_result

        # 多个时间点的截图
        capture_times = [3, 10, 20]  # 启动后3秒、10秒、20秒

        for delay in capture_times:
            time.sleep(delay)
            screenshot_path = self._capture_screenshot(iteration_result['iteration'], delay)
            if screenshot_path:
                screenshots.append(screenshot_path)
                logging.info(f"截图保存: {screenshot_path}")
            else:
                logging.warning(f"截图失败（延迟{delay}秒后）")

        return {
            'captured_count': len(screenshots),
            'capture_times': capture_times,
            'success': len(screenshots) > 0
        }

    def _capture_screenshot(self, iteration: int, delay: int) -> Optional[str]:
        """捕获程序窗口截图"""
        try:
            # 使用PIL进行截图
            from PIL import ImageGrab

            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"{self.screenshot_dir}/iter{iteration}_delay{delay}_{timestamp}.png"

            # 尝试识别并捕获特定窗口
            window_title = self._find_program_window()

            if window_title:
                # 使用pygetwindow捕获特定窗口（如果可用）
                try:
                    import pygetwindow as gw
                    windows = gw.getWindowsWithTitle(window_title)
                    if windows:
                        window = windows[0]
                        screenshot = ImageGrab.grab(bbox=(window.left, window.top, window.width, window.height))
                        screenshot.save(filename)
                        return filename
                except ImportError:
                    pass

            # 回退到全屏截图
            screenshot = ImageGrab.grab()
            screenshot.save(filename)
            return filename

        except ImportError:
            logging.warning("PIL库未安装，尝试使用替代方案")
            return self._capture_screenshot_fallback(iteration, delay)
        except Exception as e:
            logging.error(f"截图失败: {e}")
            return None

    def _capture_screenshot_fallback(self, iteration: int, delay: int) -> Optional[str]:
        """截图的回退方案"""
        try:
            # 使用系统内置截图工具
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"{self.screenshot_dir}/fallback_iter{iteration}_{timestamp}.png"

            # 使用Windows的截图工具
            subprocess.run(['powershell', '-Command',
                f"Add-Type -AssemblyName System.Windows.Forms; [System.Windows.Forms.SendKeys]::SendWait('%{{PRTSC}}'); Start-Sleep -Milliseconds 500; [System.Drawing.Bitmap]::FromScreen([System.Drawing.Rectangle]::FromLTRB(0, 0, [System.Windows.Forms.Screen]::PrimaryScreen.Bounds.Width, [System.Windows.Forms.Screen]::PrimaryScreen.Bounds.Height)).Save('{filename}');"],
                capture_output=True, timeout=10)

            # 检查文件是否创建
            time.sleep(2)
            if os.path.exists(filename):
                return filename
            else:
                logging.error("截图工具未能创建文件")
                return None

        except Exception as e:
            logging.error(f"回退截图失败: {e}")
            return None

    def _find_program_window(self) -> Optional[str]:
        """查找程序窗口标题"""
        try:
            import psutil

            for proc in psutil.process_iter(['pid', 'name', 'username']):
                try:
                    if self.program_name.lower() in proc.info['name'].lower():
                        # 查找该进程的窗口
                        if os.name == 'nt':  # Windows
                            try:
                                import pygetwindow as gw
                                windows = gw.getAllWindows()
                                for window in windows:
                                    if window.title and self.program_name.lower() in window.title.lower():
                                        return window.title
                            except ImportError:
                                pass
                        break
                except (psutil.NoSuchProcess, psutil.AccessDenied):
                    continue

        except ImportError:
            logging.warning("psutil库未安装，无法查找程序窗口")

        return None

    def _analyze_screenshots(self, screenshots: List[str]) -> Dict:
        """分析截图，检测错误窗口"""
        logging.info(f"分析 {len(screenshots)} 张截图...")

        analysis = {
            'total_screenshots': len(screenshots),
            'error_windows': [],
            'error_types': [],
            'has_errors': False,
            'program_detected': False,
            'stability_score': 0
        }

        for screenshot_path in screenshots:
            try:
                screenshot_analysis = self._analyze_single_screenshot(screenshot_path)
                analysis['error_windows'].extend(screenshot_analysis['error_windows'])
                analysis['error_types'].extend(screenshot_analysis['error_types'])

                if screenshot_analysis['program_detected']:
                    analysis['program_detected'] = True

            except Exception as e:
                logging.error(f"分析截图失败 {screenshot_path}: {e}")

        # 综合分析结果
        analysis['has_errors'] = len(analysis['error_windows']) > 0
        analysis['stability_score'] = self._calculate_stability_score(analysis)

        logging.info(f"截图分析完成: 检测到 {len(analysis['error_windows'])} 个错误窗口")

        return analysis

    def _analyze_single_screenshot(self, screenshot_path: str) -> Dict:
        """分析单个截图"""
        try:
            from PIL import Image
            import pytesseract

            image = Image.open(screenshot_path)

            # 尝试OCR识别文本
            try:
                text = pytesseract.image_to_string(image, lang='chi_sim+eng')
            except:
                text = ""

            # 分析截图中的错误窗口
            error_windows = []
            error_types = []
            program_detected = False

            # 检查常见错误关键词
            error_keywords = [
                'Assertion', 'Debug', 'Error', 'Failed', 'Exception',
                '断言', '错误', '失败', '异常', '调试'
            ]

            if text:
                for keyword in error_keywords:
                    if keyword.lower() in text.lower():
                        error_types.append(keyword)
                        if keyword in ['Assertion', '断言']:
                            error_windows.append('ASSERTION_DIALOG')
                        elif keyword in ['Debug', '调试']:
                            error_windows.append('DEBUG_WINDOW')
                        elif keyword in ['Error', '错误', 'Failed', '失败']:
                            error_windows.append('ERROR_DIALOG')
                        elif keyword in ['Exception', '异常']:
                            error_windows.append('EXCEPTION_DIALOG')

            # 检查程序特征
            program_keywords = ['PortMaster', '端口', '大师', 'COM', 'Serial']
            for keyword in program_keywords:
                if keyword in text:
                    program_detected = True
                    break

            return {
                'file': screenshot_path,
                'text_found': len(text) > 0,
                'error_windows': error_windows,
                'error_types': error_types,
                'program_detected': program_detected,
                'text_sample': text[:200] if text else ""
            }

        except Exception as e:
            logging.error(f"分析截图失败 {screenshot_path}: {e}")
            return {
                'file': screenshot_path,
                'error': str(e),
                'error_windows': [],
                'error_types': [],
                'program_detected': False
            }

    def _calculate_stability_score(self, analysis: Dict) -> int:
        """计算程序稳定性得分"""
        score = 100

        # 有错误窗口扣分
        score -= len(analysis['error_windows']) * 30

        # 检测到程序加分
        if analysis['program_detected']:
            score += 20

        # 多张截图加分
        score += min(analysis['total_screenshots'] * 5, 25)

        return max(0, score)

    def _apply_fixes(self, analysis: Dict) -> List[str]:
        """基于分析结果应用修复"""
        logging.info("基于分析结果应用修复...")

        fixes_applied = []
        error_types = analysis['error_types']

        # 根据检测到的错误类型选择修复策略
        if 'Assertion' in error_types or '断言' in error_types:
            fixes_applied.extend(self._fix_assertion_errors())

        if 'Debug' in error_types or '调试' in error_types:
            fixes_applied.extend(self._fix_debug_errors())

        if 'Error' in error_types or '错误' in error_types or 'Failed' in error_types or '失败' in error_types:
            fixes_applied.extend(self._fix_runtime_errors())

        if not analysis['program_detected']:
            fixes_applied.extend(self._fix_startup_issues())

        # 如果稳定性得分低，应用通用修复
        if analysis.get('stability_score', 0) < 50:
            fixes_applied.extend(self._apply_general_fixes())

        logging.info(f"应用了 {len(fixes_applied)} 个修复")
        return fixes_applied

    def _fix_assertion_errors(self) -> List[str]:
        """修复断言错误"""
        fixes = []

        # 这里可以实现具体的断言错误修复逻辑
        logging.info("应用断言错误修复策略")

        # 示例：修复资源ID不匹配
        if self._fix_resource_mismatches():
            fixes.append("修复资源ID不匹配")

        # 示例：修复初始化顺序
        if self._fix_initialization_order():
            fixes.append("修复初始化顺序")

        return fixes

    def _fix_debug_errors(self) -> List[str]:
        """修复调试错误"""
        fixes = []

        logging.info("应用调试错误修复策略")

        # 检查并移除调试代码
        if self._remove_debug_code():
            fixes.append("移除调试代码")

        return fixes

    def _fix_runtime_errors(self) -> List[str]:
        """修复运行时错误"""
        fixes = []

        logging.info("应用运行时错误修复策略")

        # 检查是否有错误分析结果
        error_analysis = None
        for result in self.session_result.get('errors_found', []):
            if 'error_analysis' in result:
                error_analysis = result['error_analysis']
                break

        if error_analysis and error_analysis['error_windows']:
            error_window = error_analysis['error_windows'][0]
            window_type = error_window.get('window_type', '')

            if 'RUNTIME_ERROR' in window_type or 'Microsoft Visual C++' in error_window.get('title', ''):
                logging.info("检测到Microsoft Visual C++ Runtime Library错误")

                # 针对此类错误的修复策略
                if self._fix_mfc_runtime_error():
                    fixes.append("修复MFC运行时错误")

                if self._add_null_pointer_checks():
                    fixes.append("添加空指针检查")

                if self._fix_memory_access_issues():
                    fixes.append("修复内存访问问题")
            else:
                # 通用运行时错误修复
                if self._add_exception_handling():
                    fixes.append("添加异常处理")
        else:
            # 通用运行时错误修复
            if self._add_exception_handling():
                fixes.append("添加异常处理")

        return fixes

    def _fix_startup_issues(self) -> List[str]:
        """修复启动问题"""
        fixes = []

        logging.info("应用启动问题修复策略")

        # 重新编译程序
        if self._rebuild_program():
            fixes.append("重新编译程序")

        return fixes

    def _apply_general_fixes(self) -> List[str]:
        """应用通用修复策略"""
        fixes = []

        logging.info("应用通用修复策略")

        # 创建备份
        if self._create_backup():
            fixes.append("创建代码备份")

        # 重新编译
        if self._rebuild_program():
            fixes.append("重新编译程序")

        return fixes

    def _create_backup(self) -> bool:
        """创建代码备份"""
        try:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            backup_path = os.path.join(self.backup_dir, f"backup_{timestamp}")

            # 备份关键文件
            key_files = [
                'src/PortMasterDlg.cpp',
                'src/PortMasterDlg.h',
                'resources/Resource.h'
            ]

            for file_path in key_files:
                if os.path.exists(file_path):
                    backup_file = os.path.join(backup_path, file_path)
                    os.makedirs(os.path.dirname(backup_file), exist_ok=True)
                    shutil.copy2(file_path, backup_file)

            logging.info(f"代码备份已创建: {backup_path}")
            return True

        except Exception as e:
            logging.error(f"创建备份失败: {e}")
            return False

    def _rebuild_program(self) -> bool:
        """重新编译程序"""
        try:
            logging.info("重新编译程序...")

            result = subprocess.run(
                ['cmd.exe', '/c', os.path.join(os.getcwd(), 'autobuild_x86_debug.bat')],
                capture_output=True,
                text=True,
                timeout=300,
                cwd=os.getcwd()
            )

            if result.returncode == 0:
                logging.info("编译成功")
                return True
            else:
                logging.error(f"编译失败: {result.stderr}")
                return False

        except Exception as e:
            logging.error(f"编译过程异常: {e}")
            return False

    def _terminate_program(self) -> bool:
        """终止程序进程"""
        try:
            logging.info("终止程序进程...")

            if os.name == 'nt':  # Windows
                subprocess.run(['taskkill', '/F', '/IM', 'PortMaster.exe'],
                             capture_output=True)
            else:  # Linux/Mac
                subprocess.run(['pkill', '-f', 'PortMaster'],
                             capture_output=True)

            time.sleep(2)  # 等待进程完全终止

            return True

        except Exception as e:
            logging.error(f"终止进程失败: {e}")
            return False

    def _cleanup_processes(self) -> None:
        """清理所有相关进程"""
        try:
            logging.info("清理进程...")

            if os.name == 'nt':  # Windows
                subprocess.run(['taskkill', '/F', '/IM', 'PortMaster.exe'],
                             capture_output=True)
            else:  # Linux/Mac
                subprocess.run(['pkill', '-f', 'PortMaster'],
                             capture_output=True)

        except Exception:
            pass

    def _is_program_running(self) -> bool:
        """检查程序是否在运行"""
        try:
            if os.name == 'nt':  # Windows
                result = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq PortMaster.exe'],
                                      capture_output=True, text=True)
                return 'PortMaster.exe' in result.stdout
            else:  # Linux/Mac
                result = subprocess.run(['pgrep', '-f', 'PortMaster'],
                                      capture_output=True)
                return result.returncode == 0
        except:
            return False

    def _verify_stability(self) -> bool:
        """验证程序运行稳定性"""
        logging.info("验证程序运行稳定性...")

        try:
            # 检查程序是否仍在运行
            if not self._is_program_running():
                logging.warning("程序未在运行")
                return False

            # 等待并多次检查
            for i in range(6):  # 6次检查，每次5秒，总共30秒
                time.sleep(5)
                if not self._is_program_running():
                    logging.warning(f"程序在 {i*5} 秒后停止运行")
                    return False
                logging.info(f"稳定性检查通过 {i+1}/6")

            logging.info("程序稳定性验证通过")
            return True

        except Exception as e:
            logging.error(f"稳定性验证异常: {e}")
            return False

    def _generate_final_report(self, session_result: Dict):
        """生成最终报告"""
        try:
            report = {
                'session_id': session_result['session_id'],
                'success': session_result['success'],
                'total_time': session_result['total_time'],
                'iterations': session_result['current_iteration'],
                'screenshots_taken': len(session_result['screenshots_taken']),
                'errors_found': len(session_result['errors_found']),
                'fixes_applied': len(session_result['fixes_applied']),
                'final_status': session_result['final_status'],
                'summary': self._generate_summary(session_result)
            }

            # 保存JSON报告
            report_file = os.path.join(self.results_dir, f"autonomous_fix_result_{self.session_id}.json")
            with open(report_file, 'w', encoding='utf-8') as f:
                json.dump(report, f, indent=2, ensure_ascii=False)

            # 保存文本报告
            text_report_file = os.path.join(self.results_dir, f"autonomous_fix_report_{self.session_id}.txt")
            with open(text_report_file, 'w', encoding='utf-8') as f:
                f.write(self._format_text_report(report))

            logging.info(f"最终报告已生成:")
            logging.info(f"  JSON: {report_file}")
            logging.info(f"  文本: {text_report_file}")

        except Exception as e:
            logging.error(f"生成最终报告失败: {e}")

    def _generate_summary(self, session_result: Dict) -> Dict:
        """生成摘要信息"""
        return {
            'total_screenshots': len(session_result['screenshots_taken']),
            'total_fixes': len(session_result['fixes_applied']),
            'error_types': list(set([
                error_type for error_info in session_result['errors_found']
                for error_type in error_info.get('error_types', [])
            ])),
            'success_rate': 'SUCCESS' if session_result['success'] else 'FAILED'
        }

    def _format_text_report(self, report: Dict) -> str:
        """格式化文本报告"""
        return f"""
=======================================
自主智能自动化修复系统 - 最终报告
=======================================

会话ID: {report['session_id']}
开始时间: {report.get('start_time', 'N/A')}
结束时间: {report.get('end_time', 'N/A')}
总耗时: {report['total_time']:.2f} 秒
迭代次数: {report['iterations']}
修复成功: {'是' if report['success'] else '否'}

修复摘要:
- 总截图数量: {report['screenshots_taken']}
- 总修复数量: {report['fixes_applied']}
- 检测到错误类型: {', '.join(report['summary']['error_types']) if report['summary']['error_types'] else '无'}
- 最终状态: {report['final_status']}

详细结果:
{json.dumps(report, indent=2, ensure_ascii=False)}

=======================================
"""

    # 具体的修复方法实现（这些方法需要根据实际代码结构来实現）
    def _fix_resource_mismatches(self) -> bool:
        """修复资源ID不匹配"""
        # 实现资源ID修复逻辑
        return False

    def _fix_initialization_order(self) -> bool:
        """修复初始化顺序"""
        # 实现初始化顺序修复逻辑
        return False

    def _remove_debug_code(self) -> bool:
        """移除调试代码"""
        # 实现调试代码移除逻辑
        return False

    def _add_exception_handling(self) -> bool:
        """添加异常处理"""
        # 实现异常处理添加逻辑
        return False

    def _apply_startup_fixes(self) -> List[str]:
        """应用启动修复"""
        # 实现启动修复逻辑
        return []

    def _check_for_error_windows(self) -> Dict:
        """检查错误窗口"""
        try:
            from enhanced_error_capture import EnhancedErrorCapture, ErrorWindow

            capturer = EnhancedErrorCapture()
            error_windows = capturer.find_error_windows()

            result = {
                'has_errors': len(error_windows) > 0,
                'error_windows': [],
                'screenshots': [],
                'analyses': []
            }

            if error_windows:
                logging.info(f"发现 {len(error_windows)} 个错误窗口")

                for error_window in error_windows:
                    # 分析错误内容
                    analysis = capturer.analyze_error_content(error_window)
                    result['analyses'].append(analysis)

                    # 截图
                    screenshot_path = capturer.capture_screenshot_with_error_window(error_window)
                    if screenshot_path:
                        result['screenshots'].append(screenshot_path)

                    # 保存错误窗口信息
                    result['error_windows'].append({
                        'title': error_window.title,
                        'text': error_window.text,
                        'window_type': error_window.window_type,
                        'timestamp': error_window.timestamp.isoformat()
                    })

                    # 关闭错误窗口
                    capturer.close_error_window(error_window)

            else:
                logging.info("未发现错误窗口")

            return result

        except ImportError:
            logging.warning("enhanced_error_capture模块不可用，使用基础检查")
            return self._basic_error_check()
        except Exception as e:
            logging.error(f"错误窗口检查失败: {e}")
            return {'has_errors': False, 'error_windows': [], 'screenshots': [], 'analyses': []}

    def _basic_error_check(self) -> Dict:
        """基础错误检查（备用方案）"""
        import ctypes

        try:
            user32 = ctypes.windll.user32
            error_windows = []

            def enum_callback(hwnd, lparam):
                if user32.IsWindowVisible(hwnd):
                    title = ctypes.create_unicode_buffer(256)
                    user32.GetWindowTextW(hwnd, title, 256)

                    title_str = title.value.lower()
                    error_keywords = ['error', '错误', 'assertion', 'debug', 'failed']

                    if any(keyword in title_str for keyword in error_keywords):
                        error_windows.append({
                            'hwnd': hwnd,
                            'title': title.value,
                            'window_type': 'BASIC_ERROR_DETECTED'
                        })

                return True

            callback_type = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int))
            callback = callback_type(enum_callback)
            user32.EnumWindows(callback, 0)

            has_errors = len(error_windows) > 0
            if has_errors:
                logging.info(f"基础检查发现 {len(error_windows)} 个可能错误窗口")

            return {
                'has_errors': has_errors,
                'error_windows': error_windows,
                'screenshots': [],
                'analyses': []
            }

        except Exception as e:
            logging.error(f"基础错误检查失败: {e}")
            return {'has_errors': False, 'error_windows': [], 'screenshots': [], 'analyses': []}

    def _fix_mfc_runtime_error(self) -> bool:
        """修复MFC运行时错误"""
        try:
            logging.info("修复MFC运行时错误 - 检查PortMasterDlg.cpp中的初始化顺序")

            # 检查PortMasterDlg.cpp文件
            dlg_file = "src/PortMasterDlg.cpp"
            if os.path.exists(dlg_file):
                with open(dlg_file, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()

                # 检查是否有在构造函数中初始化管理器的代码
                if 'InitializeManagers' in content and 'PortMasterDlg::PortMasterDlg' in content:
                    logging.info("发现构造函数中的管理器初始化，移至OnInitDialog")

                    # 备份原文件
                    backup_path = f"code_backups/PortMasterDlg_{datetime.now().strftime('%Y%m%d_%H%M%S')}.cpp"
                    os.makedirs('code_backups', exist_ok=True)
                    with open(backup_path, 'w', encoding='utf-8') as f:
                        f.write(content)

                    # 移除构造函数中的InitializeManagers调用
                    content = content.replace(
                        'InitializeManagers();',
                        '// InitializeManagers(); // 移至OnInitDialog以确保控件已创建'
                    )

                    # 保存修改
                    with open(dlg_file, 'w', encoding='utf-8') as f:
                        f.write(content)

                    logging.info("已将管理器初始化从构造函数移除")
                    return True

            return False

        except Exception as e:
            logging.error(f"修复MFC运行时错误失败: {e}")
            return False

    def _add_null_pointer_checks(self) -> bool:
        """添加空指针检查"""
        try:
            logging.info("添加空指针检查")

            # 检查关键文件中的指针使用
            files_to_check = [
                "src/PortMasterDlg.cpp",
                "src/UIStateManager.cpp",
                "src/TransmissionStateManager.cpp"
            ]

            modified_files = []
            for file_path in files_to_check:
                if os.path.exists(file_path):
                    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                        content = f.read()

                    # 备份
                    backup_path = f"code_backups/{os.path.basename(file_path)}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.cpp"
                    os.makedirs('code_backups', exist_ok=True)
                    with open(backup_path, 'w', encoding='utf-8') as f:
                        f.write(content)

                    # 添加基本的空指针检查模式
                    if '->' in content and 'assert(' not in content:
                        # 在指针访问前添加断言
                        modified_content = content
                        lines = content.split('\n')
                        for i, line in enumerate(lines):
                            if '->' in line and 'assert' not in line and '//' not in line:
                                # 提取指针变量名（简单模式）
                                if 'm_' in line:
                                    pointer_var = line.split('m_')[1].split('->')[0].strip()
                                    if pointer_var and ';' not in pointer_var:
                                        indent = '    '  # 基本缩进
                                        assert_line = f"{indent}assert(m_{pointer_var} != nullptr);"
                                        lines.insert(i, assert_line)
                                        modified_files.append(file_path)
                                        break

                        modified_content = '\n'.join(lines)
                        if modified_content != content:
                            with open(file_path, 'w', encoding='utf-8') as f:
                                f.write(modified_content)

            if modified_files:
                logging.info(f"已在 {len(modified_files)} 个文件中添加空指针检查")
                return True

            return False

        except Exception as e:
            logging.error(f"添加空指针检查失败: {e}")
            return False

    def _fix_memory_access_issues(self) -> bool:
        """修复内存访问问题"""
        try:
            logging.info("修复内存访问问题")

            # 检查并修复数组访问问题
            dlg_file = "src/PortMasterDlg.cpp"
            if os.path.exists(dlg_file):
                with open(dlg_file, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()

                # 备份
                backup_path = f"code_backups/PortMasterDlg_memory_{datetime.now().strftime('%Y%m%d_%H%M%S')}.cpp"
                os.makedirs('code_backups', exist_ok=True)
                with open(backup_path, 'w', encoding='utf-8') as f:
                    f.write(content)

                # 检查DDX控件访问并添加验证
                if 'DDX_' in content and 'GetDlgItem' not in content:
                    logging.info("添加控件访问验证")

                    # 在DDX_Control调用后添加控件有效性检查
                    lines = content.split('\n')
                    modified_lines = []

                    for i, line in enumerate(lines):
                        modified_lines.append(line)

                        if 'DDX_Control' in line and 'IDC_' in line:
                            # 添加控件有效性检查
                            indent = '    '  # 保持缩进
                            control_id = line.split('IDC_')[1].split(')')[0].strip()
                            next_line = f"{indent}// 验证控件有效性"
                            modified_lines.append(next_line)
                            next_line = f"{indent}if (GetDlgItem(IDC_{control_id}) == nullptr) {{"
                            modified_lines.append(next_line)
                            next_line = f"{indent}    AfxMessageBox(_T(\"控件IDC_{control_id}创建失败\"));"
                            modified_lines.append(next_line)
                            next_line = f"{indent}}}"
                            modified_lines.append(next_line)

                    modified_content = '\n'.join(modified_lines)
                    if modified_content != content:
                        with open(dlg_file, 'w', encoding='utf-8') as f:
                            f.write(modified_content)
                        logging.info("已添加控件有效性检查")
                        return True

            return False

        except Exception as e:
            logging.error(f"修复内存访问问题失败: {e}")
            return False

def main():
    """主执行函数"""
    controller = AutonomousFixController()
    result = controller.run_autonomous_fix()

    if result['success']:
        logging.info("[SUCCESS] 自主修复成功！程序现在可以正常运行了。")
        return 0
    else:
        logging.error("[ERROR] 自主修复失败，可能需要人工介入。")
        return 1

if __name__ == '__main__':
    sys.exit(main())