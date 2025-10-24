#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PortMaster综合测试运行器
执行完整的自动化测试套件，包括单元测试、集成测试、性能测试等
"""

import os
import sys
import json
import time
import subprocess
import logging
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional

# Windows编码设置
if sys.platform == 'win32':
    subprocess.run(['chcp', '65001'], shell=True, capture_output=True)

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('comprehensive_test.log', encoding='utf-8'),
        logging.StreamHandler()
    ]
)

class ComprehensiveTestRunner:
    """综合测试运行器"""

    def __init__(self):
        """初始化测试运行器"""
        self.start_time = datetime.now()
        self.test_results = []
        self.current_phase = ""
        self.report_dir = Path("test_reports")
        self.report_dir.mkdir(exist_ok=True)

    def log_phase(self, phase_name: str):
        """记录测试阶段"""
        self.current_phase = phase_name
        logging.info(f"=== {phase_name} ===")

    def run_build_verification(self) -> Dict:
        """运行构建验证"""
        self.log_phase("构建验证")

        try:
            result = subprocess.run(
                ["autobuild_x86_debug.bat"],
                capture_output=True,
                text=True,
                timeout=300
            )

            success = result.returncode == 0

            # 检查生成的可执行文件
            exe_exists = os.path.exists("build/Debug/PortMaster.exe")

            build_result = {
                "phase": "构建验证",
                "success": success and exe_exists,
                "duration": time.time(),
                "details": {
                    "build_returncode": result.returncode,
                    "exe_exists": exe_exists,
                    "stdout_lines": len(result.stdout.split('\n')) if result.stdout else 0
                }
            }

            if build_result["success"]:
                logging.info("✓ 构建验证通过")
            else:
                logging.error("✗ 构建验证失败")

            self.test_results.append(build_result)
            return build_result

        except subprocess.TimeoutExpired:
            error_result = {
                "phase": "构建验证",
                "success": False,
                "duration": 300,
                "error": "构建超时"
            }
            logging.error("✗ 构建验证超时")
            self.test_results.append(error_result)
            return error_result

    def run_unit_tests(self) -> Dict:
        """运行单元测试"""
        self.log_phase("单元测试")

        try:
            # 使用修复后的自动化系统
            from autotest_integration import AutoTestIntegration

            autotest = AutoTestIntegration()
            if not autotest.verify_autotest_ready():
                raise Exception("AutoTest未就绪")

            # 运行单元测试
            test_result = autotest.run_unit_tests("unit_tests_report.json")

            unit_test_result = {
                "phase": "单元测试",
                "success": test_result.get("success", False),
                "duration": time.time(),
                "details": {
                    "exit_code": test_result.get("exit_code", -1),
                    "report_path": test_result.get("report_path"),
                    "analysis": test_result.get("analysis", {})
                }
            }

            if unit_test_result["success"]:
                analysis = test_result.get("analysis", {})
                logging.info(f"✓ 单元测试通过 ({analysis.get('passed_tests', 0)}/{analysis.get('total_tests', 0)})")
            else:
                logging.error("✗ 单元测试失败")

            self.test_results.append(unit_test_result)
            return unit_test_result

        except Exception as e:
            error_result = {
                "phase": "单元测试",
                "success": False,
                "duration": time.time(),
                "error": str(e)
            }
            logging.error(f"✗ 单元测试异常: {e}")
            self.test_results.append(error_result)
            return error_result

    def run_integration_tests(self) -> Dict:
        """运行集成测试"""
        self.log_phase("集成测试")

        try:
            from autotest_integration import AutoTestIntegration

            autotest = AutoTestIntegration()
            test_result = autotest.run_integration_tests("integration_tests_report.json")

            integration_result = {
                "phase": "集成测试",
                "success": test_result.get("success", False),
                "duration": time.time(),
                "details": {
                    "exit_code": test_result.get("exit_code", -1),
                    "report_path": test_result.get("report_path"),
                    "analysis": test_result.get("analysis", {})
                }
            }

            if integration_result["success"]:
                analysis = test_result.get("analysis", {})
                logging.info(f"✓ 集成测试通过 ({analysis.get('passed_tests', 0)}/{analysis.get('total_tests', 0)})")
            else:
                logging.error("✗ 集成测试失败")

            self.test_results.append(integration_result)
            return integration_result

        except Exception as e:
            error_result = {
                "phase": "集成测试",
                "success": False,
                "duration": time.time(),
                "error": str(e)
            }
            logging.error(f"✗ 集成测试异常: {e}")
            self.test_results.append(error_result)
            return error_result

    def run_performance_tests(self) -> Dict:
        """运行性能测试"""
        self.log_phase("性能测试")

        try:
            from autotest_integration import AutoTestIntegration

            autotest = AutoTestIntegration()
            test_result = autotest.run_performance_tests("performance_tests_report.json")

            performance_result = {
                "phase": "性能测试",
                "success": test_result.get("success", False),
                "duration": time.time(),
                "details": {
                    "exit_code": test_result.get("exit_code", -1),
                    "report_path": test_result.get("report_path"),
                    "analysis": test_result.get("analysis", {})
                }
            }

            if performance_result["success"]:
                analysis = test_result.get("analysis", {})
                logging.info(f"✓ 性能测试通过 ({analysis.get('passed_tests', 0)}/{analysis.get('total_tests', 0)})")
            else:
                logging.error("✗ 性能测试失败")

            self.test_results.append(performance_result)
            return performance_result

        except Exception as e:
            error_result = {
                "phase": "性能测试",
                "success": False,
                "duration": time.time(),
                "error": str(e)
            }
            logging.error(f"✗ 性能测试异常: {e}")
            self.test_results.append(error_result)
            return error_result

    def run_functionality_verification(self) -> Dict:
        """运行功能验证"""
        self.log_phase("功能验证")

        try:
            # 运行功能验证脚本
            result = subprocess.run(
                ["python", "test_portmaster_functionality.py"],
                capture_output=True,
                text=True,
                timeout=120
            )

            success = result.returncode == 0

            # 读取功能验证报告
            func_report = {}
            if os.path.exists("portmaster_functionality_report.json"):
                with open("portmaster_functionality_report.json", "r", encoding="utf-8") as f:
                    func_report = json.load(f)

            functionality_result = {
                "phase": "功能验证",
                "success": success,
                "duration": time.time(),
                "details": {
                    "returncode": result.returncode,
                    "functionality_report": func_report
                }
            }

            if functionality_result["success"]:
                success_rate = func_report.get("success_rate", 0)
                logging.info(f"✓ 功能验证通过 (成功率: {success_rate}%)")
            else:
                logging.error("✗ 功能验证失败")

            self.test_results.append(functionality_result)
            return functionality_result

        except Exception as e:
            error_result = {
                "phase": "功能验证",
                "success": False,
                "duration": time.time(),
                "error": str(e)
            }
            logging.error(f"✗ 功能验证异常: {e}")
            self.test_results.append(error_result)
            return error_result

    def generate_comprehensive_report(self) -> Dict:
        """生成综合测试报告"""
        end_time = datetime.now()
        total_duration = end_time - self.start_time

        # 统计结果
        total_tests = len(self.test_results)
        passed_tests = sum(1 for r in self.test_results if r.get("success", False))
        failed_tests = total_tests - passed_tests
        success_rate = (passed_tests / total_tests * 100) if total_tests > 0 else 0

        report = {
            "test_summary": {
                "test_start": self.start_time.strftime("%Y-%m-%d %H:%M:%S"),
                "test_end": end_time.strftime("%Y-%m-%d %H:%M:%S"),
                "total_duration": str(total_duration),
                "total_phases": total_tests,
                "passed_phases": passed_tests,
                "failed_phases": failed_tests,
                "success_rate": success_rate
            },
            "test_results": self.test_results,
            "recommendations": []
        }

        # 生成建议
        if success_rate >= 90:
            report["recommendations"].append("系统测试通过率良好，可以进入生产环境")
        if success_rate < 100:
            report["recommendations"].append("存在失败的测试阶段，需要检查和修复")
        if failed_tests > 0:
            report["recommendations"].append("建议修复失败的问题后重新运行测试")

        return report

    def save_report(self, report: Dict):
        """保存测试报告"""
        # 保存JSON格式报告
        json_file = self.report_dir / f"comprehensive_test_report_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
        with open(json_file, "w", encoding="utf-8") as f:
            json.dump(report, f, ensure_ascii=False, indent=2)

        # 生成Markdown格式报告
        md_file = self.report_dir / f"comprehensive_test_report_{datetime.now().strftime('%Y%m%d_%H%M%S')}.md"
        self.generate_markdown_report(report, md_file)

        logging.info(f"测试报告已保存:")
        logging.info(f"  JSON: {json_file}")
        logging.info(f"  Markdown: {md_file}")

    def generate_markdown_report(self, report: Dict, output_file: Path):
        """生成Markdown格式报告"""
        md_content = f"""# PortMaster 综合测试报告

## 测试概述

- **测试开始时间**: {report['test_summary']['test_start']}
- **测试结束时间**: {report['test_summary']['test_end']}
- **总测试时长**: {report['test_summary']['total_duration']}
- **测试阶段数**: {report['test_summary']['total_phases']}
- **通过阶段数**: {report['test_summary']['passed_phases']}
- **失败阶段数**: {report['test_summary']['failed_phases']}
- **总体通过率**: {report['test_summary']['success_rate']:.1f}%

## 测试结果详情

"""

        for result in report['test_results']:
            status = "✅ 通过" if result.get('success', False) else "❌ 失败"
            md_content += f"### {result['phase']}\n\n"
            md_content += f"- **状态**: {status}\n"
            md_content += f"- **执行时间**: {result['duration']:.2f}秒\n"

            if 'error' in result:
                md_content += f"- **错误信息**: {result['error']}\n"

            if 'details' in result:
                details = result['details']
                if 'analysis' in details and details['analysis']:
                    analysis = details['analysis']
                    md_content += f"- **测试统计**: 总计{analysis.get('total_tests', 0)}个, 通过{analysis.get('passed_tests', 0)}个\n"
                    if analysis.get('failed_tests', 0) > 0:
                        md_content += f"- **失败详情**: {analysis.get('failed_tests', 0)}个测试失败\n"

            md_content += "\n"

        md_content += """## 建议和结论

"""

        for recommendation in report['recommendations']:
            md_content += f"- {recommendation}\n"

        md_content += f"""

---

*报告生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}*
"""

        with open(output_file, "w", encoding="utf-8") as f:
            f.write(md_content)

    def run_comprehensive_tests(self) -> Dict:
        """运行综合测试"""
        logging.info("开始PortMaster综合测试")
        logging.info("=" * 60)

        test_phases = [
            ("构建验证", self.run_build_verification),
            ("功能验证", self.run_functionality_verification),
            ("单元测试", self.run_unit_tests),
            ("集成测试", self.run_integration_tests),
            ("性能测试", self.run_performance_tests)
        ]

        for phase_name, phase_func in test_phases:
            try:
                phase_func()
            except Exception as e:
                logging.error(f"测试阶段 {phase_name} 发生异常: {e}")
                # 继续执行后续测试

        # 生成综合报告
        report = self.generate_comprehensive_report()
        self.save_report(report)

        logging.info("=" * 60)
        logging.info("综合测试完成")

        summary = report['test_summary']
        logging.info(f"总通过率: {summary['success_rate']:.1f}%")
        logging.info(f"通过阶段: {summary['passed_phases']}/{summary['total_phases']}")

        if summary['success_rate'] >= 90:
            logging.info("🎉 测试结果良好！")
        else:
            logging.warning("⚠️  存在失败的测试，需要检查和修复")

        return report

if __name__ == "__main__":
    runner = ComprehensiveTestRunner()
    report = runner.run_comprehensive_tests()

    # 设置退出码
    success_rate = report['test_summary']['success_rate']
    sys.exit(0 if success_rate >= 90 else 1)